// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __DMIC_MACROS_H
#define __DMIC_MACROS_H

#include "dmic-intel.h"

#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_FIFOS		2
#define DMIC_HW_FIR_LENGTH_MAX	250

/* Get max and min signed integer values for N bits word length */
#define INT_MAX(N)	((int64_t)((1ULL << ((N) - 1)) - 1))

/* Fractional multiplication with shift and round
 * Note that the parameters px and py must be cast to (int64_t) if other type.
 */
#define Q_MULTSR_32X32(px, py, qx, qy, qp) \
	((((px) * (py) >> ((qx) + (qy) - (qp) - 1)) + 1) >> 1)

/* Convert a float number to fractional Qnx.ny format. Note that there is no
 * check for nx+ny number of bits to fit the word length of int. The parameter
 * qy must be 31 or less.
 */
#define Q_CONVERT_FLOAT(f, qy) \
	((int32_t)(((const double)f) * ((int64_t)1 << (const int)qy) + 0.5))

/* Saturation */
#define SATP_INT32(x) (((x) > INT32_MAX) ? INT32_MAX : (x))

#define DMIC_MAX_MODES 50
#define DMIC_FIR_PIPELINE_OVERHEAD 5

/* Minimum OSR is always applied for 48 kHz and less sample rates */
#define DMIC_MIN_OSR  50

/* These are used as guideline for configuring > 48 kHz sample rates. The
 * minimum OSR can be relaxed down to 40 (use 3.84 MHz clock for 96 kHz).
 */
#define DMIC_HIGH_RATE_MIN_FS	64000
#define DMIC_HIGH_RATE_OSR_MIN	40

/* Used for scaling FIR coefficients for HW */
#define DMIC_HW_FIR_COEF_MAX ((1 << (DMIC_HW_BITS_FIR_COEF - 1)) - 1)
#define DMIC_HW_FIR_COEF_Q (DMIC_HW_BITS_FIR_COEF - 1)

/* Internal precision in gains computation, e.g. Q4.28 in int32_t */
#define DMIC_FIR_SCALE_Q 28

/* Parameters used in modes computation */
#define DMIC_HW_BITS_CIC		26
#define DMIC_HW_BITS_FIR_COEF		20
#define DMIC_HW_BITS_FIR_GAIN		20
#define DMIC_HW_BITS_FIR_INPUT		22
#define DMIC_HW_BITS_FIR_OUTPUT		24
#define DMIC_HW_BITS_FIR_INTERNAL	26
#define DMIC_HW_BITS_GAIN_OUTPUT	22
#define DMIC_HW_CIC_SHIFT_MIN		-8
#define DMIC_HW_CIC_SHIFT_MAX		4
#define DMIC_HW_FIR_SHIFT_MIN		0
#define DMIC_HW_FIR_SHIFT_MAX		8
#define DMIC_HW_CIC_DECIM_MIN		5
#define DMIC_HW_CIC_DECIM_MAX		31 /* Note: Limited by BITS_CIC */
#define DMIC_HW_FIR_DECIM_MIN		2
#define DMIC_HW_FIR_DECIM_MAX		20 /* Note: Practical upper limit */
#define DMIC_HW_SENS_Q28		Q_CONVERT_FLOAT(1.0, 28) /* Q1.28 */
#define DMIC_HW_PDM_CLK_MIN		100000 /* Note: Practical min value */
#define DMIC_HW_DUTY_MIN		20 /* Note: Practical min value */
#define DMIC_HW_DUTY_MAX		80 /* Note: Practical max value */

/* OUTCONTROL0 bits */
#define OUTCONTROL0_TIE_BIT	BIT(27)
#define OUTCONTROL0_SIP_BIT	BIT(26)
#define OUTCONTROL0_FINIT_BIT	BIT(25)
#define OUTCONTROL0_FCI_BIT	BIT(24)
#define OUTCONTROL0_TIE(x)	SET_BIT(27, x)
#define OUTCONTROL0_SIP(x)	SET_BIT(26, x)
#define OUTCONTROL0_FINIT(x)	SET_BIT(25, x)
#define OUTCONTROL0_FCI(x)	SET_BIT(24, x)
#define OUTCONTROL0_BFTH(x)	SET_BITS(23, 20, x)
#define OUTCONTROL0_OF(x)	SET_BITS(19, 18, x)
#define OUTCONTROL0_TH(x)	SET_BITS(5, 0, x)

/* OUTCONTROL1 bits */
#define OUTCONTROL1_TIE_BIT	BIT(27)
#define OUTCONTROL1_SIP_BIT	BIT(26)
#define OUTCONTROL1_FINIT_BIT	BIT(25)
#define OUTCONTROL1_FCI_BIT	BIT(24)
#define OUTCONTROL1_TIE(x)	SET_BIT(27, x)
#define OUTCONTROL1_SIP(x)	SET_BIT(26, x)
#define OUTCONTROL1_FINIT(x)	SET_BIT(25, x)
#define OUTCONTROL1_FCI(x)	SET_BIT(24, x)
#define OUTCONTROL1_BFTH(x)	SET_BITS(23, 20, x)
#define OUTCONTROL1_OF(x)	SET_BITS(19, 18, x)
#define OUTCONTROL1_TH(x)	SET_BITS(5, 0, x)

/* OUTCONTROL0 bits ver1*/
#define OUTCONTROL0_IPM_VER1(x)	SET_BITS(17, 16, x)
/* OUTCONTROL1 bits ver1 */
#define OUTCONTROL1_IPM_VER1(x)	SET_BITS(17, 16, x)

/* OUTCONTROL0 bits */
#define OUTCONTROL0_IPM_VER2(x)			SET_BITS(17, 15, x)
#define OUTCONTROL0_IPM_SOURCE_1(x)		SET_BITS(14, 13, x)
#define OUTCONTROL0_IPM_SOURCE_2(x)		SET_BITS(12, 11, x)
#define OUTCONTROL0_IPM_SOURCE_3(x)		SET_BITS(10, 9, x)
#define OUTCONTROL0_IPM_SOURCE_4(x)		SET_BITS(8, 7, x)
#define OUTCONTROL0_IPM_SOURCE_MODE(x)		SET_BIT(6, x)

/* OUTCONTROL1 bits */
#define OUTCONTROL1_IPM_VER2(x)			SET_BITS(17, 15, x)
#define OUTCONTROL1_IPM_SOURCE_1(x)		SET_BITS(14, 13, x)
#define OUTCONTROL1_IPM_SOURCE_2(x)		SET_BITS(12, 11, x)
#define OUTCONTROL1_IPM_SOURCE_3(x)		SET_BITS(10, 9, x)
#define OUTCONTROL1_IPM_SOURCE_4(x)		SET_BITS(8, 7, x)
#define OUTCONTROL1_IPM_SOURCE_MODE(x)		SET_BIT(6, x)

#define OUTCONTROLX_IPM_NUMSOURCES		4

/* CIC_CONTROL bits */
#define CIC_CONTROL_SOFT_RESET_BIT	BIT(16)
#define CIC_CONTROL_CIC_START_B_BIT	BIT(15)
#define CIC_CONTROL_CIC_START_A_BIT	BIT(14)
#define CIC_CONTROL_MIC_B_POLARITY_BIT	BIT(3)
#define CIC_CONTROL_MIC_A_POLARITY_BIT	BIT(2)
#define CIC_CONTROL_MIC_MUTE_BIT	BIT(1)
#define CIC_CONTROL_STEREO_MODE_BIT	BIT(0)

#define CIC_CONTROL_SOFT_RESET(x)	SET_BIT(16, x)
#define CIC_CONTROL_CIC_START_B(x)	SET_BIT(15, x)
#define CIC_CONTROL_CIC_START_A(x)	SET_BIT(14, x)
#define CIC_CONTROL_MIC_B_POLARITY(x)	SET_BIT(3, x)
#define CIC_CONTROL_MIC_A_POLARITY(x)	SET_BIT(2, x)
#define CIC_CONTROL_MIC_MUTE(x)		SET_BIT(1, x)
#define CIC_CONTROL_STEREO_MODE(x)	SET_BIT(0, x)

/* CIC_CONFIG bits */
#define CIC_CONFIG_CIC_SHIFT(x)		SET_BITS(27, 24, x)
#define CIC_CONFIG_COMB_COUNT(x)	SET_BITS(15, 8, x)

/* CIC_CONFIG masks */
#define CIC_CONFIG_CIC_SHIFT_MASK	MASK(27, 24)
#define CIC_CONFIG_COMB_COUNT_MASK	MASK(15, 8)

/* MIC_CONTROL bits */
#define MIC_CONTROL_PDM_EN_B_BIT	BIT(1)
#define MIC_CONTROL_PDM_EN_A_BIT	BIT(0)
#define MIC_CONTROL_PDM_CLKDIV(x)	SET_BITS(15, 8, x)
#define MIC_CONTROL_PDM_SKEW(x)		SET_BITS(7, 4, x)
#define MIC_CONTROL_CLK_EDGE(x)		SET_BIT(3, x)
#define MIC_CONTROL_PDM_EN_B(x)		SET_BIT(1, x)
#define MIC_CONTROL_PDM_EN_A(x)		SET_BIT(0, x)

/* MIC_CONTROL masks */
#define MIC_CONTROL_PDM_CLKDIV_MASK	MASK(15, 8)

/* FIR_CONTROL_A bits */
#define FIR_CONTROL_A_START_BIT			BIT(7)
#define FIR_CONTROL_A_ARRAY_START_EN_BIT	BIT(6)
#define FIR_CONTROL_A_MUTE_BIT			BIT(1)
#define FIR_CONTROL_A_START(x)			SET_BIT(7, x)
#define FIR_CONTROL_A_ARRAY_START_EN(x)		SET_BIT(6, x)
#define FIR_CONTROL_A_DCCOMP(x)			SET_BIT(4, x)
#define FIR_CONTROL_A_MUTE(x)			SET_BIT(1, x)
#define FIR_CONTROL_A_STEREO(x)			SET_BIT(0, x)

/* FIR_CONFIG_A bits */
#define FIR_CONFIG_A_FIR_DECIMATION(x)		SET_BITS(20, 16, x)
#define FIR_CONFIG_A_FIR_SHIFT(x)		SET_BITS(11, 8, x)
#define FIR_CONFIG_A_FIR_LENGTH(x)		SET_BITS(7, 0, x)

/* DC offset compensation time constants */
#define DCCOMP_TC0	0
#define DCCOMP_TC1	1
#define DCCOMP_TC2	2
#define DCCOMP_TC3	3
#define DCCOMP_TC4	4
#define DCCOMP_TC5	5
#define DCCOMP_TC6	6
#define DCCOMP_TC7	7

/* DC_OFFSET_LEFT_A bits */
#define DC_OFFSET_LEFT_A_DC_OFFS(x)		SET_BITS(21, 0, x)

/* DC_OFFSET_RIGHT_A bits */
#define DC_OFFSET_RIGHT_A_DC_OFFS(x)		SET_BITS(21, 0, x)

/* OUT_GAIN_LEFT_A bits */
#define OUT_GAIN_LEFT_A_GAIN(x)			SET_BITS(19, 0, x)

/* OUT_GAIN_RIGHT_A bits */
#define OUT_GAIN_RIGHT_A_GAIN(x)		SET_BITS(19, 0, x)

/* FIR_CONTROL_B bits */
#define FIR_CONTROL_B_START_BIT			BIT(7)
#define FIR_CONTROL_B_ARRAY_START_EN_BIT	BIT(6)
#define FIR_CONTROL_B_MUTE_BIT			BIT(1)
#define FIR_CONTROL_B_START(x)			SET_BIT(7, x)
#define FIR_CONTROL_B_ARRAY_START_EN(x)		SET_BIT(6, x)
#define FIR_CONTROL_B_DCCOMP(x)			SET_BIT(4, x)
#define FIR_CONTROL_B_MUTE(x)			SET_BIT(1, x)
#define FIR_CONTROL_B_STEREO(x)			SET_BIT(0, x)

/* FIR_CONFIG_B bits */
#define FIR_CONFIG_B_FIR_DECIMATION(x)		SET_BITS(20, 16, x)
#define FIR_CONFIG_B_FIR_SHIFT(x)		SET_BITS(11, 8, x)
#define FIR_CONFIG_B_FIR_LENGTH(x)		SET_BITS(7, 0, x)

/* DC_OFFSET_LEFT_B bits */
#define DC_OFFSET_LEFT_B_DC_OFFS(x)		SET_BITS(21, 0, x)

/* DC_OFFSET_RIGHT_B bits */
#define DC_OFFSET_RIGHT_B_DC_OFFS(x)		SET_BITS(21, 0, x)

/* OUT_GAIN_LEFT_B bits */
#define OUT_GAIN_LEFT_B_GAIN(x)			SET_BITS(19, 0, x)

/* OUT_GAIN_RIGHT_B bits */
#define OUT_GAIN_RIGHT_B_GAIN(x)		SET_BITS(19, 0, x)

/* FIR coefficients */
#define FIR_COEF_A(x)				SET_BITS(19, 0, x)
#define FIR_COEF_B(x)				SET_BITS(19, 0, x)

/* structs for dmic internal calculations */
struct dmic_calc_decim_modes {
	int16_t clkdiv[DMIC_MAX_MODES];
	int16_t mcic[DMIC_MAX_MODES];
	int16_t mfir[DMIC_MAX_MODES];
	int num_of_modes;
};

struct dmic_calc_matched_modes {
	int16_t clkdiv[DMIC_MAX_MODES];
	int16_t mcic[DMIC_MAX_MODES];
	int16_t mfir_a[DMIC_MAX_MODES];
	int16_t mfir_b[DMIC_MAX_MODES];
	int num_of_modes;
};

struct dmic_calc_configuration {
	struct pdm_decim *fir_a;
	struct pdm_decim *fir_b;
	int clkdiv;
	int mcic;
	int mfir_a;
	int mfir_b;
	int cic_shift;
	int fir_a_shift;
	int fir_b_shift;
	int fir_a_length;
	int fir_b_length;
	int32_t fir_a_scale;
	int32_t fir_b_scale;
};

/* structs for gathering the parameters from topology */
struct dmic_config_pdm {
	uint16_t id;
	uint16_t enable_mic_a;
	uint16_t enable_mic_b;
	uint16_t polarity_mic_a;
	uint16_t polarity_mic_b;
	uint16_t clk_edge;
	uint16_t skew;
};

struct dmic_config_dai {
	uint32_t driver_version;
	uint32_t io_clk;
	uint32_t pdmclk_min;
	uint32_t pdmclk_max;
	uint32_t fifo_fs;
	uint16_t fifo_bits;
	uint16_t fifo_bits_b;
	uint16_t duty_min;
	uint16_t duty_max;
	uint32_t num_pdm_active;
	uint32_t wake_up_time;
	uint32_t min_clock_on_time;
	uint32_t unmute_ramp_time;
	struct dmic_config_pdm pdm[DMIC_HW_CONTROLLERS];
};

/* every pdm controller has separate fir filter for output fifos */
struct dmic_calc_fir_coeffs_array {
	uint32_t fir_len[DMIC_HW_CONTROLLERS];
	int32_t fir_coeffs[DMIC_HW_CONTROLLERS][DMIC_HW_FIFOS][DMIC_HW_FIR_LENGTH_MAX];
};

struct dmic_config_mic_vendor {
	uint8_t type;
	uint8_t panel;
	uint32_t speaker_position_distance;
	uint32_t horizontal_offset;
	uint32_t vertical_offset;
	uint8_t frequency_low_band;
	uint8_t frequency_high_band;
	uint16_t direction_angle;
	uint16_t elevation_angle;
	uint16_t vertical_angle_begin;
	uint16_t vertical_angle_end;
	uint16_t horizontal_angle_begin;
	uint16_t horizontal_angle_end;
};

struct dmic_config_mic {
	uint8_t num_mics;
	uint8_t extension;
	int8_t array_type;
	uint32_t snr;
	uint32_t sensitivity;
	struct dmic_config_mic_vendor vendor[8];
};

struct intel_dmic_params {
	/* structs to gather dmic params before calculations */
	struct dmic_config_dai dmic_prm[DMIC_HW_FIFOS];
	uint32_t dmic_dai_index;
	int dmic_count;

	/* dmic vendor blob structs */
	struct dmic_intel_config_data dmic_blob;
	struct dmic_intel_pdm_ctrl_cfg dmic_blob_pdm[DMIC_HW_CONTROLLERS];
	struct dmic_intel_fir_config dmic_blob_fir[DMIC_HW_CONTROLLERS][DMIC_HW_FIFOS];
	struct dmic_calc_fir_coeffs_array dmic_fir_array;
	struct dmic_config_mic dmic_mic_config;
};

#endif /* __DMIC_MACROS_H */
