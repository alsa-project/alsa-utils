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
#include "intel-nhlt.h"
#include "ssp-nhlt.h"
#include "ssp/ssp-process.h"

static int set_ssp_data(struct intel_nhlt_params *nhlt, snd_config_t *dai_cfg, snd_config_t *top)
{
	const char *tdm_padding_per_slot = NULL;
	const char *direction = NULL;
	const char *quirks = NULL;
	long frame_pulse_width = 0;
	long clks_control = 0;
	long sample_bits = 0;
	long bclk_delay = 0;
	long dai_index = 0;
	long mclk_id = 0;
	long io_clk = 0;
	int ret;

	struct dai_values ssp_data[] = {
		{ "io_clk",  SND_CONFIG_TYPE_INTEGER, NULL, &io_clk, NULL},
		{ "direction", SND_CONFIG_TYPE_STRING, NULL, NULL, &direction},
		{ "quirks", SND_CONFIG_TYPE_STRING, NULL, NULL, &quirks},
		{ "dai_index", SND_CONFIG_TYPE_INTEGER, NULL, &dai_index, NULL},
		{ "sample_bits", SND_CONFIG_TYPE_INTEGER, NULL, &sample_bits, NULL},
		{ "bclk_delay", SND_CONFIG_TYPE_INTEGER, NULL, &bclk_delay, NULL},
		{ "mclk_id", SND_CONFIG_TYPE_INTEGER, NULL, &mclk_id, NULL},
		{ "clks_control", SND_CONFIG_TYPE_INTEGER, NULL, &clks_control, NULL},
		{ "frame_pulse_width", SND_CONFIG_TYPE_INTEGER, NULL, &frame_pulse_width, NULL},
		{ "tdm_padding_per_slot", SND_CONFIG_TYPE_STRING, NULL, NULL,
		  &tdm_padding_per_slot},
	};

	ret = find_set_values(&ssp_data[0], ARRAY_SIZE(ssp_data), dai_cfg, top, "Class.Dai.SSP");
	if (ret < 0)
		return ret;

	return ssp_set_params(nhlt, direction, dai_index, io_clk, bclk_delay, sample_bits, mclk_id,
			      clks_control, frame_pulse_width, tdm_padding_per_slot, quirks);
}

static int set_hw_config(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top)
{
	const char *format = NULL;
	const char *mclk = NULL;
	const char *bclk = NULL;
	const char *bclk_invert = NULL;
	const char *fsync = NULL;
	const char *fsync_invert = NULL;
	long mclk_freq = 0;
	long bclk_freq = 0;
	long fsync_freq = 0;
	long tdm_slots = 0;
	long tdm_slot_width = 0;
	long tx_slots = 0;
	long rx_slots = 0;
	long ret;

	struct dai_values ssp_hw_data[] = {
		{"format", SND_CONFIG_TYPE_STRING, NULL, NULL, &format},
		{"mclk", SND_CONFIG_TYPE_STRING, NULL, NULL, &mclk},
		{"bclk", SND_CONFIG_TYPE_STRING, NULL, NULL, &bclk},
		{"fsync", SND_CONFIG_TYPE_STRING, NULL, NULL, &fsync},
		{"bclk_invert", SND_CONFIG_TYPE_STRING, NULL, NULL, &bclk_invert},
		{"fsync_invert", SND_CONFIG_TYPE_STRING, NULL, NULL, &fsync_invert},
		{"fsync_freq", SND_CONFIG_TYPE_INTEGER, NULL, &fsync_freq, NULL},
		{"bclk_freq", SND_CONFIG_TYPE_INTEGER, NULL, &bclk_freq, NULL},
		{"mclk_freq", SND_CONFIG_TYPE_INTEGER, NULL, &mclk_freq, NULL},
		{"tdm_slots", SND_CONFIG_TYPE_INTEGER, NULL, &tdm_slots, NULL},
		{"tdm_slot_width", SND_CONFIG_TYPE_INTEGER, NULL, &tdm_slot_width, NULL},
		{"tx_slots", SND_CONFIG_TYPE_INTEGER, NULL, &tx_slots, NULL},
		{"rx_slots", SND_CONFIG_TYPE_INTEGER, NULL, &rx_slots, NULL},
	};

	ret = find_set_values(&ssp_hw_data[0], ARRAY_SIZE(ssp_hw_data), cfg, top,
			      "Class.Base.hw_config");
	if (ret < 0)
		return ret;

	return ssp_hw_set_params(nhlt, format, mclk, bclk, bclk_invert, fsync, fsync_invert,
				 mclk_freq, bclk_freq, fsync_freq, tdm_slots, tdm_slot_width,
				 tx_slots, rx_slots);
}

/* init ssp parameters, should be called before parsing dais */
int nhlt_ssp_init_params(struct intel_nhlt_params *nhlt)
{
	return ssp_init_params(nhlt);
}

int nhlt_ssp_get_ep_count(struct intel_nhlt_params *nhlt)
{
	return ssp_get_vendor_blob_count(nhlt);
}

int nhlt_ssp_get_dir(struct intel_nhlt_params *nhlt, int dai_index, uint8_t *dir)
{
	return ssp_get_dir(nhlt, dai_index, dir);
}

int nhlt_ssp_get_ep(struct intel_nhlt_params *nhlt, struct endpoint_descriptor **eps,
		    int dai_index, uint8_t dir)
{
	struct endpoint_descriptor ep;
	struct ssp_device_specific_config ssp_conf;
	struct formats_config f_conf;
	struct format_config f_conf1[8];
	uint32_t sample_rate;
	uint16_t channel_count;
	uint32_t bits_per_sample;
	uint32_t virtualbus_id;
	uint32_t formats_count;
	uint8_t *ep_target;
	size_t blob_size;
	int ret;
	int i;

	/*
	 * nhlt ssp structure:
	 *
	 * endpoint_descriptor, sizeof(struct endpoint_descriptor)
	 * device_specific_config (headset), sizeof(struct ssp_device_specific_config)
	 * formats_config (formats_count), sizeof(struct formats_config)
	 * format_config (waveex), sizeof(struct format_config)
	 * vendor_blob sizeof(vendor_blob)
	 */

	ret = ssp_get_params(nhlt, dai_index, &virtualbus_id, &formats_count);
	if (ret < 0) {
		fprintf(stderr, "nhlt_ssp_get_ep: ssp_get_params failed\n");
		return ret;
	}

	ep.link_type = NHLT_LINK_TYPE_SSP;
	ep.instance_id = 0;
	ep.vendor_id = NHLT_VENDOR_ID_INTEL;
	ep.device_id = NHLT_DEVICE_ID_INTEL_I2S_TDM;
	ep.revision_id = 0;
	ep.subsystem_id = 0;
	ep.device_type = 0;

	ep.direction = dir;
	/* ssp device index */
	ep.virtualbus_id = virtualbus_id;
	/* ssp config */
	ssp_conf.config.capabilities_size = 2;
	ssp_conf.device_config.virtual_slot = 0;
	ssp_conf.device_config.config_type = 0;

	/* formats_config */
	f_conf.formats_count = formats_count;

	for (i = 0; i < f_conf.formats_count; i++) {
		/* fill in wave format extensible types */
		f_conf1[i].format.wFormatTag = 0xFFFE;

		ret = ssp_get_hw_params(nhlt, i, &sample_rate, &channel_count, &bits_per_sample);

		if (ret < 0) {
			fprintf(stderr, "nhlt_ssp_get_ep: ssp_get_hw_params failed\n");
			return ret;
		}

		f_conf1[i].format.nChannels = channel_count;
		f_conf1[i].format.nSamplesPerSec = sample_rate;
		f_conf1[i].format.wBitsPerSample = bits_per_sample;
		f_conf1[i].format.nBlockAlign = channel_count * bits_per_sample / 8;
		f_conf1[i].format.nAvgBytesPerSec = sample_rate * f_conf1[i].format.nBlockAlign;

		/* bytes after this value in this struct */
		f_conf1[i].format.cbSize = 22;
		/* actual bits in container */
		f_conf1[i].format.wValidBitsPerSample = bits_per_sample;
		/* channel map not used at this time */
		f_conf1[i].format.dwChannelMask = 0;
		/* WAVE_FORMAT_PCM guid (0x0001) ? */
		f_conf1[i].format.SubFormat[0] = 0;
		f_conf1[i].format.SubFormat[1] = 0;
		f_conf1[i].format.SubFormat[2] = 0;
		f_conf1[i].format.SubFormat[3] = 0;

		ret = ssp_get_vendor_blob_size(nhlt, &blob_size);
		if (ret < 0) {
			fprintf(stderr, "nhlt_ssp_get_ep: dmic_get_vendor_blob_size failed\n");
			return ret;
		}
		f_conf1[i].vendor_blob.capabilities_size = blob_size;
	}

	ep.length = sizeof(struct endpoint_descriptor) +
		sizeof(struct ssp_device_specific_config) +
		sizeof(struct formats_config) +
		sizeof(struct format_config) * f_conf.formats_count +
		blob_size * f_conf.formats_count;

	/* allocate the final variable length ep struct */
	ep_target = calloc(ep.length, sizeof(uint8_t));
	if (!ep_target)
		return -ENOMEM;

	*eps = (struct endpoint_descriptor *)ep_target;

	/* copy all parsed sub arrays into the top level array */
	memcpy(ep_target, &ep, sizeof(struct endpoint_descriptor));

	ep_target += sizeof(struct endpoint_descriptor);

	memcpy(ep_target, &ssp_conf, sizeof(struct ssp_device_specific_config));
	ep_target += sizeof(struct ssp_device_specific_config);

	memcpy(ep_target, &f_conf, sizeof(struct formats_config));
	ep_target += sizeof(struct formats_config);

	/* copy all hw configs */
	for (i = 0; i < f_conf.formats_count; i++) {
		memcpy(ep_target, &f_conf1[i], sizeof(struct format_config));
		ep_target += sizeof(struct format_config);
		ret = ssp_get_vendor_blob(nhlt, ep_target, dai_index, i);
		if (ret < 0) {
			fprintf(stderr, "nhlt_sso_get_ep: ssp_get_vendor_blob failed\n");
			return ret;
		}
		ep_target += blob_size;
	}

	return 0;
}

/* Set ssp parameters from topology for ssp coefficient calculation.
 *
 * You can see an example of topology v2 config of ssp below. In this example the default
 * object parameters are spelled out for clarity. General parameters like sample_bits are parsed
 * with set_ssp_data and hw_config object data with set_hw_data. Ssp can have multiple hw_configs.
 * Values are saved into intermediate structs and the vendor specific blob is calculated at the end
 * of parsing with ssp_calculate.
 *
 * 	SSP."0" {
 *		id 			0
 *		direction		"duplex"
 *		name			NoCodec-0
 *		io_clk			38400000
 *		default_hw_conf_id	0
 *		sample_bits		16
 *		quirks			"lbm_mode"
 *		bclk_delay		0
 *		mclk_id 		0
 *		clks_control 		0
 *		frame_pulse_width	0
 *		tdm_padding_per_slot	false
 *
 *		Object.Base.hw_config."SSP0" {
 *			id	0
 *			mclk_freq	24576000
 *			bclk_freq	3072000
 *			tdm_slot_width	32
 *			format		"I2S"
 *			mclk		"codec_mclk_in"
 *			bclk		"codec_consumer"
 *			fsync		"codec_consumer"
 *			fsync_freq	48000
 *			tdm_slots	2
 *			tx_slots	3
 *			rx_slots	3
 *		}
 *	}
 */
int nhlt_ssp_set_params(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top)
{
	snd_config_iterator_t i, next;
	snd_config_t *items;
	snd_config_t *n;
	const char *id;
	int ret;

	ret = set_ssp_data(nhlt, cfg, top);
	if (ret < 0)
		return ret;

	ret = snd_config_search(cfg, "Object.Base.hw_config", &items);
	if (ret < 0)
		return ret;

	snd_config_for_each(i, next, items) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			continue;

		ret = set_hw_config(nhlt, n, top);
		if (ret < 0)
			return ret;
	}

	ret = ssp_calculate(nhlt);

	return ret;
}
