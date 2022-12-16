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
		   int frame_pulse_width, const char *tdm_padding_per_slot, const char *quirks,
		   int version);
int ssp_hw_set_params(struct intel_nhlt_params *nhlt, const char *format, const char *mclk,
		      const char *bclk, const char *bclk_invert, const char *fsync,
		      const char *fsync_invert, int mclk_freq, int bclk_freq, int fsync_freq,
		      int tdm_slots, int tdm_slot_width, int tx_slots, int rx_slots);

/* set aux params when parsing topology2 conf */
int ssp_mn_set_params(struct intel_nhlt_params *nhlt, int m_div, int n_div);
int ssp_clk_set_params(struct intel_nhlt_params *nhlt, int clock_warm_up, int mclk, int warm_up_ovr,
		       int clock_stop_delay, int keep_running, int clock_stop_ovr);
int ssp_tr_start_set_params(struct intel_nhlt_params *nhlt, int sampling_frequency,
			    int bit_depth, int channel_map, int hannel_config,
			    int interleaving_style, int number_of_channels,
			    int valid_bit_depth, int sample_type);
int ssp_tr_stop_set_params(struct intel_nhlt_params *nhlt, int sampling_frequency,
			   int bit_depth, int channel_map, int hannel_config,
			   int interleaving_style, int number_of_channels,
			   int valid_bit_depth, int sample_type);
int ssp_run_set_params(struct intel_nhlt_params *nhlt, int always_run);
int ssp_sync_set_params(struct intel_nhlt_params *nhlt, int sync_denominator);
int ssp_node_set_params(struct intel_nhlt_params *nhlt, int node_id, int sampling_rate);
int ssp_ext_set_params(struct intel_nhlt_params *nhlt, int mclk_policy_override,
		       int mclk_always_running, int mclk_starts_on_gtw_init, int mclk_starts_on_run,
		       int mclk_starts_on_pause, int mclk_stops_on_pause, int mclk_stops_on_reset,
		       int bclk_policy_override, int bclk_always_running,
		       int bclk_starts_on_gtw_init, int bclk_starts_on_run,
		       int bclk_starts_on_pause, int bclk_stops_on_pause, int bclk_stops_on_reset,
		       int sync_policy_override, int sync_always_running,
		       int sync_starts_on_gtw_init, int sync_starts_on_run,
		       int sync_starts_on_pause, int sync_stops_on_pause, int sync_stops_on_reset);
int ssp_link_set_params(struct intel_nhlt_params *nhlt, int clock_source);

/* calculate the blob after parsing the values*/
int ssp_calculate(struct intel_nhlt_params *nhlt);
/* get spec parameters when building the nhlt endpoint */
int ssp_get_params(struct intel_nhlt_params *nhlt, int dai_index, uint32_t *virtualbus_id,
		   uint32_t *formats_count);
int ssp_get_hw_params(struct intel_nhlt_params *nhlt, int dai_index, int hw_index,
		      uint32_t *sample_rate, uint16_t *channel_count, uint32_t *bits_per_sample);
int ssp_get_dir(struct intel_nhlt_params *nhlt, int dai_index, uint8_t *dir);
/* get vendor specific blob when building the nhlt endpoint */
int ssp_get_vendor_blob_count(struct intel_nhlt_params *nhlt);
int ssp_get_vendor_blob_size(struct intel_nhlt_params *nhlt, int dai_index, int hw_config_index,
			     size_t *size);
int ssp_get_vendor_blob(struct intel_nhlt_params *nhlt, uint8_t *vendor_blob, int dai_index,
			int hw_config_index);

#endif
