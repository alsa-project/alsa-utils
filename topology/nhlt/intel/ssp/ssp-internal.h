// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __SSP_MACROS_H
#define __SSP_MACROS_H

#include "ssp-intel.h"

#define SSP_MAX_DAIS 8
#define SSP_MAX_HW_CONFIG 8
#define SSP_TDM_MAX_SLOT_MAP_COUNT 8

struct ssp_aux_config_mn {
	uint32_t m_div;
	uint32_t n_div;
};

struct ssp_aux_config_clk {
	uint32_t clock_warm_up;
	uint32_t mclk;
	uint32_t warm_up_ovr;
	uint32_t clock_stop_delay;
	uint32_t keep_running;
	uint32_t clock_stop_ovr;
};

struct ssp_aux_config_tr {
	uint32_t sampling_frequency;
	uint32_t bit_depth;
	uint32_t channel_map;
	uint32_t channel_config;
	uint32_t interleaving_style;
	uint32_t number_of_channels;
	uint32_t valid_bit_depth;
	uint32_t sample_type;
};

struct ssp_aux_config_run {
	uint32_t always_run;
};

struct ssp_aux_config_node {
	uint32_t node_id;
	uint32_t sampling_rate;
};

struct ssp_aux_config_sync {
	uint32_t sync_denominator;
	uint32_t count;
	struct ssp_aux_config_node nodes[SSP_MAX_DAIS];
};

struct ssp_aux_config_ext {
	uint32_t mclk_policy_override;
	uint32_t mclk_always_running;
	uint32_t mclk_starts_on_gtw_init;
	uint32_t mclk_starts_on_run;
	uint32_t mclk_starts_on_pause;
	uint32_t mclk_stops_on_pause;
	uint32_t mclk_stops_on_reset;
	uint32_t bclk_policy_override;
	uint32_t bclk_always_running;
	uint32_t bclk_starts_on_gtw_init;
	uint32_t bclk_starts_on_run;
	uint32_t bclk_starts_on_pause;
	uint32_t bclk_stops_on_pause;
	uint32_t bclk_stops_on_reset;
	uint32_t sync_policy_override;
	uint32_t sync_always_running;
	uint32_t sync_starts_on_gtw_init;
	uint32_t sync_starts_on_run;
	uint32_t sync_starts_on_pause;
	uint32_t sync_stops_on_pause;
	uint32_t sync_stops_on_reset;
};

struct ssp_aux_config_link {
	uint32_t clock_source;
};

struct ssp_config_aux {
	/* bits set for found aux structs */
	uint32_t enabled;
	struct ssp_aux_config_mn mn;
	struct ssp_aux_config_clk clk;
	struct ssp_aux_config_tr tr_start;
	struct ssp_aux_config_tr tr_stop;
	struct ssp_aux_config_run run;
	struct ssp_aux_config_sync sync;
	struct ssp_aux_config_ext ext;
	struct ssp_aux_config_link link;
};

struct ssp_aux_blob {
	uint32_t size;
	uint8_t aux_blob[256];
};

struct ssp_config_mdivr {
	uint32_t count;
	uint32_t mdivrs[8];
};

/* structs for gathering the ssp parameters from topology */
struct ssp_config_hw {
	uint32_t mclk_rate;
	uint32_t bclk_rate;
	uint32_t fsync_rate;
	uint32_t tdm_slots;
	uint32_t tdm_slot_width;
	uint32_t tx_slots;
	uint32_t rx_slots;
	uint32_t format;
};

struct ssp_config_dai {
	uint32_t io_clk;
	uint32_t dai_index;
	uint16_t mclk_id;
	uint32_t sample_valid_bits;
	uint32_t mclk_direction;
	uint16_t frame_pulse_width;
	uint16_t tdm_per_slot_padding_flag;
	uint32_t clks_control;
	uint32_t quirks;
	uint32_t bclk_delay;
	uint8_t direction;
	uint32_t version;
	struct ssp_config_hw hw_cfg[SSP_MAX_HW_CONFIG];
	struct ssp_config_aux aux_cfg[SSP_MAX_HW_CONFIG];
	struct ssp_config_mdivr mdivr[SSP_MAX_HW_CONFIG];
};

struct intel_ssp_params {
	/* structs to gather ssp params before calculations */
	struct ssp_config_dai ssp_prm[SSP_MAX_DAIS];
	uint32_t ssp_dai_index[SSP_MAX_DAIS];
	uint32_t ssp_hw_config_count[SSP_MAX_DAIS];
	int ssp_count;

	/* ssp vendor blob structs */
	struct ssp_intel_config_data ssp_blob[SSP_MAX_DAIS][SSP_MAX_HW_CONFIG];
	struct ssp_intel_config_data_1_5 ssp_blob_1_5[SSP_MAX_DAIS][SSP_MAX_HW_CONFIG];
	struct ssp_aux_blob ssp_blob_ext[SSP_MAX_DAIS][SSP_MAX_HW_CONFIG];
};

#define SSP_MN_DIVIDER_CONTROLS                 0
#define SSP_DMA_CLK_CONTROLS                    1
#define SSP_DMA_TRANSMISSION_START              2
#define SSP_DMA_TRANSMISSION_STOP               3
#define SSP_DMA_ALWAYS_RUNNING_MODE             4
#define SSP_DMA_SYNC_DATA                       5
#define SSP_DMA_CLK_CONTROLS_EXT                6
#define SSP_LINK_CLK_SOURCE                     7
/* officially "undefined" node for topology parsing */
#define SSP_DMA_SYNC_NODE                       32

#define SSP_CLOCK_XTAL_OSCILLATOR       0x0
#define SSP_CLOCK_AUDIO_CARDINAL        0x1
#define SSP_CLOCK_PLL_FIXED             0x2

#define MCDSS(x)        SET_BITS(17, 16, x)
#define MNDSS(x)        SET_BITS(21, 20, x)

#define SSP_FMT_I2S         1 /**< I2S mode */
#define SSP_FMT_RIGHT_J     2 /**< Right Justified mode */
#define SSP_FMT_LEFT_J      3 /**< Left Justified mode */
#define SSP_FMT_DSP_A       4 /**< L data MSB after FRM LRC */
#define SSP_FMT_DSP_B       5 /**< L data MSB during FRM LRC */
#define SSP_FMT_PDM         6 /**< Pulse density modulation */

#define SSP_FMT_CONT        (1 << 4) /**< continuous clock */
#define SSP_FMT_GATED       (0 << 4) /**< clock is gated */

#define SSP_FMT_NB_NF       (0 << 8) /**< normal bit clock + frame */
#define SSP_FMT_NB_IF       (2 << 8) /**< normal BCLK + inv FRM */
#define SSP_FMT_IB_NF       (3 << 8) /**< invert BCLK + nor FRM */
#define SSP_FMT_IB_IF       (4 << 8) /**< invert BCLK + FRM */

#define SSP_FMT_CBP_CFP     (0 << 12) /**< codec bclk provider & frame provider */
#define SSP_FMT_CBC_CFP     (2 << 12) /**< codec bclk consumer & frame provider */
#define SSP_FMT_CBP_CFC     (3 << 12) /**< codec bclk provider & frame consumer */
#define SSP_FMT_CBC_CFC     (4 << 12) /**< codec bclk consumer & frame consumer */

#define SSP_FMT_FORMAT_MASK         0x000f
#define SSP_FMT_CLOCK_MASK          0x00f0
#define SSP_FMT_INV_MASK            0x0f00
#define SSP_FMT_CLOCK_PROVIDER_MASK 0xf000

/* SSCR0 bits */
#define SSCR0_DSIZE(x)	SET_BITS(3, 0, (x) - 1)
#define SSCR0_FRF	MASK(5, 4)
#define SSCR0_MOT	SET_BITS(5, 4, 0)
#define SSCR0_TI	SET_BITS(5, 4, 1)
#define SSCR0_NAT	SET_BITS(5, 4, 2)
#define SSCR0_PSP	SET_BITS(5, 4, 3)
#define SSCR0_ECS	BIT(6)
#define SSCR0_SSE	BIT(7)
#define SSCR0_SCR_MASK	MASK(19, 8)
#define SSCR0_SCR(x)	SET_BITS(19, 8, x)
#define SSCR0_EDSS	BIT(20)
#define SSCR0_NCS	BIT(21)
#define SSCR0_RIM	BIT(22)
#define SSCR0_TIM	BIT(23)
#define SSCR0_FRDC(x)	SET_BITS(26, 24, (x) - 1)
#define SSCR0_ACS	BIT(30)
#define SSCR0_MOD	BIT(31)

/* SSCR1 bits */
#define SSCR1_RIE	BIT(0)
#define SSCR1_TIE	BIT(1)
#define SSCR1_LBM	BIT(2)
#define SSCR1_SPO	BIT(3)
#define SSCR1_SPH	BIT(4)
#define SSCR1_MWDS	BIT(5)
#define SSCR1_TFT_MASK	MASK(9, 6)
#define SSCR1_TFT(x)	SET_BITS(9, 6, (x) - 1)
#define SSCR1_RFT_MASK	MASK(13, 10)
#define SSCR1_RFT(x)	SET_BITS(13, 10, (x) - 1)
#define SSCR1_EFWR	BIT(14)
#define SSCR1_STRF	BIT(15)
#define SSCR1_IFS	BIT(16)
#define SSCR1_PINTE	BIT(18)
#define SSCR1_TINTE	BIT(19)
#define SSCR1_RSRE	BIT(20)
#define SSCR1_TSRE	BIT(21)
#define SSCR1_TRAIL	BIT(22)
#define SSCR1_RWOT	BIT(23)
#define SSCR1_SFRMDIR	BIT(24)
#define SSCR1_SCLKDIR	BIT(25)
#define SSCR1_ECRB	BIT(26)
#define SSCR1_ECRA	BIT(27)
#define SSCR1_SCFR	BIT(28)
#define SSCR1_EBCEI	BIT(29)
#define SSCR1_TTE	BIT(30)
#define SSCR1_TTELP	BIT(31)

/* SSCR2 bits */
#define SSCR2_URUN_FIX0	BIT(0)
#define SSCR2_URUN_FIX1	BIT(1)
#define SSCR2_SLV_EXT_CLK_RUN_EN	BIT(2)
#define SSCR2_CLK_DEL_EN		BIT(3)
#define SSCR2_UNDRN_FIX_EN		BIT(6)
#define SSCR2_FIFO_EMPTY_FIX_EN		BIT(7)
#define SSCR2_ASRC_CNTR_EN		BIT(8)
#define SSCR2_ASRC_CNTR_CLR		BIT(9)
#define SSCR2_ASRC_FRM_CNRT_EN		BIT(10)
#define SSCR2_ASRC_INTR_MASK		BIT(11)
#define SSCR2_TURM1		BIT(1)
#define SSCR2_PSPSRWFDFD	BIT(3)
#define SSCR2_PSPSTWFDFD	BIT(4)
#define SSCR2_SDFD		BIT(14)
#define SSCR2_SDPM		BIT(16)
#define SSCR2_LJDFD		BIT(17)
#define SSCR2_MMRATF		BIT(18)
#define SSCR2_SMTATF		BIT(19)

/* SSR bits */
#define SSSR_TNF	BIT(2)
#define SSSR_RNE	BIT(3)
#define SSSR_BSY	BIT(4)
#define SSSR_TFS	BIT(5)
#define SSSR_RFS	BIT(6)
#define SSSR_ROR	BIT(7)
#define SSSR_TUR	BIT(21)

/* SSPSP bits */
#define SSPSP_SCMODE(x)		SET_BITS(1, 0, x)
#define SSPSP_SFRMP(x)		SET_BIT(2, x)
#define SSPSP_ETDS		BIT(3)
#define SSPSP_STRTDLY(x)	SET_BITS(6, 4, x)
#define SSPSP_DMYSTRT(x)	SET_BITS(8, 7, x)
#define SSPSP_SFRMDLY(x)	SET_BITS(15, 9, x)
#define SSPSP_SFRMWDTH(x)	SET_BITS(21, 16, x)
#define SSPSP_DMYSTOP(x)	SET_BITS(24, 23, x)
#define SSPSP_DMYSTOP_BITS	2
#define SSPSP_DMYSTOP_MASK	MASK(SSPSP_DMYSTOP_BITS - 1, 0)
#define SSPSP_FSRT		BIT(25)
#define SSPSP_EDMYSTOP(x)	SET_BITS(28, 26, x)

#define SSPSP2			0x44
#define SSPSP2_FEP_MASK		0xff

#define SSCR3		0x48
#define SSIOC		0x4C
#define SSP_REG_MAX	SSIOC

/* SSTSA bits */
#define SSTSA_SSTSA(x)		SET_BITS(7, 0, x)
#define SSTSA_TXEN		BIT(8)

/* SSRSA bits */
#define SSRSA_SSRSA(x)		SET_BITS(7, 0, x)
#define SSRSA_RXEN		BIT(8)

/* SSCR3 bits */
#define SSCR3_FRM_MST_EN	BIT(0)
#define SSCR3_I2S_MODE_EN	BIT(1)
#define SSCR3_I2S_FRM_POL(x)	SET_BIT(2, x)
#define SSCR3_I2S_TX_SS_FIX_EN	BIT(3)
#define SSCR3_I2S_RX_SS_FIX_EN	BIT(4)
#define SSCR3_I2S_TX_EN		BIT(9)
#define SSCR3_I2S_RX_EN		BIT(10)
#define SSCR3_CLK_EDGE_SEL	BIT(12)
#define SSCR3_STRETCH_TX	BIT(14)
#define SSCR3_STRETCH_RX	BIT(15)
#define SSCR3_MST_CLK_EN	BIT(16)
#define SSCR3_SYN_FIX_EN	BIT(17)

/* SSCR4 bits */
#define SSCR4_TOT_FRM_PRD(x)	((x) << 7)

/* SSCR5 bits */
#define SSCR5_FRM_ASRT_CLOCKS(x)	(((x) - 1) << 1)
#define SSCR5_FRM_POLARITY(x)	SET_BIT(0, x)

/* SFIFOTT bits */
#define SFIFOTT_TX(x)		((x) - 1)
#define SFIFOTT_RX(x)		(((x) - 1) << 16)

/* SFIFOL bits */
#define SFIFOL_TFL(x)		((x) & 0xFFFF)
#define SFIFOL_RFL(x)		((x) >> 16)

#define SSTSA_TSEN			BIT(8)
#define SSRSA_RSEN			BIT(8)

#define SSCR3_TFL_MASK	MASK(5, 0)
#define SSCR3_RFL_MASK	MASK(13, 8)
#define SSCR3_TFL_VAL(scr3_val)	(((scr3_val) >> 0) & MASK(5, 0))
#define SSCR3_RFL_VAL(scr3_val)	(((scr3_val) >> 8) & MASK(5, 0))
#define SSCR3_TX(x)	SET_BITS(21, 16, (x) - 1)
#define SSCR3_RX(x)	SET_BITS(29, 24, (x) - 1)

#define SSIOC_TXDPDEB	BIT(1)
#define SSIOC_SFCR	BIT(4)
#define SSIOC_SCOE	BIT(5)

#define MAX_SSP_COUNT 8
#define SSP_FIFO_DEPTH          16
#define SSP_FIFO_WATERMARK      8

#define SSP_INTEL_QUIRK_TINTE		(1 << 0)
#define SSP_INTEL_QUIRK_PINTE		(1 << 1)
#define SSP_INTEL_QUIRK_SMTATF		(1 << 2)
#define SSP_INTEL_QUIRK_MMRATF		(1 << 3)
#define SSP_INTEL_QUIRK_PSPSTWFDFD	(1 << 4)
#define SSP_INTEL_QUIRK_PSPSRWFDFD	(1 << 5)
#define SSP_INTEL_QUIRK_LBM		(1 << 6)

#define SSP_INTEL_FRAME_PULSE_WIDTH_MAX		38
#define SSP_INTEL_SLOT_PADDING_MAX		31

/* SSP clocks control settings */
#define SSP_INTEL_MCLK_0_DISABLE		BIT(0)
#define SSP_INTEL_MCLK_1_DISABLE		BIT(1)
#define SSP_INTEL_CLKCTRL_MCLK_KA		BIT(2)
#define SSP_INTEL_CLKCTRL_BCLK_KA		BIT(3)
#define SSP_INTEL_CLKCTRL_FS_KA			BIT(4)
#define SSP_INTEL_CLKCTRL_BCLK_IDLE_HIGH	BIT(5)

#endif /* __SSP_MACROS_H */
