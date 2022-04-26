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

#endif /* __SSP_INTEL_H */
