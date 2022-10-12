// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __SSP_INTEL_H
#define __SSP_INTEL_H

#include <stdint.h>

/* struct for intel ssp nhlt vendor specific blob generation */
struct ssp_intel_config_data {
	uint32_t gateway_attributes;
	uint32_t ts_group[8];
	uint32_t ssc0;
	uint32_t ssc1;
	uint32_t sscto;
	uint32_t sspsp;
	uint32_t sstsa;
	uint32_t ssrsa;
	uint32_t ssc2;
	uint32_t sspsp2;
	uint32_t ssc3;
	uint32_t ssioc;
	uint32_t mdivc;
	uint32_t mdivr;
} __attribute__((packed));

#define SSP_BLOB_VER_1_5 0xEE000105

struct ssp_intel_config_data_1_5 {
	uint32_t gateway_attributes;
	uint32_t version;
	uint32_t size;
	uint32_t ts_group[8];
	uint32_t ssc0;
	uint32_t ssc1;
	uint32_t sscto;
	uint32_t sspsp;
	uint32_t sstsa;
	uint32_t ssrsa;
	uint32_t ssc2;
	uint32_t sspsp2;
	uint32_t ssc3;
	uint32_t ssioc;
	uint32_t mdivctlr;
	uint32_t mdivrcnt;
	uint32_t mdivr[];
} __attribute__((packed));

struct ssp_intel_aux_tlv {
	uint32_t type;
	uint32_t size;
	uint32_t val[];
} __attribute__((packed));

struct ssp_intel_mn_ctl {
	uint32_t div_m;
	uint32_t div_n;
} __attribute__((packed));

struct ssp_intel_clk_ctl {
	uint32_t start;
	uint32_t stop;
} __attribute__((packed));

struct ssp_intel_tr_ctl {
	uint32_t sampling_frequency;
	uint32_t bit_depth;
	uint32_t channel_map;
	uint32_t channel_config;
	uint32_t interleaving_style;
	uint32_t format;
} __attribute__((packed));

struct ssp_intel_run_ctl {
	uint32_t enabled;
} __attribute__((packed));

struct ssp_intel_node_ctl {
	uint32_t node_id;
	uint32_t sampling_rate;
} __attribute__((packed));

struct ssp_intel_sync_ctl {
	uint32_t sync_denominator;
	uint32_t count;
} __attribute__((packed));

struct ssp_intel_ext_ctl {
	uint32_t ext_data;
} __attribute__((packed));

struct ssp_intel_link_ctl {
	uint32_t clock_source;
} __attribute__((packed));

#endif /* __SSP_INTEL_H */
