// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __DMIC_PROCESS_H
#define __DMIC_PROCESS_H

#include <stdint.h>

/* initialize and set default values before parsing */
int dmic_init_params(struct intel_nhlt_params *nhlt);

/* set parameters when parsing topology2 conf */
int dmic_set_params(struct intel_nhlt_params *nhlt, int dai_index, int driver_version,
		    int io_clk, int num_pdm_active, int fifo_word_length, int clk_min, int clk_max,
		    int duty_min, int duty_max, int sample_rate, int unmute_ramp_time);
int dmic_set_pdm_params(struct intel_nhlt_params *nhlt, int pdm_index, int enable_a,
			int enable_b, int polarity_a, int polarity_b, int clk_edge, int skew);
int dmic_set_ext_params(struct intel_nhlt_params *nhlt, uint32_t snr, uint32_t sensitivity);
int dmic_set_mic_params(struct intel_nhlt_params *nhlt, int index,
			uint8_t type, uint8_t panel, uint32_t speaker_position_distance,
			uint32_t horizontal_offset, uint32_t vertical_offset,
			uint8_t frequency_low_band, uint8_t frequency_high_band,
			uint16_t direction_angle, uint16_t elevation_angle,
			uint16_t vertical_angle_begin, uint16_t vertical_angle_end,
			uint16_t horizontal_angle_begin, uint16_t horizontal_angle_end);

/* calculate the blob after parsing the values*/
int dmic_calculate(struct intel_nhlt_params *nhlt);

/* get spec parameters when building the nhlt endpoint */
int dmic_get_params(struct intel_nhlt_params *nhlt, int index, uint32_t *sample_rate,
		    uint16_t *channel_count, uint32_t *bits_per_sample, uint8_t *array_type,
		    uint8_t *num_mics, uint8_t *extension, uint32_t *snr, uint32_t *sensitivity);
int dmic_get_mic_params(struct intel_nhlt_params *nhlt, int index,
			uint8_t *type, uint8_t *panel, uint32_t *speaker_position_distance,
			uint32_t *horizontal_offset, uint32_t *vertical_offset,
			uint8_t *frequency_low_band, uint8_t *frequency_high_band,
			uint16_t *direction_angle, uint16_t *elevation_angle,
			uint16_t *vertical_angle_begin, uint16_t *vertical_angle_end,
			uint16_t *horizontal_angle_begin, uint16_t *horizontal_angle_end);

/* get vendor specific blob when building the nhlt endpoint */
int dmic_get_vendor_blob_count(struct intel_nhlt_params *nhlt);
int dmic_get_vendor_blob_size(struct intel_nhlt_params *nhlt, size_t *size);
int dmic_get_vendor_blob(struct intel_nhlt_params *nhlt, uint8_t *vendor_blob);

#endif
