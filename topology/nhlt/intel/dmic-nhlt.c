// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>
#include "dmic-nhlt.h"
#include "dmic/dmic-process.h"

static int set_dmic_data(struct intel_nhlt_params *nhlt, snd_config_t *dai_cfg, snd_config_t *top)
{
	long unmute_ramp_time_ms = 0;
	long fifo_word_length = 0;
	long driver_version = 0;
	long num_pdm_active = 0;
	long sample_rate = 0;
	long dai_index_t = 0;
	long duty_min = 0;
	long duty_max = 0;
	long clk_min = 0;
	long clk_max = 0;
	long io_clk = 0;
	int ret;

	struct dai_values dmic_data[] = {
		{ "driver_version", SND_CONFIG_TYPE_INTEGER, NULL, &driver_version, NULL},
		{ "io_clk", SND_CONFIG_TYPE_INTEGER, NULL, &io_clk, NULL},
		{ "dai_index", SND_CONFIG_TYPE_INTEGER, NULL, &dai_index_t, NULL},
		{ "num_pdm_active", SND_CONFIG_TYPE_INTEGER, NULL, &num_pdm_active, NULL},
		{ "fifo_word_length", SND_CONFIG_TYPE_INTEGER, NULL, &fifo_word_length, NULL},
		{ "clk_min", SND_CONFIG_TYPE_INTEGER, NULL, &clk_min, NULL},
		{ "clk_max", SND_CONFIG_TYPE_INTEGER, NULL, &clk_max, NULL},
		{ "duty_min", SND_CONFIG_TYPE_INTEGER, NULL, &duty_min, NULL},
		{ "duty_max", SND_CONFIG_TYPE_INTEGER, NULL, &duty_max, NULL},
		{ "sample_rate", SND_CONFIG_TYPE_INTEGER, NULL, &sample_rate, NULL},
		{ "unmute_ramp_time_ms", SND_CONFIG_TYPE_INTEGER, NULL, &unmute_ramp_time_ms, NULL},
	};

	ret = find_set_values(&dmic_data[0], ARRAY_SIZE(dmic_data), dai_cfg, top, "Class.Dai.DMIC");
	if (ret < 0)
		return ret;

	return dmic_set_params(nhlt, dai_index_t, driver_version, io_clk, num_pdm_active,
			       fifo_word_length, clk_min, clk_max, duty_min, duty_max, sample_rate,
			       unmute_ramp_time_ms);
}

static int set_pdm_data(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top)
{
	long mic_a_enable = 0;
	long mic_b_enable = 0;
	long polarity_a = 0;
	long polarity_b = 0;
	long clk_edge = 0;
	long ctrl_id = 0;
	long skew = 0;
	int ret;

	struct dai_values dmic_pdm_data[] = {
		{ "mic_a_enable", SND_CONFIG_TYPE_INTEGER, NULL, &mic_a_enable, NULL},
		{ "mic_b_enable", SND_CONFIG_TYPE_INTEGER, NULL, &mic_b_enable, NULL},
		{ "polarity_a", SND_CONFIG_TYPE_INTEGER, NULL, &polarity_a, NULL},
		{ "polarity_b", SND_CONFIG_TYPE_INTEGER, NULL, &polarity_b, NULL},
		{ "clk_edge", SND_CONFIG_TYPE_INTEGER, NULL, &clk_edge, NULL},
		{ "ctrl_id", SND_CONFIG_TYPE_INTEGER, NULL, &ctrl_id, NULL},
		{ "skew", SND_CONFIG_TYPE_INTEGER, NULL, &skew, NULL},
	};

	ret = find_set_values(&dmic_pdm_data[0], ARRAY_SIZE(dmic_pdm_data), cfg, top,
			      "Class.Base.pdm_config");
	if (ret < 0)
		return ret;

	return dmic_set_pdm_params(nhlt, ctrl_id, mic_a_enable, mic_b_enable, polarity_a,
				   polarity_b, clk_edge, skew);
}

static int set_mic_data(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top)
{
	long sensitivity = 0;
	long snr = 0;
	int ret;

	struct dai_values dmic_mic_data[] = {
		{ "snr", SND_CONFIG_TYPE_INTEGER, NULL, &snr, NULL},
		{ "sensitivity", SND_CONFIG_TYPE_INTEGER, NULL, &snr, NULL},
	};

	ret = find_set_values(&dmic_mic_data[0], ARRAY_SIZE(dmic_mic_data), cfg, top,
			      "Class.Base.mic_extension");
	if (ret < 0)
		return ret;

	return dmic_set_ext_params(nhlt, snr, sensitivity);
}

static int set_vendor_mic_data(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top)
{
	long speaker_position_distance = 0;
	long horizontal_angle_begin = 0;
	long horizontal_angle_end = 0;
	long vertical_angle_begin = 0;
	long vertical_angle_end = 0;
	long frequency_high_band = 0;
	long frequency_low_band = 0;
	long horizontal_offset = 0;
	long vertical_offset = 0;
	long direction_angle = 0;
	long elevation_angle = 0;
	long mic_type = 0;
	long location = 0;
	long mic_id = 0;
	int ret;

	struct dai_values dmic_vendor_data[] = {
		{ "mic_id", SND_CONFIG_TYPE_INTEGER, NULL, &mic_id, NULL},
		{ "mic_type", SND_CONFIG_TYPE_INTEGER, NULL, &mic_type, NULL},
		{ "location", SND_CONFIG_TYPE_INTEGER, NULL, &location, NULL},
		{ "speaker_position_distance", SND_CONFIG_TYPE_INTEGER, NULL,
		  &speaker_position_distance, NULL},
		{ "horizontal_offset", SND_CONFIG_TYPE_INTEGER, NULL, &horizontal_offset, NULL},
		{ "vertical_offset", SND_CONFIG_TYPE_INTEGER, NULL, &vertical_offset, NULL},
		{ "frequency_low_band", SND_CONFIG_TYPE_INTEGER, NULL, &frequency_low_band, NULL},
		{ "frequency_high_band", SND_CONFIG_TYPE_INTEGER, NULL, &frequency_high_band, NULL},
		{ "direction_angle", SND_CONFIG_TYPE_INTEGER, NULL, &direction_angle, NULL},
		{ "elevation_angle", SND_CONFIG_TYPE_INTEGER, NULL, &elevation_angle, NULL},
		{ "vertical_angle_begin", SND_CONFIG_TYPE_INTEGER, NULL, &vertical_angle_begin,
		  NULL},
		{ "vertical_angle_end", SND_CONFIG_TYPE_INTEGER, NULL, &vertical_angle_end, NULL},
		{ "horizontal_angle_begin", SND_CONFIG_TYPE_INTEGER, NULL, &horizontal_angle_begin,
		  NULL},
		{ "horizontal_angle_end", SND_CONFIG_TYPE_INTEGER, NULL, &horizontal_angle_end,
		  NULL},
	};

	ret = find_set_values(&dmic_vendor_data[0], ARRAY_SIZE(dmic_vendor_data), cfg, top,
			      "Class.Base.vendor_mic_config");
	if (ret < 0)
		return ret;

	return dmic_set_mic_params(nhlt, mic_id, mic_type, location, speaker_position_distance,
				   horizontal_offset, vertical_offset, frequency_low_band,
				   frequency_high_band, direction_angle, elevation_angle,
				   vertical_angle_begin, vertical_angle_end, horizontal_angle_begin,
				   horizontal_angle_end);
}

static int set_bytes_data(struct intel_nhlt_params *nhlt, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *bytes;
	const char *id;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (strcmp(id, "fir_coeffs"))
		return 0;

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_string(n, &bytes))
			return -EINVAL;
	}

	return 0;
}

/* init dmic parameters, should be called before parsing dais */
int nhlt_dmic_init_params(struct intel_nhlt_params *nhlt)
{
	return dmic_init_params(nhlt);
}

/* get dmic endpoint count */
int nhlt_dmic_get_ep_count(struct intel_nhlt_params *nhlt)
{
	return dmic_get_vendor_blob_count(nhlt);
}

int nhlt_dmic_get_ep(struct intel_nhlt_params *nhlt, struct endpoint_descriptor **eps,
		     int index)
{
	struct endpoint_descriptor ep;
	struct mic_array_device_specific_config mic_s_conf;
	struct mic_array_device_specific_vendor_config mic_v_conf;
	struct mic_snr_sensitivity_extension mic_ext;
	struct mic_vendor_config mic_conf;
	struct formats_config f_conf;
	struct format_config f_conf1;
	uint8_t *ep_target;
	size_t blob_size;
	int ret;
	int i;

	size_t mic_config_size;
	uint32_t sample_rate;
	uint16_t channel_count;
	uint32_t bits_per_sample;
	uint8_t array_type;
	uint8_t extension;
	uint8_t num_mics;
	uint32_t snr;
	uint32_t sensitivity;

	uint8_t type;
	uint8_t panel;
	uint32_t speaker_position_distance;
	uint32_t horizontal_offset;
	uint32_t vertical_offset;
	uint8_t frequency_low_band;
	uint8_t frequency_high_band;
	uint16_t direction_angle;
	uint16_t elevation_angle;
	uint16_t vertical_angle_begin;
	uint16_t vertical_angle_end;
	uint16_t horizontal_angle_begin;
	uint16_t horizontal_angle_end;

	/*
	 * nhlt dmic structure:
	 *
	 * endpoint_descriptor, sizeof(struct endpoint_descriptor)
	 *
	 * device_specific_config (mic), sizeof(mic_array_device_specific_config)
	 * or
	 * device_specific_config (mic), sizeof(mic_array_device_specific_vendor_config)
	 *
	 * formats_config (formats_count), sizeof(struct formats_config)
	 * format_config (waveex), sizeof(struct format_config)
	 * vendor_blob sizeof(vendor_blob)
	 */

	/* dmic ep */
	ep.link_type = NHLT_LINK_TYPE_PDM;
	ep.instance_id = 0;
	ep.vendor_id = NHLT_VENDOR_ID_INTEL;
	ep.device_id = NHLT_DEVICE_ID_INTEL_PDM_DMIC;
	ep.revision_id = 0;
	ep.subsystem_id = 0;
	ep.device_type = 0;
	ep.direction = NHLT_ENDPOINT_DIRECTION_CAPTURE;
	ep.virtualbus_id = index;

	ret = dmic_get_params(nhlt, index, &sample_rate, &channel_count, &bits_per_sample,
			      &array_type, &num_mics, &extension, &snr, &sensitivity);

	if (ret) {
		fprintf(stderr, "nhlt_dmic_get_ep: dmic_get_params failed\n");
		return ret;
	}

	if (array_type == NHLT_MIC_ARRAY_TYPE_VENDOR_DEFINED) {
		mic_v_conf.config.capabilities_size = 4 + num_mics *
			sizeof(struct mic_vendor_config);
		mic_v_conf.device_config.virtual_slot = 0; /* always 0 for dmic */
		mic_v_conf.device_config.config_type = NHLT_DEVICE_CONFIG_TYPE_MICARRAY;
		mic_v_conf.number_of_microphones = num_mics;
		mic_v_conf.array_type_ex = array_type;
		/* precense of extension struct is coded into lower 4 bits of array_type */
		if (extension) {
			mic_v_conf.array_type_ex = (array_type & ~0x0F) | (0x01 & 0x0F);
			mic_v_conf.config.capabilities_size +=
				sizeof(struct mic_snr_sensitivity_extension);
		}
	} else {
		mic_s_conf.config.capabilities_size = 3;
		mic_s_conf.device_config.virtual_slot = 0; /* always 0 for dmic */
		mic_s_conf.device_config.config_type = NHLT_DEVICE_CONFIG_TYPE_MICARRAY;
		mic_s_conf.array_type_ex = array_type;
		/* presense of extension struct coded into lower 4 bits of array_type */
		if (extension) {
			mic_s_conf.array_type_ex = (array_type & ~0x0F) | (0x01 & 0x0F);
			mic_s_conf.config.capabilities_size +=
				sizeof(struct mic_snr_sensitivity_extension);
		}
	}

	/* formats_config */
	f_conf.formats_count = 1;

	/* fill in wave format extensible types */
	f_conf1.format.wFormatTag = 0xFFFE;
	f_conf1.format.nSamplesPerSec = sample_rate;
	f_conf1.format.nChannels = channel_count;
	f_conf1.format.wBitsPerSample = bits_per_sample;
	f_conf1.format.nBlockAlign = channel_count * bits_per_sample / 8;
	f_conf1.format.nAvgBytesPerSec = f_conf1.format.nSamplesPerSec * f_conf1.format.nBlockAlign;

	/* bytes after this value in this struct */
	f_conf1.format.cbSize = 22;
	/* actual bits in container */
	f_conf1.format.wValidBitsPerSample = bits_per_sample;
	/* channel map not used at this time */
	f_conf1.format.dwChannelMask = 0;
	/* WAVE_FORMAT_PCM guid (0x0001) ? */
	f_conf1.format.SubFormat[0] = 0;
	f_conf1.format.SubFormat[1] = 0;
	f_conf1.format.SubFormat[2] = 0;
	f_conf1.format.SubFormat[3] = 0;

	ret = dmic_get_vendor_blob_size(nhlt, &blob_size);
	if (ret) {
		fprintf(stderr, "nhlt_dmic_get_ep: dmic_get_vendor_blob_size failed\n");
		return ret;
	}

	f_conf1.vendor_blob.capabilities_size = blob_size;

	if (array_type == NHLT_MIC_ARRAY_TYPE_VENDOR_DEFINED)
		mic_config_size = sizeof(struct mic_array_device_specific_vendor_config) +
			num_mics * sizeof(struct mic_vendor_config);
	else
		mic_config_size = sizeof(struct mic_array_device_specific_config);

	if (extension)
		mic_config_size = sizeof(struct mic_snr_sensitivity_extension);

	ep.length = sizeof(struct endpoint_descriptor) +
		mic_config_size +
		sizeof(struct formats_config) +
		sizeof(struct format_config) +
		blob_size;

	/* allocate the final variable length ep struct */
	ep_target = calloc(ep.length, sizeof(uint8_t));
	if (!ep_target)
		return -ENOMEM;

	*eps = (struct endpoint_descriptor *)ep_target;

	/* copy all parsed sub arrays into the top level array */
	memcpy(ep_target, &ep, sizeof(struct endpoint_descriptor));

	ep_target += sizeof(struct endpoint_descriptor);

	if (array_type == NHLT_MIC_ARRAY_TYPE_VENDOR_DEFINED) {
		memcpy(ep_target, &mic_v_conf,
		       sizeof(struct mic_array_device_specific_vendor_config));
		ep_target += sizeof(struct mic_array_device_specific_vendor_config);
		for (i = 0; i < num_mics; i++) {
			ret = dmic_get_mic_params(nhlt, i, &type,
						  &panel, &speaker_position_distance,
						  &horizontal_offset, &vertical_offset,
						  &frequency_low_band, &frequency_high_band,
						  &direction_angle, &elevation_angle,
						  &vertical_angle_begin, &vertical_angle_end,
						  &horizontal_angle_begin, &horizontal_angle_end);

			if (ret) {
				fprintf(stderr, "nhlt_dmic_get_ep: dmic_get_mic_params failed\n");
				return ret;
			}

			mic_conf.type = type;
			mic_conf.panel = panel;
			mic_conf.speaker_position_distance = speaker_position_distance;
			mic_conf.horizontal_offset = horizontal_offset;
			mic_conf.vertical_offset = vertical_offset;
			mic_conf.frequency_low_band = frequency_low_band;
			mic_conf.frequency_high_band = frequency_high_band;
			mic_conf.direction_angle = direction_angle;
			mic_conf.elevation_angle = elevation_angle;
			mic_conf.vertical_angle_begin = vertical_angle_begin;
			mic_conf.vertical_angle_end = vertical_angle_end;
			mic_conf.horizontal_angle_begin = horizontal_angle_begin;
			mic_conf.horizontal_angle_end = horizontal_angle_end;

			memcpy(ep_target, &mic_conf, sizeof(struct mic_vendor_config));
			ep_target += sizeof(struct mic_vendor_config);
		}
	} else {
		memcpy(ep_target, &mic_s_conf, sizeof(struct mic_array_device_specific_config));
		ep_target += sizeof(struct mic_array_device_specific_config);
	}

	if (extension) {
		mic_ext.snr = snr;
		mic_ext.sensitivity = sensitivity;
		memcpy(ep_target, &mic_ext, sizeof(struct mic_snr_sensitivity_extension));
		ep_target += sizeof(struct mic_snr_sensitivity_extension);
	}

	memcpy(ep_target, &f_conf, sizeof(struct formats_config));
	ep_target += sizeof(struct formats_config);

	memcpy(ep_target, &f_conf1, sizeof(struct format_config));
	ep_target += sizeof(struct format_config);

	ret = dmic_get_vendor_blob(nhlt, ep_target);
	if (ret) {
		fprintf(stderr, "nhlt_dmic_get_ep: dmic_get_vendor_blob failed\n");
		return ret;
	}

	return 0;
}

/*
 * Set dmic parameters from topology for dmic coefficient calculation.
 *
 * Coefficients are recalculated in case of multiple DAIs in topology and might affect each other.
 *
 * You can see an example of topology v2 config of dmic below. In this example the default
 * object parameters are spelled out for clarity. General parameters like clk_min are parsed with
 * set_dmic_data and pdm object data with set_pdm_data. Number of pdm's can vary from 1 to 2. Values
 * are saved into intermediate structs and the vendor specific blob is calculated at the end of
 * parsing with dmic_calculate.
 *
 *	DMIC."0" {
 *		name NoCodec-6
 *		id 6
 *		index 0
 *		driver_version		1
 *		io_clk                  38400000
 *		clk_min			500000
 *		clk_max			4800000
 *		duty_min		40
 *		duty_max		60
 *		sample_rate		48000
 *		fifo_word_length	16
 *		unmute_ramp_time_ms	200
 *		num_pdm_active          2
 *
 *		# PDM controller config
 *		Object.Base.pdm_config."0" {
 *			ctrl_id	0
 *			mic_a_enable	1
 *			mic_b_enable	1
 *			polarity_a	0
 *			polarity_b	0
 *			clk_edge	0
 *			skew		0
 *		}
 *      }
 */
int nhlt_dmic_set_params(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top)
{
	snd_config_t *items;
	int ret;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;

	/* set basic dmic data */
	ret = set_dmic_data(nhlt, cfg, top);
	if (ret < 0)
		return ret;

	/* we need to have at least one pdm object */
	ret = snd_config_search(cfg, "Object.Base.pdm_config", &items);
	if (ret < 0)
		return ret;

	snd_config_for_each(i, next, items) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			continue;

		ret = set_pdm_data(nhlt, n, top);
		if (ret < 0)
			return ret;
	}

	/* check for microphone parameter configuration */
	ret = snd_config_search(cfg, "Object.Base.mic_extension", &items);
	if (!ret) {
		snd_config_for_each(i, next, items) {
			n = snd_config_iterator_entry(i);

			if (snd_config_get_id(n, &id) < 0)
				continue;

			ret = set_mic_data(nhlt, n, top);
			if (ret < 0)
				return ret;
		}
	}

	/* check for microphone parameter configuration */
	ret = snd_config_search(cfg, "Object.Base.vendor_mic_config", &items);
	if (!ret) {
		snd_config_for_each(i, next, items) {
			n = snd_config_iterator_entry(i);

			if (snd_config_get_id(n, &id) < 0)
				continue;

			set_vendor_mic_data(nhlt, n, top);
		}
	}

	/* check for optional filter coeffs */
	ret = snd_config_search(cfg, "Object.Base.data", &items);
	if (!ret) {
		snd_config_for_each(i, next, items) {
			n = snd_config_iterator_entry(i);

			if (snd_config_get_id(n, &id) < 0)
				continue;

			set_bytes_data(nhlt, n);
		}
	}

	ret = dmic_calculate(nhlt);

	return ret;
}
