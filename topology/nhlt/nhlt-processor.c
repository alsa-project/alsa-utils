// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>
#include "pre-process-external.h"
#include "nhlt.h"
#include "intel/intel-nhlt.h"
#include "intel/dmic-nhlt.h"
#include "intel/ssp-nhlt.h"

#define MAX_ENDPOINT_COUNT 20
#define ALSA_BYTE_CHARS 5
#define SOF_ABI_CHARS 29
#define SOF_MANIFEST_DATA_TYPE_NHLT 1

struct sof_manifest_tlv {
	uint32_t type;
	uint32_t size;
	uint8_t data[];
} __attribute__((packed));

struct sof_manifest {
	uint16_t abi_major;
	uint16_t abi_minor;
	uint16_t abi_patch;
	uint16_t count;
	struct sof_manifest_tlv items[];
} __attribute__((packed));

#ifdef NHLT_DEBUG
static void debug_print_nhlt(struct nhlt *blob, struct endpoint_descriptor **eps)
{
	uint8_t *top_p = (uint8_t *)blob;
	struct endpoint_descriptor *ep;
	uint8_t *ep_p;
	int i, j, k, lines, remain;

	fprintf(stdout, "printing nhlt as bytes:\n");

	lines = sizeof(struct nhlt) / 8;
	remain = sizeof(struct nhlt) % 8;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < 8; j++) {
			fprintf(stdout, "0x%02x,", *top_p);
			top_p++;
		}
		fprintf(stdout, "\n");
	}
	for (i = 0; i < remain; i++) {
		fprintf(stdout, "0x%02x,", *top_p);
		top_p++;
	}
	fprintf(stdout, "\n\n");

	for (i = 0; i < blob->endpoint_count; i++) {
		ep = eps[i];
		ep_p = (uint8_t *)ep;
		lines = ep->length / 8;
		remain = ep->length % 8;
		for (j = 0; j < lines; j++) {
			for (k = 0; k < 8; k++) {
				fprintf(stdout, "0x%02x,", *ep_p);
				ep_p++;
			}
			fprintf(stdout, "\n");
		}
		for (j = 0; j < remain; j++) {
			fprintf(stdout, "0x%02x,", *ep_p);
			ep_p++;
		}
		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\n");
}
#else
static void debug_print_nhlt(struct nhlt *blob, struct endpoint_descriptor **eps) {}
#endif

static int print_as_hex_bytes(uint8_t *manifest_buffer, uint32_t manifest_size,
			      uint8_t *nhlt_buffer, uint32_t nhlt_size, char **src)
{
	char *bytes_string_buffer;
	char *dst;
	int i;

	bytes_string_buffer = calloc((manifest_size + nhlt_size) * ALSA_BYTE_CHARS + 1,
				     sizeof(uint8_t));
	if (!bytes_string_buffer)
		return -ENOMEM;

	dst = bytes_string_buffer;
	for (i = 0; i < manifest_size; i++) {
		snprintf(dst, ALSA_BYTE_CHARS + 1, "0x%02x,", *manifest_buffer);
		dst += ALSA_BYTE_CHARS;
		manifest_buffer++;
	}

	for (i = 0; i < nhlt_size; i++) {
		snprintf(dst, ALSA_BYTE_CHARS + 1, "0x%02x,", *nhlt_buffer);
		dst += ALSA_BYTE_CHARS;
		nhlt_buffer++;
	}

	/* remove the last comma... */
	dst--;
	*dst = '\0';

	*src = bytes_string_buffer;

	return 0;
}

static int merge_manifest_data(snd_config_t *cfg, uint8_t *manifest_buffer, uint32_t manifest_size,
			       uint8_t *nhlt_buffer, uint32_t nhlt_size)
{
	const char *data_name = "SOF ABI";
	snd_config_t *data_section;
	snd_config_t *manifest;
	snd_config_t *old_bytes;
	snd_config_t *new_bytes;
	char *src = NULL;
	int ret;

	/* merge manifest struct and nhlt bytes as new config into existing SectionData*/
	ret = snd_config_search(cfg, "SectionData", &data_section);
	if (ret < 0)
		return ret;

	ret = snd_config_search(data_section, data_name, &manifest);
	if (ret < 0)
		return ret;

	ret = snd_config_search(manifest, "bytes", &old_bytes);
	if (ret < 0)
		return ret;

	ret = snd_config_make(&new_bytes, "bytes", SND_CONFIG_TYPE_STRING);
	if (ret < 0)
		goto err;

	ret = print_as_hex_bytes(manifest_buffer, manifest_size, nhlt_buffer, nhlt_size, &src);
	if (ret < 0)
		goto err;

	ret = snd_config_set_string(new_bytes, src);
	if (ret < 0)
		goto err;

	ret = snd_config_merge(old_bytes, new_bytes, true);
	if (ret < 0)
		goto err;

	free(src);

	return 0;
err:
	if (new_bytes)
		snd_config_delete(new_bytes);
	if (src)
		free(src);

	return ret;
}

static void save_nhlt_binary(struct nhlt *blob, struct endpoint_descriptor **eps, snd_config_t *cfg)
{
	const char *bin_file = NULL;
	snd_config_t *defines;
	FILE *fp;
	int ret;
	int i;

	ret = snd_config_search(cfg, "Define.NHLT_BIN", &defines);
	if (ret < 0)
		return;

	if (snd_config_get_string(defines, &bin_file) < 0)
		return;

	fp = fopen(bin_file, "wb");
	if (fp == NULL) {
		fprintf(stderr, "can't open nhlt binary output file %s\n", bin_file);
		return;
	}

	fprintf(stdout, "saving nhlt as binary in %s\n", bin_file);

	fwrite(blob, 1, sizeof(struct nhlt), fp);

	for (i = 0; i < blob->endpoint_count; i++)
		fwrite(eps[i], eps[i]->length, sizeof(uint8_t), fp);

	fclose(fp);
}

static int manifest_create(snd_config_t *input, uint8_t **manifest_buffer, uint32_t *size, uint32_t nhlt_size)
{
	struct sof_manifest_tlv manifest_tlv;
	struct sof_manifest manifest;
	snd_config_t *data_section;
	snd_config_t *data;
	snd_config_t *old_bytes;
	uint32_t manifest_size;
	uint8_t *byte_buffer;
	const char *abi;
	uint8_t *top_p;
	uint8_t *dst;
	int ret;
	int i;

	ret = snd_config_search(input, "SectionData", &data_section);
	if (ret < 0)
		return ret;

	ret = snd_config_search(data_section, "SOF ABI", &data);
	if (ret < 0)
		return ret;

	ret = snd_config_search(data, "bytes", &old_bytes);
	if (ret < 0)
		return ret;

	ret = snd_config_get_string(old_bytes, &abi);
	if (ret < 0)
		return ret;

	/* we have something funny in abi string */
	if (strlen(abi) != SOF_ABI_CHARS)
		return -EINVAL;

	manifest.count = 1;
	manifest_tlv.type = SOF_MANIFEST_DATA_TYPE_NHLT;
	manifest_tlv.size = nhlt_size;

	manifest_size = sizeof(struct sof_manifest) + sizeof(struct sof_manifest_tlv);
	byte_buffer = calloc(manifest_size, sizeof(uint8_t));
	if (!byte_buffer)
		return -ENOMEM;

	*size = manifest_size;

	dst = byte_buffer;

	/* copy the ABI version bytes */
	for (i = 0; i < 6; i++)
		sscanf(&abi[i * ALSA_BYTE_CHARS], "%" SCNx8, dst++);

	/* set the count */
	*dst++ = manifest.count;
	*dst++ = manifest.count >> 2;

	top_p = (uint8_t *)&manifest_tlv;
	for (i = 0; i < sizeof(struct sof_manifest_tlv); i++)
		*dst++ = *top_p++;

	*manifest_buffer = byte_buffer;

	return 0;
}

static int nhlt_get_flat_buffer(struct nhlt *blob, struct endpoint_descriptor **eps,
				uint32_t eps_count, uint32_t *size, uint8_t **nhlt_buffer)
{
	uint8_t *top_p = (uint8_t *)blob;
	struct endpoint_descriptor *ep;
	uint8_t *byte_buffer;
	uint32_t nhlt_size;
	uint8_t *ep_p;
	uint8_t *dst;
	int i, j;

	/* get blob total size */
	nhlt_size = sizeof(struct nhlt);
	for (i = 0; i < eps_count; i++) {
		if (eps[i])
			nhlt_size += eps[i]->length;
	}

	*size = nhlt_size;

	byte_buffer = calloc(nhlt_size, sizeof(uint8_t));
	if (!byte_buffer)
		return -ENOMEM;

	dst = byte_buffer;
	for (i = 0; i < sizeof(struct nhlt); i++)
		*dst++ = *top_p++;

	for (i = 0; i < blob->endpoint_count; i++) {
		ep = eps[i];
		ep_p = (uint8_t *)ep;
		for (j = 0; j < ep->length; j++)
			*dst++ = *ep_p++;
	}

	*nhlt_buffer = byte_buffer;

	return 0;
}

/* called at the end of topology pre-processing, create flat buffer from variable size nhlt */
static int nhlt_create(struct intel_nhlt_params *nhlt, snd_config_t *input, snd_config_t *output,
		       uint8_t **nhlt_buffer, uint32_t *nhlt_size)
{
	struct endpoint_descriptor *eps[MAX_ENDPOINT_COUNT];
	int eps_count = 0;
	struct nhlt blob;
	uint32_t size;
	uint8_t dir;
	int ret;
	int i;

	for (i = 0; i < MAX_ENDPOINT_COUNT; i++)
		eps[i] = NULL;

	/* we always have only 0 or 1 dmic ep */
	for (i = 0; i < nhlt_dmic_get_ep_count(nhlt); i++) {
		ret = nhlt_dmic_get_ep(nhlt, &eps[eps_count], i);
		if (ret < 0)
			return -EINVAL;
		eps_count++;
	}

	/* we can have 0 to several ssp eps */
	for (i = 0; i < nhlt_ssp_get_ep_count(nhlt); i++) {
		nhlt_ssp_get_dir(nhlt, i, &dir);
		/* duplicate endpoint for duplex dai */
		if (dir > NHLT_ENDPOINT_DIRECTION_FEEDBACK_FOR_RENDER) {
			ret = nhlt_ssp_get_ep(nhlt, &eps[eps_count], i,
					      NHLT_ENDPOINT_DIRECTION_RENDER);
			if (ret < 0)
				goto err;
			eps_count++;
			ret = nhlt_ssp_get_ep(nhlt, &eps[eps_count], i,
					      NHLT_ENDPOINT_DIRECTION_CAPTURE);
		} else {
			ret = nhlt_ssp_get_ep(nhlt, &eps[eps_count], i, dir);
		}
		if (ret < 0)
			goto err;
		eps_count++;
	}

	/* we don't have endpoints */
	if (!eps_count)
		return 0;

	uint8_t sig[4] = {'N', 'H', 'L', 'T'};
	blob.efi_acpi.signature = *((uint32_t *)sig);
	blob.efi_acpi.length = 0;
	blob.efi_acpi.revision = 0;
	blob.efi_acpi.checksum = 0;
	for (i = 0; i < 6; i++)
		blob.efi_acpi.oem_id[i] = 0;
	blob.efi_acpi.oem_table_id = 0;
	blob.efi_acpi.oem_revision = 0;
	blob.efi_acpi.creator_id = 0;
	blob.efi_acpi.creator_revision = 0;

	blob.endpoint_count = eps_count;

	/* get blob total size */
	size = sizeof(struct nhlt);
	for (i = 0; i < eps_count; i++) {
		if (eps[i])
			size += eps[i]->length;
	}

	/* add the total length to top level struct */
	blob.efi_acpi.length = size;

	debug_print_nhlt(&blob, eps);

	save_nhlt_binary(&blob, eps, input);

	ret = nhlt_get_flat_buffer(&blob, eps, eps_count, nhlt_size, nhlt_buffer);

err:
	/* remove all enpoints */
	for (i = 0; i < eps_count; i++)
		free(eps[i]);

	return ret;
}

static int do_nhlt(struct intel_nhlt_params *nhlt, snd_config_t *input, snd_config_t *output)
{
	uint8_t *manifest_buffer = NULL;
	uint8_t *nhlt_buffer = NULL;
	uint32_t manifest_size;
	uint32_t nhlt_size = 0;
	int ret = 0;

	ret = nhlt_create(nhlt, input, output, &nhlt_buffer, &nhlt_size);
	if (ret) {
		fprintf(stderr, "can't create nhlt blob, err %d\n", ret);
		return ret;
	}

	ret = manifest_create(output, &manifest_buffer, &manifest_size, nhlt_size);
	if (ret) {
		fprintf(stderr, "can't re-create manifest, err %d\n", ret);
		goto err;
	}

	ret = merge_manifest_data(output, manifest_buffer, manifest_size, nhlt_buffer, nhlt_size);
	if (ret)
		fprintf(stderr, "can't merge manifest data, err %d\n", ret);

err:
	if (manifest_buffer)
		free(manifest_buffer);
	if (nhlt_buffer)
		free(nhlt_buffer);

	return ret;
}

SND_TOPOLOGY_PLUGIN_DEFINE_FUNC(nhlt)
{
	snd_config_iterator_t i, i2, next, next2;
	struct intel_nhlt_params nhlt;
	snd_config_t *n, *n2;
	snd_config_t *items;
	const char *id, *id2;
	int ret;

	/* initialize the internal structs */
	ret = nhlt_ssp_init_params(&nhlt);
	if (ret < 0)
		return ret;

	ret = nhlt_dmic_init_params(&nhlt);
	if (ret < 0)
		return ret;

	/* find DAIs and set internal parameters */
	ret = snd_config_search(input, "Object.Dai", &items);
	if (ret < 0)
		return ret;

	snd_config_for_each(i, next, items) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			continue;

		snd_config_for_each(i2, next2, n) {
			n2 = snd_config_iterator_entry(i2);

			if (snd_config_get_id(n2, &id2) < 0)
				continue;

			/* set dai parameters here */
			if (!strncmp(id, "DMIC", 4)) {
				ret = nhlt_dmic_set_params(&nhlt, n2, input);
				if (ret < 0)
					return ret;
			}

			if (!strncmp(id, "SSP", 3)) {
				ret = nhlt_ssp_set_params(&nhlt, n2, input);
				if (ret < 0)
					return ret;
			}
		}
	}

	/* create the nhlt blob from internal structs */
	ret = do_nhlt(&nhlt, input, output);
	if (ret)
		fprintf(stderr, "error in nhlt processing\n");

	free(nhlt.ssp_params);
	free(nhlt.dmic_params);

	return 0;
}
