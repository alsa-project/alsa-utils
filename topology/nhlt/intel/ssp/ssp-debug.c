// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdio.h>
#include <stdint.h>
#include "ssp-debug.h"
#include "../intel-nhlt.h"

#ifdef NHLT_DEBUG

void ssp_print_calculated(struct intel_ssp_params *ssp)
{
	struct ssp_intel_config_data *blob;
	struct ssp_intel_config_data_1_5 *blob15;
	struct ssp_aux_blob *blob_aux;
	int ssp_index = ssp->ssp_count;
	uint32_t *ptr;
	int i, j;

	fprintf(stdout, "printing ssp nhlt calculated data:\n");

	/* top level struct */
	fprintf(stdout, "ssp index %d\n", ssp_index);

	fprintf(stdout, "ssp %d dai_index: %u\n", ssp_index, ssp->ssp_dai_index[ssp_index]);

	fprintf(stdout, "ssp blob version %u\n", ssp->ssp_prm[ssp_index].version);

	fprintf(stdout, "ssp %d hw_config_count: %u\n", ssp_index,
		ssp->ssp_hw_config_count[ssp_index]);

	fprintf(stdout, "\n");

	for (i = 0; i < ssp->ssp_hw_config_count[ssp_index]; i++) {
		fprintf(stdout, "ssp blob %d hw_config %d\n", ssp->ssp_count, i);
		if (ssp->ssp_prm[ssp_index].version == SSP_BLOB_VER_1_5) {
			blob15 = &ssp->ssp_blob_1_5[ssp_index][i];
			fprintf(stdout, "gateway_attributes %u\n", blob15->gateway_attributes);
			fprintf(stdout, "version %u\n", blob15->version);
			fprintf(stdout, "size %u\n", blob15->size);
			fprintf(stdout, "ts_group[0] 0x%08x\n", blob15->ts_group[0]);
			fprintf(stdout, "ts_group[1] 0x%08x\n", blob15->ts_group[1]);
			fprintf(stdout, "ts_group[2] 0x%08x\n", blob15->ts_group[2]);
			fprintf(stdout, "ts_group[3] 0x%08x\n", blob15->ts_group[3]);
			fprintf(stdout, "ts_group[4] 0x%08x\n", blob15->ts_group[4]);
			fprintf(stdout, "ts_group[5] 0x%08x\n", blob15->ts_group[5]);
			fprintf(stdout, "ts_group[6] 0x%08x\n", blob15->ts_group[6]);
			fprintf(stdout, "ts_group[7] 0x%08x\n", blob15->ts_group[7]);
			fprintf(stdout, "ssc0 0x%08x\n", blob15->ssc0);
			fprintf(stdout, "ssc1 0x%08x\n", blob15->ssc1);
			fprintf(stdout, "sscto 0x%08x\n", blob15->sscto);
			fprintf(stdout, "sspsp 0x%08x\n", blob15->sspsp);
			fprintf(stdout, "sstsa 0x%08x\n", blob15->sstsa);
			fprintf(stdout, "ssrsa 0x%08x\n", blob15->ssrsa);
			fprintf(stdout, "ssc2 0x%08x\n", blob15->ssc2);
			fprintf(stdout, "sspsp2 0x%08x\n", blob15->sspsp2);
			fprintf(stdout, "ssc3 0x%08x\n", blob15->ssc3);
			fprintf(stdout, "ssioc 0x%08x\n", blob15->ssioc);
			fprintf(stdout, "mdivc 0x%08x\n", blob15->mdivctlr);
			fprintf(stdout, "mdivr count 0x%08x\n", blob15->mdivrcnt);
			for (j = 0; j < blob15->mdivrcnt; j++)
				fprintf(stdout, "mdivr 0x%08x\n",
					ssp->ssp_prm[ssp_index].mdivr[i].mdivrs[j]);
		} else {
			blob = &ssp->ssp_blob[ssp_index][i];
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
		blob_aux = (struct ssp_aux_blob *)&(ssp->ssp_blob_ext[ssp_index][i]);
		fprintf(stdout, "aux_blob size  %u\n", blob_aux->size);
		for (j = 0; j < blob_aux->size; j += 4) {
			ptr = (uint32_t *)&(blob_aux->aux_blob[j]);
			fprintf(stdout, "aux_blob %d 0x%08x\n", j, *ptr);
		}

		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\n");
}

void ssp_print_internal(struct intel_ssp_params *ssp)
{
	struct ssp_aux_config_link *link;
	struct ssp_aux_config_sync *sync;
	struct ssp_aux_config_ext *ext;
	struct ssp_aux_config_run *run;
	struct ssp_aux_config_clk *clk;
	struct ssp_aux_config_mn *mn;
	struct ssp_aux_config_tr *tr;
	struct ssp_config_dai *dai;
	struct ssp_config_hw *hw_conf;
	uint32_t enabled;
	int i, j;

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
	fprintf(stdout, "version %u\n", dai->version);

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

		enabled = dai->aux_cfg[i].enabled;
		fprintf(stdout, "aux enabled %x\n", enabled);
		fprintf(stdout, "\n");

		mn = (struct ssp_aux_config_mn *)&(dai->aux_cfg[i].mn);
		clk = (struct ssp_aux_config_clk *)&(dai->aux_cfg[i].clk);
		tr = (struct ssp_aux_config_tr *)&(dai->aux_cfg[i].tr_start);
		tr = (struct ssp_aux_config_tr *)&(dai->aux_cfg[i].tr_stop);
		run = (struct ssp_aux_config_run *)&(dai->aux_cfg[i].run);
		sync = (struct ssp_aux_config_sync *)&(dai->aux_cfg[i].sync);
		ext = (struct ssp_aux_config_ext *)&(dai->aux_cfg[i].ext);
		link = (struct ssp_aux_config_link *)&(dai->aux_cfg[i].link);

		if (enabled & BIT(SSP_MN_DIVIDER_CONTROLS)) {
			fprintf(stdout, "aux mn m_div %u\n", mn->m_div);
			fprintf(stdout, "aux mn n_div %u\n", mn->n_div);
			fprintf(stdout, "\n");
		}

		if (enabled & BIT(SSP_DMA_CLK_CONTROLS)) {
			fprintf(stdout, "aux clk clock_warm_up %u\n", clk->clock_warm_up);
			fprintf(stdout, "aux clk mclk %u\n", clk->mclk);
			fprintf(stdout, "aux clk warm_up_ovr %u\n", clk->warm_up_ovr);
			fprintf(stdout, "aux clk clock_stop_delay %u\n", clk->clock_stop_delay);
			fprintf(stdout, "aux clk keep_running %u\n", clk->keep_running);
			fprintf(stdout, "aux clk keep_running %u\n", clk->clock_stop_ovr);
			fprintf(stdout, "\n");
		}

		if (enabled & BIT(SSP_DMA_TRANSMISSION_START)) {
			fprintf(stdout, "aux tr start sampling_frequency %u\n", tr->sampling_frequency);
			fprintf(stdout, "aux tr start bit_depth %u\n", tr->bit_depth);
			fprintf(stdout, "aux tr start channel_map %u\n", tr->channel_map);
			fprintf(stdout, "aux tr start channel_config %u\n", tr->channel_config);
			fprintf(stdout, "aux tr start interleaving_style %u\n", tr->interleaving_style);
			fprintf(stdout, "aux tr start number_of_channels %u\n", tr->number_of_channels);
			fprintf(stdout, "aux tr start valid_bit_depth %u\n", tr->valid_bit_depth);
			fprintf(stdout, "aux tr start sample_types %u\n", tr->sample_type);
			fprintf(stdout, "\n");
		}

		if (enabled & BIT(SSP_DMA_TRANSMISSION_STOP)) {
			fprintf(stdout, "aux tr start sampling_frequency %u\n", tr->sampling_frequency);
			fprintf(stdout, "aux tr start bit_depth %u\n", tr->bit_depth);
			fprintf(stdout, "aux tr start channel_map %u\n", tr->channel_map);
			fprintf(stdout, "aux tr start channel_config %u\n", tr->channel_config);
			fprintf(stdout, "aux tr start interleaving_style %u\n", tr->interleaving_style);
			fprintf(stdout, "aux tr start number_of_channels %u\n", tr->number_of_channels);
			fprintf(stdout, "aux tr start valid_bit_depth %u\n", tr->valid_bit_depth);
			fprintf(stdout, "aux tr start sample_types %u\n", tr->sample_type);
			fprintf(stdout, "\n");
		}

		if (enabled & BIT(SSP_DMA_ALWAYS_RUNNING_MODE)) {
			fprintf(stdout, "aux run always_run %u\n", run->always_run);
			fprintf(stdout, "\n");
		}

		if (enabled & BIT(SSP_DMA_SYNC_DATA)) {
			fprintf(stdout, "aux sync sync_denominator %u\n", sync->sync_denominator);
			fprintf(stdout, "aux sync count %u\n", sync->count);

			for (j = 0; j < sync->count; j++) {
				fprintf(stdout, "aux sync node_id %u\n", sync->nodes[j].node_id);
				fprintf(stdout, "aux sync sampling_rate %u\n", sync->nodes[j].sampling_rate);
			}

			fprintf(stdout, "\n");
		}

		if (enabled & BIT(SSP_DMA_CLK_CONTROLS_EXT)) {
			fprintf(stdout, "aux ext mclk_policy_override %u\n", ext->mclk_policy_override);
			fprintf(stdout, "aux ext mclk_always_running %u\n", ext->mclk_always_running);
			fprintf(stdout, "aux ext mclk_starts_on_gtw_init %u\n", ext->mclk_starts_on_gtw_init);
			fprintf(stdout, "aux ext mclk_starts_on_run %u\n", ext->mclk_starts_on_run);
			fprintf(stdout, "aux ext mclk_starts_on_pause %u\n", ext->mclk_starts_on_pause);
			fprintf(stdout, "aux ext mclk_stops_on_pause %u\n", ext->mclk_stops_on_pause);
			fprintf(stdout, "aux ext mclk_stops_on_reset %u\n", ext->mclk_stops_on_reset);
			fprintf(stdout, "aux ext bclk_policy_override %u\n", ext->bclk_policy_override);
			fprintf(stdout, "aux ext bclk_always_running %u\n", ext->bclk_always_running);
			fprintf(stdout, "aux ext bclk_starts_on_gtw_init %u\n", ext->bclk_starts_on_gtw_init);
			fprintf(stdout, "aux ext bclk_starts_on_run %u\n", ext->bclk_starts_on_run);
			fprintf(stdout, "aux ext bclk_starts_on_pause %u\n", ext->bclk_starts_on_pause);
			fprintf(stdout, "aux ext bclk_stops_on_pause %u\n", ext->bclk_stops_on_pause);
			fprintf(stdout, "aux ext bclk_stops_on_reset %u\n", ext->bclk_stops_on_reset);
			fprintf(stdout, "aux ext sync_policy_override %u\n", ext->sync_policy_override);
			fprintf(stdout, "aux ext sync_always_running %u\n", ext->sync_always_running);
			fprintf(stdout, "aux ext sync_starts_on_gtw_init %u\n", ext->sync_starts_on_gtw_init);
			fprintf(stdout, "aux ext sync_starts_on_run %u\n", ext->sync_starts_on_run);
			fprintf(stdout, "aux ext sync_starts_on_pause %u\n", ext->sync_starts_on_pause);
			fprintf(stdout, "aux ext sync_stops_on_pause %u\n", ext->sync_stops_on_pause);
			fprintf(stdout, "aux ext sync_stops_on_reset %u\n", ext->sync_stops_on_reset);
			fprintf(stdout, "\n");
		}

		if (enabled & BIT(SSP_LINK_CLK_SOURCE)) {
			fprintf(stdout, "aux link clock_source %u\n", link->clock_source);
			fprintf(stdout, "\n");
		}
	}

	fprintf(stdout, "\n");
}

#else /* NHLT_DEBUG */
void ssp_print_internal(struct intel_ssp_params *ssp) {}
void ssp_print_calculated(struct intel_ssp_params *ssp) {}
#endif
