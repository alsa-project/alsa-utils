// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __DMIC_DEBUG_H
#define __DMIC_DEBUG_H

#include "dmic-internal.h"

void dmic_print_bytes_as_hex(uint8_t *src, size_t size);
void dmic_print_integers_as_hex(uint32_t *src, size_t size);
void dmic_print_internal(struct intel_dmic_params *dmic);

#endif
