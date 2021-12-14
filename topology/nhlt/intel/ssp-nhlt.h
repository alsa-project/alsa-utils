// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __SSP_NHLT_H
#define __SSP_NHLT_H

#include "intel-nhlt.h"
#include "../nhlt.h"

int nhlt_ssp_init_params(struct intel_nhlt_params *nhlt);
int nhlt_ssp_set_params(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top);
int nhlt_ssp_get_ep(struct intel_nhlt_params *nhlt, struct endpoint_descriptor **eps,
		    int dai_index, uint8_t dir);
int nhlt_ssp_get_ep_count(struct intel_nhlt_params *nhlt);
int nhlt_ssp_get_dir(struct intel_nhlt_params *nhlt, int dai_index, uint8_t *dir);
#endif
