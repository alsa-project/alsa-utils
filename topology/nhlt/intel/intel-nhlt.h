// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __INTEL_NHLT_H
#define __INTEL_NHLT_H

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>

#define MIN(a, b) ({		\
	typeof(a) __a = (a);	\
	typeof(b) __b = (b);	\
	__a > __b ? __b : __a;	\
})
#define MAX(a, b) ({		\
	typeof(a) __a = (a);	\
	typeof(b) __b = (b);	\
	__a < __b ? __b : __a;	\
})

#define ARRAY_SIZE(a) (sizeof (a) / sizeof (a)[0])

#define BIT(b)                  (1UL << (b))
#define MASK(b_hi, b_lo)        (((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL) << (b_lo))
#define SET_BIT(b, x)           (((x) & 1) << (b))
#define SET_BITS(b_hi, b_lo, x) (((x) & ((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL)) << (b_lo))

struct intel_nhlt_params {
	void *dmic_params;
	void *ssp_params;
};

struct dai_values {
	char name[32];
	snd_config_type_t type;
	snd_config_t *data;
	long *int_val;
	const char **string_val;
};

int find_set_values(struct dai_values *values, int size, snd_config_t *dai_cfg,
		    snd_config_t *top, const char *class_name);

#endif /* __INTEL_NHLT_H */
