// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __SSP_DEBUG_H
#define __SSP_DEBUG_H

#include "ssp-internal.h"

void ssp_print_internal(struct intel_ssp_params *ssp);
void ssp_print_calculated(struct intel_ssp_params *ssp);

#endif
