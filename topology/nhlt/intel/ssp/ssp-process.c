// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>
#include "../intel-nhlt.h"
#include "../../nhlt.h"
#include "ssp-process.h"
#include "ssp-intel.h"
#include "ssp-internal.h"
#include "ssp-debug.h"

static int popcount(uint32_t value)
{
	int bits_set = 0;

	while (value) {
		bits_set += value & 1;
		value >>= 1;
	}

	return bits_set;
}

static int ssp_calculate_intern(struct intel_nhlt_params *nhlt, int hwi)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;
	uint32_t active_tx_slots = 2;
	uint32_t active_rx_slots = 2;
	uint32_t inverted_frame = 0;
	uint32_t inverted_bclk = 0;
	uint32_t frame_end_padding;
	uint32_t total_sample_size;
	uint32_t slot_end_padding;
	bool start_delay = false;
	uint32_t frame_len = 0;
	uint32_t sample_width;
	uint32_t end_padding;
	uint32_t data_size;
	uint32_t bdiv_min;
	bool cfs = false;
	uint32_t clk_div;
	uint32_t bdiv;
	uint32_t tft;
	uint32_t rft;
	int di;
	int i, j;

	if (!ssp)
		return -EINVAL;

	di = ssp->ssp_count;

	/* should be eventually the lp_mode defined in pipeline */
	ssp->ssp_blob[di][hwi].gateway_attributes = 0;

	for (j = 0; j < SSP_TDM_MAX_SLOT_MAP_COUNT; j++) {
		for (i = 0; i < ssp->ssp_prm.hw_cfg[hwi].tdm_slots; i++)
			ssp->ssp_blob[di][hwi].ts_group[j] |= (i << (i * 4));
		for (; i < SSP_TDM_MAX_SLOT_MAP_COUNT; i++)
			ssp->ssp_blob[di][hwi].ts_group[j] |= (0xF << (i * 4));
	}

	/* reset SSP settings */
	/* sscr0 dynamic settings are DSS, EDSS, SCR, FRDC, ECS */
	ssp->ssp_blob[di][hwi].ssc0 = SSCR0_PSP | SSCR0_RIM | SSCR0_TIM;

	/* sscr1 dynamic settings are SFRMDIR, SCLKDIR, SCFR */
	ssp->ssp_blob[di][hwi].ssc1 = SSCR1_TTE | SSCR1_TTELP | SSCR1_TRAIL | SSCR1_RSRE |
		SSCR1_TSRE;

	/* sscr2 dynamic setting is LJDFD */
	ssp->ssp_blob[di][hwi].ssc2 = SSCR2_SDFD | SSCR2_TURM1;

	/* sscr3 dynamic settings are TFT, RFT */
	ssp->ssp_blob[di][hwi].ssc3 = 0;

	/* sspsp dynamic settings are SCMODE, SFRMP, DMYSTRT, SFRMWDTH */
	ssp->ssp_blob[di][hwi].sspsp = 0;

	/* sspsp2 no dynamic setting */
	ssp->ssp_blob[di][hwi].sspsp2 = 0x0;

	/* ssioc dynamic setting is SFCR */
	ssp->ssp_blob[di][hwi].ssioc = SSIOC_SCOE;

	/* ssto no dynamic setting */
	ssp->ssp_blob[di][hwi].sscto = 0x0;

	/* sstsa dynamic setting is TTSA, default 2 slots */
	ssp->ssp_blob[di][hwi].sstsa = SSTSA_SSTSA(ssp->ssp_prm.hw_cfg[hwi].tx_slots);

	/* ssrsa dynamic setting is RTSA, default 2 slots */
	ssp->ssp_blob[di][hwi].ssrsa = SSRSA_SSRSA(ssp->ssp_prm.hw_cfg[hwi].rx_slots);

	switch (ssp->ssp_prm.hw_cfg[hwi].format & SSP_FMT_CLOCK_PROVIDER_MASK) {
	case SSP_FMT_CBP_CFP:
		ssp->ssp_blob[di][hwi].ssc1 |= SSCR1_SCLKDIR | SSCR1_SFRMDIR;
		break;
	case SSP_FMT_CBC_CFC:
		ssp->ssp_blob[di][hwi].ssc1 |= SSCR1_SCFR;
		cfs = true;
		break;
	case SSP_FMT_CBP_CFC:
		ssp->ssp_blob[di][hwi].ssc1 |= SSCR1_SCLKDIR;
		/* FIXME: this mode has not been tested */

		cfs = true;
		break;
	case SSP_FMT_CBC_CFP:
		ssp->ssp_blob[di][hwi].ssc1 |= SSCR1_SCFR | SSCR1_SFRMDIR;
		/* FIXME: this mode has not been tested */
		break;
	default:
		fprintf(stderr, "ssp_calculate(): format & PROVIDER_MASK EINVAL\n");
		return -EINVAL;
	}

	/* clock signal polarity */
	switch (ssp->ssp_prm.hw_cfg[hwi].format & SSP_FMT_INV_MASK) {
	case SSP_FMT_NB_NF:
		break;
	case SSP_FMT_NB_IF:
		inverted_frame = 1; /* handled later with format */
		break;
	case SSP_FMT_IB_IF:
		inverted_bclk = 1; /* handled later with bclk idle */
		inverted_frame = 1; /* handled later with format */
		break;
	case SSP_FMT_IB_NF:
		inverted_bclk = 1; /* handled later with bclk idle */
		break;
	default:
		fprintf(stderr, "ssp_calculate: format & INV_MASK EINVAL\n");
		return -EINVAL;
	}

	/* supporting bclk idle state */
	if (ssp->ssp_prm.clks_control &
		SSP_INTEL_CLKCTRL_BCLK_IDLE_HIGH) {
		/* bclk idle state high */
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_SCMODE((inverted_bclk ^ 0x3) & 0x3);
	} else {
		/* bclk idle state low */
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_SCMODE(inverted_bclk);
	}

	ssp->ssp_blob[di][hwi].ssc0 |= SSCR0_MOD | SSCR0_ACS;

	/* Additional hardware settings */

	/* Receiver Time-out Interrupt Disabled/Enabled */
	ssp->ssp_blob[di][hwi].ssc1 |= (ssp->ssp_prm.quirks & SSP_INTEL_QUIRK_TINTE) ?
		SSCR1_TINTE : 0;

	/* Peripheral Trailing Byte Interrupts Disable/Enable */
	ssp->ssp_blob[di][hwi].ssc1 |= (ssp->ssp_prm.quirks & SSP_INTEL_QUIRK_PINTE) ?
		SSCR1_PINTE : 0;

	/* Enable/disable internal loopback. Output of transmit serial
	 * shifter connected to input of receive serial shifter, internally.
	 */
	ssp->ssp_blob[di][hwi].ssc1 |= (ssp->ssp_prm.quirks & SSP_INTEL_QUIRK_LBM) ?
		SSCR1_LBM : 0;

	/* Transmit data are driven at the same/opposite clock edge specified
	 * in SSPSP.SCMODE[1:0]
	 */
	ssp->ssp_blob[di][hwi].ssc2 |= (ssp->ssp_prm.quirks & SSP_INTEL_QUIRK_SMTATF) ?
		SSCR2_SMTATF : 0;

	/* Receive data are sampled at the same/opposite clock edge specified
	 * in SSPSP.SCMODE[1:0]
	 */
	ssp->ssp_blob[di][hwi].ssc2 |= (ssp->ssp_prm.quirks & SSP_INTEL_QUIRK_MMRATF) ?
		SSCR2_MMRATF : 0;

	/* Enable/disable the fix for PSP consumer mode TXD wait for frame
	 * de-assertion before starting the second channel
	 */
	ssp->ssp_blob[di][hwi].ssc2 |= (ssp->ssp_prm.quirks & SSP_INTEL_QUIRK_PSPSTWFDFD) ?
		SSCR2_PSPSTWFDFD : 0;

	/* Enable/disable the fix for PSP provider mode FSRT with dummy stop &
	 * frame end padding capability
	 */
	ssp->ssp_blob[di][hwi].ssc2 |= (ssp->ssp_prm.quirks & SSP_INTEL_QUIRK_PSPSRWFDFD) ?
		SSCR2_PSPSRWFDFD : 0;

	if (!ssp->ssp_prm.hw_cfg[hwi].mclk_rate) {
		fprintf(stderr, "ssp_calculate(): invalid MCLK = %u \n",
			ssp->ssp_prm.hw_cfg[hwi].mclk_rate);
		return -EINVAL;
	}

	if (!ssp->ssp_prm.hw_cfg[hwi].bclk_rate ||
	    ssp->ssp_prm.hw_cfg[hwi].bclk_rate > ssp->ssp_prm.hw_cfg[hwi].mclk_rate) {
		fprintf(stderr, "ssp_calculate(): BCLK %u Hz = 0 or > MCLK %u Hz\n",
			ssp->ssp_prm.hw_cfg[hwi].bclk_rate, ssp->ssp_prm.hw_cfg[hwi].mclk_rate);
		return -EINVAL;
	}

	/* calc frame width based on BCLK and rate - must be divisible */
	if (ssp->ssp_prm.hw_cfg[hwi].bclk_rate % ssp->ssp_prm.hw_cfg[hwi].fsync_rate) {
		fprintf(stderr, "ssp_calculate(): BCLK %u is not divisible by rate %u\n",
			ssp->ssp_prm.hw_cfg[hwi].bclk_rate, ssp->ssp_prm.hw_cfg[hwi].fsync_rate);
		return -EINVAL;
	}

	/* must be enough BCLKs for data */
	bdiv = ssp->ssp_prm.hw_cfg[hwi].bclk_rate / ssp->ssp_prm.hw_cfg[hwi].fsync_rate;
	if (bdiv < ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width * ssp->ssp_prm.hw_cfg[hwi].tdm_slots) {
		fprintf(stderr, "ssp_calculate(): not enough BCLKs need %u\n",
			ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width *
			ssp->ssp_prm.hw_cfg[hwi].tdm_slots);
		return -EINVAL;
	}

	/* tdm_slot_width must be <= 38 for SSP */
	if (ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width > 38) {
		fprintf(stderr, "ssp_calculate(): tdm_slot_width %u > 38\n",
			ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width);
		return -EINVAL;
	}

	bdiv_min = ssp->ssp_prm.hw_cfg[hwi].tdm_slots *
		   (ssp->ssp_prm.tdm_per_slot_padding_flag ?
		    ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width : ssp->ssp_prm.sample_valid_bits);
	if (bdiv < bdiv_min) {
		fprintf(stderr, "ssp_calculate(): bdiv(%u) < bdiv_min(%u)\n",
			bdiv, bdiv_min);
		return -EINVAL;
	}

	frame_end_padding = bdiv - bdiv_min;
	if (frame_end_padding > SSPSP2_FEP_MASK) {
		fprintf(stderr, "ssp_calculate(): frame_end_padding too big: %u\n",
			frame_end_padding);
		return -EINVAL;
	}

	/* format */
	switch (ssp->ssp_prm.hw_cfg[hwi].format & SSP_FMT_FORMAT_MASK) {
	case SSP_FMT_I2S:

		start_delay = true;

		ssp->ssp_blob[di][hwi].ssc0 |= SSCR0_FRDC(ssp->ssp_prm.hw_cfg[hwi].tdm_slots);

		if (bdiv % 2) {
			fprintf(stderr, "ssp_calculate(): bdiv %u is not divisible by 2\n",
				bdiv);
			return -EINVAL;
		}

		/* set asserted frame length to half frame length */
		frame_len = bdiv / 2;

		/*
		 * handle frame polarity, I2S default is falling/active low,
		 * non-inverted(inverted_frame=0) -- active low(SFRMP=0),
		 * inverted(inverted_frame=1) -- rising/active high(SFRMP=1),
		 * so, we should set SFRMP to inverted_frame.
		 */
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_SFRMP(inverted_frame);

		/*
		 *  for I2S/LEFT_J, the padding has to happen at the end
		 * of each slot
		 */
		if (frame_end_padding % 2) {
			fprintf(stderr, "ssp_calculate():frame_end_padding %u not divisible by 2\n",
				frame_end_padding);
			return -EINVAL;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > SSP_INTEL_SLOT_PADDING_MAX) {
			/* too big padding */
			fprintf(stderr, "ssp_calculate(): slot_end_padding > %d\n",
				SSP_INTEL_SLOT_PADDING_MAX);
			return -EINVAL;
		}

		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_DMYSTOP(slot_end_padding);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_EDMYSTOP(slot_end_padding);

		break;

	case SSP_FMT_LEFT_J:

		/* default start_delay value is set to false */

		ssp->ssp_blob[di][hwi].ssc0 |= SSCR0_FRDC(ssp->ssp_prm.hw_cfg[hwi].tdm_slots);

		/* LJDFD enable */
		ssp->ssp_blob[di][hwi].ssc2 &= ~SSCR2_LJDFD;

		if (bdiv % 2) {
			fprintf(stderr, "ssp_calculate(): bdiv %u is not divisible by 2\n",
				bdiv);
			return -EINVAL;
		}

		/* set asserted frame length to half frame length */
		frame_len = bdiv / 2;

		/*
		 * handle frame polarity, LEFT_J default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_SFRMP(!inverted_frame ? 1 : 0);

		/*
		 *  for I2S/LEFT_J, the padding has to happen at the end
		 * of each slot
		 */
		if (frame_end_padding % 2) {
			fprintf(stderr, "ssp_set_config(): frame padding %u not divisible by 2\n",
				frame_end_padding);
			return -EINVAL;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > 15) {
			/* can't handle padding over 15 bits */
			fprintf(stderr, "ssp_set_config(): slot_end_padding %u > 15 bits\n",
				slot_end_padding);
			return -EINVAL;
		}

		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_DMYSTOP(slot_end_padding);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_EDMYSTOP(slot_end_padding);

		break;
	case SSP_FMT_DSP_A:

		start_delay = true;

		/* fallthrough */

	case SSP_FMT_DSP_B:

		/* default start_delay value is set to false */

		ssp->ssp_blob[di][hwi].ssc0 |= SSCR0_MOD |
			SSCR0_FRDC(ssp->ssp_prm.hw_cfg[hwi].tdm_slots);

		/* set asserted frame length */
		frame_len = 1; /* default */

		if (cfs && ssp->ssp_prm.frame_pulse_width > 0 &&
		    ssp->ssp_prm.frame_pulse_width <=
		    SSP_INTEL_FRAME_PULSE_WIDTH_MAX) {
			frame_len = ssp->ssp_prm.frame_pulse_width;
		}

		/* frame_pulse_width must less or equal 38 */
		if (ssp->ssp_prm.frame_pulse_width >
			SSP_INTEL_FRAME_PULSE_WIDTH_MAX) {
			fprintf(stderr, "ssp_set_config(): frame_pulse_width > %d\n",
				SSP_INTEL_FRAME_PULSE_WIDTH_MAX);
			return -EINVAL;
		}
		/*
		 * handle frame polarity, DSP_B default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_SFRMP(!inverted_frame ? 1 : 0);

		active_tx_slots = popcount(ssp->ssp_prm.hw_cfg[hwi].tx_slots);
		active_rx_slots = popcount(ssp->ssp_prm.hw_cfg[hwi].rx_slots);

		/*
		 * handle TDM mode, TDM mode has padding at the end of
		 * each slot. The amount of padding is equal to result of
		 * subtracting slot width and valid bits per slot.
		 */
		if (ssp->ssp_prm.tdm_per_slot_padding_flag) {
			frame_end_padding = bdiv - ssp->ssp_prm.hw_cfg[hwi].tdm_slots *
				ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width;

			slot_end_padding = ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width -
				ssp->ssp_prm.sample_valid_bits;

			if (slot_end_padding >
				SSP_INTEL_SLOT_PADDING_MAX) {
				fprintf(stderr, "ssp_set_config(): slot_end_padding > %d\n",
					SSP_INTEL_SLOT_PADDING_MAX);
				return -EINVAL;
			}

			ssp->ssp_blob[di][hwi].sspsp |= SSPSP_DMYSTOP(slot_end_padding);
			slot_end_padding >>= SSPSP_DMYSTOP_BITS;
			ssp->ssp_blob[di][hwi].sspsp |= SSPSP_EDMYSTOP(slot_end_padding);
		}

		ssp->ssp_blob[di][hwi].sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

		break;
	default:
		fprintf(stderr, "ssp_set_config(): invalid format 0x%04x\n",
			ssp->ssp_prm.hw_cfg[hwi].format);
		return -EINVAL;
	}

	if (start_delay)
		ssp->ssp_blob[di][hwi].sspsp |= SSPSP_FSRT;

	ssp->ssp_blob[di][hwi].sspsp |= SSPSP_SFRMWDTH(frame_len);

	data_size = ssp->ssp_prm.sample_valid_bits;

	if (data_size > 16)
		ssp->ssp_blob[di][hwi].ssc0 |= (SSCR0_EDSS | SSCR0_DSIZE(data_size - 16));
	else
		ssp->ssp_blob[di][hwi].ssc0 |= SSCR0_DSIZE(data_size);

	end_padding = 0;
	total_sample_size = ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width *
		ssp->ssp_prm.hw_cfg[hwi].tdm_slots;
	while (ssp->ssp_prm.io_clk % ((total_sample_size + end_padding) *
				      ssp->ssp_prm.hw_cfg[hwi].fsync_rate)) {
		if (++end_padding >= 256)
			break;
	}

	if (end_padding >= 256)
		return -EINVAL;

	/* calc scr divisor */
	clk_div = ssp->ssp_prm.io_clk / ((total_sample_size + end_padding) *
					 ssp->ssp_prm.hw_cfg[hwi].fsync_rate);
	if (clk_div >= 4095)
		return -EINVAL;

	ssp->ssp_blob[di][hwi].ssc0 |= SSCR0_SCR(clk_div - 1);

	/* setting TFT and RFT */
	switch (ssp->ssp_prm.sample_valid_bits) {
	case 16:
		/* use 2 bytes for each slot */
		sample_width = 2;
		break;
	case 24:
	case 32:
		/* use 4 bytes for each slot */
		sample_width = 4;
		break;
	default:
		fprintf(stderr, "ssp_set_config(): sample_valid_bits %u\n",
			ssp->ssp_prm.sample_valid_bits);
		return -EINVAL;
	}

	tft = MIN(SSP_FIFO_DEPTH - SSP_FIFO_WATERMARK,
		  sample_width * active_tx_slots);
	rft = MIN(SSP_FIFO_DEPTH - SSP_FIFO_WATERMARK,
		  sample_width * active_rx_slots);

	ssp->ssp_blob[di][hwi].ssc3 |= SSCR3_TX(tft) | SSCR3_RX(rft);

	/* calc mn divisor */
	if (ssp->ssp_prm.io_clk % ssp->ssp_prm.hw_cfg[hwi].mclk_rate) {
		fprintf(stderr, "ssp_set_config(): io_clk not divisible with mclk\n");
		return -EINVAL;
	}

	clk_div = ssp->ssp_prm.io_clk / ssp->ssp_prm.hw_cfg[hwi].mclk_rate;
	if (clk_div > 1)
		clk_div -= 2;
	else
		clk_div = 0xFFF; /* bypass clk divider */

	ssp->ssp_blob[di][hwi].mdivr = clk_div;
	/* clock will always go through the divider */
	ssp->ssp_blob[di][hwi].ssc0 |= SSCR0_ECS;
	/* enable divider for this clock id */
	ssp->ssp_blob[di][hwi].mdivc |= BIT(ssp->ssp_prm.mclk_id);
	/* set mclk source always for audio cardinal clock */
	ssp->ssp_blob[di][hwi].mdivc |= MCDSS(SSP_CLOCK_AUDIO_CARDINAL);
	/* set bclk source for audio cardinal clock */
	ssp->ssp_blob[di][hwi].mdivc |= MNDSS(SSP_CLOCK_AUDIO_CARDINAL);

	return 0;
}

int ssp_calculate(struct intel_nhlt_params *nhlt)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;
	int i;

	if (!ssp)
		return -EINVAL;

	ssp_print_internal(ssp);

	/* calculate blob for every hw config */
	for (i = 0; i < ssp->ssp_hw_config_count[ssp->ssp_count]; i++)
		ssp_calculate_intern(nhlt, i);

	ssp->ssp_count++;

	ssp_print_calculated(ssp);

	return 0;
}

int ssp_get_dir(struct intel_nhlt_params *nhlt, int dai_index, uint8_t *dir)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;

	if (!ssp)
		return -EINVAL;

	*dir = ssp->ssp_prm.direction;

	return 0;
}

int ssp_get_params(struct intel_nhlt_params *nhlt, int dai_index, uint32_t *virtualbus_id,
		   uint32_t *formats_count)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;

	if (!ssp)
		return -EINVAL;

	*virtualbus_id = ssp->ssp_dai_index[dai_index];
	*formats_count = ssp->ssp_hw_config_count[dai_index];

	return 0;
}

int ssp_get_hw_params(struct intel_nhlt_params *nhlt, int hw_index, uint32_t *sample_rate,
		      uint16_t *channel_count, uint32_t *bits_per_sample)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;

	if (!ssp)
		return -EINVAL;

	*channel_count = ssp->ssp_prm.hw_cfg[hw_index].tdm_slots;
	*sample_rate = ssp->ssp_prm.hw_cfg[hw_index].fsync_rate;
	*bits_per_sample = ssp->ssp_prm.hw_cfg[hw_index].tdm_slot_width;

	return 0;
}

/*
 * Build ssp vendor blob from calculated parameters.
 *
 * Supposed to be called after all ssp DAIs are parsed from topology and the final nhlt blob is
 * generated.
 */
int ssp_get_vendor_blob_size(struct intel_nhlt_params *nhlt, size_t *size)
{
	*size = sizeof(struct ssp_intel_config_data);

	return 0;
}

int ssp_get_vendor_blob_count(struct intel_nhlt_params *nhlt)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;

	if (!ssp || !ssp->ssp_count)
		return -EINVAL;

	return ssp->ssp_count;
}

/* Get the size of dynamic vendor blob to reserve proper amount of memory */
int ssp_get_vendor_blob(struct intel_nhlt_params *nhlt, uint8_t *vendor_blob,
			int dai_index, int hw_config_index)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;

	if (!ssp)
		return -EINVAL;

	/* top level struct */
	memcpy(vendor_blob, &ssp->ssp_blob[dai_index][hw_config_index],
	       sizeof(struct ssp_intel_config_data));

	return 0;
}

int ssp_set_params(struct intel_nhlt_params *nhlt, const char *dir, int dai_index, int io_clk,
		   int bclk_delay, int sample_bits, int mclk_id, int clks_control,
		   int frame_pulse_width, const char *tdm_padding_per_slot, const char *quirks)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;

	if (!ssp)
		return -EINVAL;

	if (dir) {
		if (!strcmp(dir, "playback"))
			ssp->ssp_prm.direction = NHLT_ENDPOINT_DIRECTION_RENDER;
		else if (!strcmp(dir, "capture"))
			ssp->ssp_prm.direction = NHLT_ENDPOINT_DIRECTION_CAPTURE;
		else if (!strcmp(dir, "duplex"))
			ssp->ssp_prm.direction = NHLT_ENDPOINT_DIRECTION_FEEDBACK_FOR_RENDER + 1;
		else
			return -EINVAL;
	}
	ssp->ssp_dai_index[ssp->ssp_count] = dai_index;
	ssp->ssp_prm.io_clk = io_clk;
	ssp->ssp_prm.bclk_delay = bclk_delay;
	ssp->ssp_prm.sample_valid_bits = sample_bits;
	ssp->ssp_prm.mclk_id = mclk_id;
	ssp->ssp_prm.clks_control = clks_control;
	ssp->ssp_prm.frame_pulse_width = frame_pulse_width;
	if (tdm_padding_per_slot && !strcmp(tdm_padding_per_slot, "true"))
		ssp->ssp_prm.tdm_per_slot_padding_flag = 1;
	else
		ssp->ssp_prm.tdm_per_slot_padding_flag = 0;
	if (quirks && !strcmp(quirks, "lbm_mode"))
		ssp->ssp_prm.quirks = 64; /* 1 << 6 */
	else
		ssp->ssp_prm.quirks = 0;

	/* reset hw config count for this ssp instance */
	ssp->ssp_hw_config_count[ssp->ssp_count] = 0;

	return 0;
}

int ssp_hw_set_params(struct intel_nhlt_params *nhlt, const char *format, const char *mclk,
		      const char *bclk, const char *bclk_invert, const char *fsync,
		      const char *fsync_invert, int mclk_freq, int bclk_freq, int fsync_freq,
		      int tdm_slots, int tdm_slot_width, int tx_slots, int rx_slots)
{
	struct intel_ssp_params *ssp = (struct intel_ssp_params *)nhlt->ssp_params;
	uint32_t hwi;

	if (!ssp)
		return -EINVAL;

	/* check that the strings are defined ?*/

	/* compose format out of clock related string variables */
	hwi = ssp->ssp_hw_config_count[ssp->ssp_count];

	if (!strcmp(format, "I2S")) {
		ssp->ssp_prm.hw_cfg[hwi].format = SSP_FMT_I2S;
	} else if (!strcmp(format, "RIGHT_J")) {
		ssp->ssp_prm.hw_cfg[hwi].format = SSP_FMT_RIGHT_J;
	} else if (!strcmp(format, "LEFT_J")) {
		ssp->ssp_prm.hw_cfg[hwi].format = SSP_FMT_LEFT_J;
	} else if (!strcmp(format, "DSP_A")) {
		ssp->ssp_prm.hw_cfg[hwi].format = SSP_FMT_DSP_A;
	} else if (!strcmp(format, "DSP_B")) {
		ssp->ssp_prm.hw_cfg[hwi].format = SSP_FMT_DSP_B;
	} else {
		fprintf(stderr, "no valid format specified for ssp: %s\n", format);
		return -EINVAL;
	}

	/* clock directions wrt codec */
	if (bclk && !strcmp(bclk, "coded_provider")) {
		/* codec is bclk provider */
		if (fsync && !strcmp(fsync, "coded_provider"))
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_CBP_CFP;
		else
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_CBP_CFC;
	} else {
		/* codec is bclk consumer */
		if (fsync && !strcmp(fsync, "coded_provider"))
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_CBC_CFP;
		else
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_CBC_CFC;
	}

	/* inverted clocks ? */
	if (bclk_invert && !strcmp(bclk_invert, "true")) {
		if (fsync_invert && !strcmp(fsync_invert, "true"))
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_IB_IF;
		else
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_IB_NF;
	} else {
		if (fsync_invert && !strcmp(fsync_invert, "true"))
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_NB_IF;
		else
			ssp->ssp_prm.hw_cfg[hwi].format |= SSP_FMT_NB_NF;
	}

	ssp->ssp_prm.hw_cfg[hwi].mclk_rate = mclk_freq;
	ssp->ssp_prm.hw_cfg[hwi].bclk_rate = bclk_freq;
	ssp->ssp_prm.hw_cfg[hwi].fsync_rate = fsync_freq;
	ssp->ssp_prm.hw_cfg[hwi].tdm_slots = tdm_slots;
	ssp->ssp_prm.hw_cfg[hwi].tdm_slot_width = tdm_slot_width;
	ssp->ssp_prm.hw_cfg[hwi].tx_slots = tx_slots;
	ssp->ssp_prm.hw_cfg[hwi].rx_slots = rx_slots;

	ssp->ssp_hw_config_count[ssp->ssp_count]++;

	return 0;
}

/* init ssp parameters, should be called before parsing dais */
int ssp_init_params(struct intel_nhlt_params *nhlt)
{
	struct intel_ssp_params *ssp;
	int i;

	ssp = calloc(1, sizeof(struct intel_ssp_params));
	if (!ssp)
		return -EINVAL;

	nhlt->ssp_params = ssp;
	ssp->ssp_count = 0;

	for (i = 0; i < SSP_MAX_DAIS; i++)
		ssp->ssp_hw_config_count[i] = 0;

	return 0;
}
