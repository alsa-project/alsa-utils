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
	struct ssp_intel_config_data *blob;
	int ssp_index = ssp->ssp_count;
	int i;

	fprintf(stdout, "printing ssp nhlt calculated data:\n");

	/* top level struct */
	fprintf(stdout, "ssp index %d\n", ssp_index);

	fprintf(stdout, "ssp %d dai_index: %u\n", ssp_index, ssp->ssp_dai_index[ssp_index]);

	fprintf(stdout, "ssp %d hw_config_count: %u\n", ssp_index,
		ssp->ssp_hw_config_count[ssp_index]);

	fprintf(stdout, "\n");

	for (i = 0; i < ssp->ssp_hw_config_count[ssp_index]; i++) {
		blob = &ssp->ssp_blob[ssp->ssp_count][i];
		fprintf(stdout, "ssp blob %d hw_config %d\n", ssp->ssp_count, i);
		fprintf(stdout, "gateway_attributes %u\n", blob->gateway_attributes);
		fprintf(stdout, "ts_group[0] 0x%08x\n", blob->ts_group[0]);
		fprintf(stdout, "ts_group[1] 0x%08x\n", blob->ts_group[1]);
		fprintf(stdout, "ts_group[2] 0x%08x\n", blob->ts_group[2]);
		fprintf(stdout, "ts_group[3] 0x%08x\n", blob->ts_group[3]);
		fprintf(stdout, "ts_group[4] 0x%08x\n", blob->ts_group[4]);
		fprintf(stdout, "ts_group[5] 0x%08x\n", blob->ts_group[5]);
		fprintf(stdout, "ts_group[6] 0x%08x\n", blob->ts_group[6]);
		fprintf(stdout, "ts_group[7] 0x%08x\n", blob->ts_group[7]);
		fprintf(stdout, "ssc0 0x%08x\n", blob->ssc0);
		fprintf(stdout, "ssc1 0x%08x\n", blob->ssc1);
		fprintf(stdout, "sscto 0x%08x\n", blob->sscto);
		fprintf(stdout, "sspsp 0x%08x\n", blob->sspsp);
		fprintf(stdout, "sstsa 0x%08x\n", blob->sstsa);
		fprintf(stdout, "ssrsa 0x%08x\n", blob->ssrsa);
		fprintf(stdout, "ssc2 0x%08x\n", blob->ssc2);
		fprintf(stdout, "sspsp2 0x%08x\n", blob->sspsp2);
		fprintf(stdout, "ssc3 0x%08x\n", blob->ssc3);
		fprintf(stdout, "ssioc 0x%08x\n", blob->ssioc);
		fprintf(stdout, "mdivc 0x%08x\n", blob->mdivc);
		fprintf(stdout, "mdivr 0x%08x\n", blob->mdivr);
	}

	fprintf(stdout, "\n");
}

void ssp_print_internal(struct intel_ssp_params *ssp)
{
	struct ssp_config_hw *hw_conf;
	struct ssp_config_dai *dai;
	int i;

	dai = &ssp->ssp_prm[ssp->ssp_count];

	fprintf(stdout, "printing ssp nhlt internal data:\n");

	fprintf(stdout, "io_clk %u\n", dai->io_clk);
	fprintf(stdout, "dai_index %u\n", dai->dai_index);
	fprintf(stdout, "mclk_id %u\n", dai->mclk_id);
	fprintf(stdout, "sample_valid_bits %u\n", dai->sample_valid_bits);
	fprintf(stdout, "mclk_direction %u\n", dai->mclk_direction);
	fprintf(stdout, "frame_pulse_width %u\n", dai->frame_pulse_width);
	fprintf(stdout, "tdm_per_slot_padding_flag %u\n", dai->tdm_per_slot_padding_flag);
	fprintf(stdout, "clks_control %u\n", dai->clks_control);
	fprintf(stdout, "quirks %u\n", dai->quirks);
	fprintf(stdout, "bclk_delay %u\n", dai->bclk_delay);

	fprintf(stdout, "\n");

	fprintf(stdout, "hw_config_count %u\n", ssp->ssp_hw_config_count[ssp->ssp_count]);

	for (i = 0; i < ssp->ssp_hw_config_count[ssp->ssp_count]; i++) {
		hw_conf = &dai->hw_cfg[i];
		fprintf(stdout, "mclk_rate %u\n", hw_conf->mclk_rate);
		fprintf(stdout, "bclk_rate %u\n", hw_conf->bclk_rate);
		fprintf(stdout, "fsync_rate %u\n", hw_conf->fsync_rate);
		fprintf(stdout, "tdm_slots %u\n", hw_conf->tdm_slots);
		fprintf(stdout, "tdm_slot_width %u\n", hw_conf->tdm_slot_width);
		fprintf(stdout, "tx_slots %u\n", hw_conf->tx_slots);
		fprintf(stdout, "rx_slots %u\n", hw_conf->rx_slots);
		fprintf(stdout, "format %u\n", hw_conf->format);
	}

	fprintf(stdout, "\n");
}

#else /* NHLT_DEBUG */
void ssp_print_internal(struct intel_ssp_params *ssp) {}
void ssp_print_calculated(struct intel_ssp_params *ssp) {}
#endif
