// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __DMIC_NHLT_H
#define __DMIC_NHLT_H

#include "intel-nhlt.h"
#include "../nhlt.h"

int nhlt_dmic_init_params(struct intel_nhlt_params *nhlt);
int nhlt_dmic_set_params(struct intel_nhlt_params *nhlt, snd_config_t *cfg, snd_config_t *top);
int nhlt_dmic_get_ep(struct intel_nhlt_params *nhlt, struct endpoint_descriptor **eps,
		     int index);
int nhlt_dmic_get_ep_count(struct intel_nhlt_params *nhlt);

#endif
