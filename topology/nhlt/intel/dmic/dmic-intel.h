// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __DMIC_INTEL_H
#define __DMIC_INTEL_H

#include <stdint.h>

#define DMIC_TS_GROUP_SIZE 4

/* structs for intel dmic nhlt vendor specific blob generation */
struct dmic_intel_fir_config {
	uint32_t fir_control;
	uint32_t fir_config;
	uint32_t dc_offset_left;
	uint32_t dc_offset_right;
	uint32_t out_gain_left;
	uint32_t out_gain_right;
	uint32_t reserved2[2];
	uint32_t fir_coeffs[];
} __attribute__((packed));

struct dmic_intel_pdm_ctrl_cfg {
	uint32_t cic_control;
	uint32_t cic_config;
	uint32_t reserved0;
	uint32_t mic_control;
	uint32_t pdmsm;
	uint32_t reuse_fir_from_pdm;
	uint32_t reserved1[2];
	struct dmic_intel_fir_config fir_config[];
} __attribute__((packed));

struct dmic_intel_config_data {
	uint32_t gateway_attributes;
	uint32_t ts_group[DMIC_TS_GROUP_SIZE];
	uint32_t clock_on_delay;
	uint32_t channel_ctrl_mask;
	uint32_t chan_ctrl_cfg[2];
	uint32_t channel_pdm_mask;
	struct dmic_intel_pdm_ctrl_cfg pdm_ctrl_cfg[];
} __attribute__((packed));

#endif /* __DMIC_INTEL_H */
