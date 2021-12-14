// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __SSP_PROCESS_H
#define __SSP_PROCESS_H

#include <stdint.h>

/* initialize and set default values before parsing */
int ssp_init_params(struct intel_nhlt_params *nhlt);

/* set parameters when parsing topology2 conf */
int ssp_set_params(struct intel_nhlt_params *nhlt, const char *dir, int dai_index, int io_clk,
		   int bclk_delay, int sample_bits, int mclk_id, int clks_control,
		   int frame_pulse_width, const char *tdm_padding_per_slot, const char *quirks);
int ssp_hw_set_params(struct intel_nhlt_params *nhlt, const char *format, const char *mclk,
		      const char *bclk, const char *bclk_invert, const char *fsync,
		      const char *fsync_invert, int mclk_freq, int bclk_freq, int fsync_freq,
		      int tdm_slots, int tdm_slot_width, int tx_slots, int rx_slots);

/* calculate the blob after parsing the values*/
int ssp_calculate(struct intel_nhlt_params *nhlt);
/* get spec parameters when building the nhlt endpoint */
int ssp_get_params(struct intel_nhlt_params *nhlt, int dai_index, uint32_t *virtualbus_id,
		   uint32_t *formats_count);
int ssp_get_hw_params(struct intel_nhlt_params *nhlt, int hw_index, uint32_t *sample_rate,
		      uint16_t *channel_count, uint32_t *bits_per_sample);
int ssp_get_dir(struct intel_nhlt_params *nhlt, int dai_index, uint8_t *dir);
/* get vendor specific blob when building the nhlt endpoint */
int ssp_get_vendor_blob_count(struct intel_nhlt_params *nhlt);
int ssp_get_vendor_blob_size(struct intel_nhlt_params *nhlt, size_t *size);
int ssp_get_vendor_blob(struct intel_nhlt_params *nhlt, uint8_t *vendor_blob, int dai_index,
			int hw_config_index);

#endif
