// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdio.h>
#include <stdint.h>
#include "ssp-debug.h"

#ifdef NHLT_DEBUG

void ssp_print_calculated(struct intel_ssp_params *ssp)
{
	int i, j;

	fprintf(stdout, "printing ssp nhlt calculated data:\n");

	/* top level struct */
	fprintf(stdout, "ssp count %d\n", ssp->ssp_count);

	for (i = 0; i < ssp->ssp_count; i++)
		fprintf(stdout, "ssp %d dai_index: %u\n", i, ssp->ssp_dai_index[i]);

	for (i = 0; i < ssp->ssp_count; i++)
		fprintf(stdout, "ssp %d hw_config_count: %u\n", i, ssp->ssp_hw_config_count[i]);

	fprintf(stdout, "\n");

	for (i = 0; i < ssp->ssp_count; i++) {
		for (j = 0; j < ssp->ssp_hw_config_count[i]; j++) {
			fprintf(stdout, "ssp blob %d hw_config %d\n", i, j);
			fprintf(stdout, "gateway_attributes %u\n",
				ssp->ssp_blob[i][j].gateway_attributes);
			fprintf(stdout, "ts_group[0] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[0]);
			fprintf(stdout, "ts_group[1] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[1]);
			fprintf(stdout, "ts_group[2] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[2]);
			fprintf(stdout, "ts_group[3] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[3]);
			fprintf(stdout, "ts_group[4] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[4]);
			fprintf(stdout, "ts_group[5] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[5]);
			fprintf(stdout, "ts_group[6] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[6]);
			fprintf(stdout, "ts_group[7] 0x%08x\n", ssp->ssp_blob[i][j].ts_group[7]);
			fprintf(stdout, "ssc0 0x%08x\n", ssp->ssp_blob[i][j].ssc0);
			fprintf(stdout, "ssc1 0x%08x\n", ssp->ssp_blob[i][j].ssc1);
			fprintf(stdout, "sscto 0x%08x\n", ssp->ssp_blob[i][j].sscto);
			fprintf(stdout, "sspsp 0x%08x\n", ssp->ssp_blob[i][j].sspsp);
			fprintf(stdout, "sstsa 0x%08x\n", ssp->ssp_blob[i][j].sstsa);
			fprintf(stdout, "ssrsa 0x%08x\n", ssp->ssp_blob[i][j].ssrsa);
			fprintf(stdout, "ssc2 0x%08x\n", ssp->ssp_blob[i][j].ssc2);
			fprintf(stdout, "sspsp2 0x%08x\n", ssp->ssp_blob[i][j].sspsp2);
			fprintf(stdout, "ssc3 0x%08x\n", ssp->ssp_blob[i][j].ssc3);
			fprintf(stdout, "ssioc 0x%08x\n", ssp->ssp_blob[i][j].ssioc);
			fprintf(stdout, "mdivc 0x%08x\n", ssp->ssp_blob[i][j].mdivc);
			fprintf(stdout, "mdivr 0x%08x\n", ssp->ssp_blob[i][j].mdivr);
		}
	}

	fprintf(stdout, "\n");
}

void ssp_print_internal(struct intel_ssp_params *ssp)
{
	int i;

	fprintf(stdout, "printing ssp nhlt internal data:\n");

	fprintf(stdout, "io_clk %u\n", ssp->ssp_prm.io_clk);
	fprintf(stdout, "dai_index %u\n", ssp->ssp_prm.dai_index);
	fprintf(stdout, "mclk_id %u\n", ssp->ssp_prm.mclk_id);
	fprintf(stdout, "sample_valid_bits %u\n", ssp->ssp_prm.sample_valid_bits);
	fprintf(stdout, "mclk_direction %u\n", ssp->ssp_prm.mclk_direction);
	fprintf(stdout, "frame_pulse_width %u\n", ssp->ssp_prm.frame_pulse_width);
	fprintf(stdout, "tdm_per_slot_padding_flag %u\n", ssp->ssp_prm.tdm_per_slot_padding_flag);
	fprintf(stdout, "clks_control %u\n", ssp->ssp_prm.clks_control);
	fprintf(stdout, "quirks %u\n", ssp->ssp_prm.quirks);
	fprintf(stdout, "bclk_delay %u\n", ssp->ssp_prm.bclk_delay);

	fprintf(stdout, "\n");

	fprintf(stdout, "hw_config_count %u\n", ssp->ssp_hw_config_count[ssp->ssp_count]);

	for (i = 0; i < ssp->ssp_hw_config_count[ssp->ssp_count]; i++) {
		fprintf(stdout, "mclk_rate %u\n", ssp->ssp_prm.hw_cfg[i].mclk_rate);
		fprintf(stdout, "bclk_rate %u\n", ssp->ssp_prm.hw_cfg[i].bclk_rate);
		fprintf(stdout, "fsync_rate %u\n", ssp->ssp_prm.hw_cfg[i].fsync_rate);
		fprintf(stdout, "tdm_slots %u\n", ssp->ssp_prm.hw_cfg[i].tdm_slots);
		fprintf(stdout, "tdm_slot_width %u\n", ssp->ssp_prm.hw_cfg[i].tdm_slot_width);
		fprintf(stdout, "tx_slots %u\n", ssp->ssp_prm.hw_cfg[i].tx_slots);
		fprintf(stdout, "rx_slots %u\n", ssp->ssp_prm.hw_cfg[i].rx_slots);
		fprintf(stdout, "format %u\n", ssp->ssp_prm.hw_cfg[i].format);
	}

	fprintf(stdout, "\n");
}

#else /* NHLT_DEBUG */
void ssp_print_internal(struct intel_ssp_params *ssp) {}
void ssp_print_calculated(struct intel_ssp_params *ssp) {}
#endif
