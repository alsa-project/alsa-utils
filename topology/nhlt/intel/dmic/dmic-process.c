// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/error.h>
#include "../intel-nhlt.h"
#include "dmic-process.h"
#include "dmic-internal.h"
#include "pdm-decim-fir.h"
#include "dmic-debug.h"

/* Note 1: Higher spec filter must be before lower spec filter if there are multiple filters for a
 * decimation factor. The first filter is skipped if the length is too much vs. overrun limit. If
 * other order the better filter would be never selected.
 *
 * Note 2: The introduction order of FIR decimation factors is the selection preference order.
 * The decimation factor 5 and 10 (2*5) cause a often less compatible output sample rate for CIC so
 * they are not used if there other suitable nearby values.
 *
 * The naming scheme of coefficients set is:
 * <type>_<decim factor>_<rel passband>_<rel stopband>_<ripple>_<attenuation>
 */
struct pdm_decim *fir_list[] = {
	&pdm_decim_int32_02_4375_5100_010_095,
	&pdm_decim_int32_02_4323_5100_010_095,
	&pdm_decim_int32_03_4375_5100_010_095,
	&pdm_decim_int32_04_4318_5100_010_095,
	&pdm_decim_int32_06_4172_5100_010_095,
	&pdm_decim_int32_05_4325_5100_010_095,
	&pdm_decim_int32_08_4156_5301_010_090,
	&pdm_decim_int32_12_4156_5345_010_090,
	&pdm_decim_int32_10_4156_5345_010_090,
	NULL, /* This marks the end of coefficients */
};

/* This is a divide function that returns ceil of the quotient. E.g. ceil_divide(9, 3) returns 3,
 * ceil_divide(10, 3) returns 4.
 */
static int ceil_divide(int a, int b)
{
	int c;

	c = a / b;

	if (!((a ^ b) & (1U << ((sizeof(int) * 8) - 1))) && c * b != a)
		c++;

	return c;
}

/* This function searches from vec[] (of length vec_length) integer values of n. The indices to
 * equal values is returned in idx[]. The function returns the number of found matches.
 * The max_results should be set to 0 (or negative) or vec_length to get all matches. The
 * max_result can be set to 1 to receive only the first match in ascending order. It avoids need for
 * an array for idx.
 */
static int find_equal_int16(int16_t idx[], int16_t vec[], int n, int vec_length,
			    int max_results)
{
	int nresults = 0;
	int i;

	for (i = 0; i < vec_length; i++) {
		if (vec[i] == n) {
			idx[nresults++] = i;
			if (nresults == max_results)
				break;
		}
	}

	return nresults;
}

/* Return the largest absolute value found in the vector. Note that smallest negative value need to
 * be saturated to preset as int32_t.
 */
static int32_t find_max_abs_int32(int32_t vec[], int vec_length)
{
	int i;
	int64_t amax = (vec[0] > 0) ? vec[0] : -vec[0];

	for (i = 1; i < vec_length; i++) {
		amax = (vec[i] > amax) ? vec[i] : amax;
		amax = (-vec[i] > amax) ? -vec[i] : amax;
	}

	return SATP_INT32(amax); /* Amax is always a positive value */
}

/* Count the left shift amount to normalize a 32 bit signed integer value without causing overflow.
 * Input value 0 will result to 31.
 */
static int norm_int32(int32_t val)
{
	int c = 0;

	/* count number of bits c that val can be right-shifted arithmetically
	 * until there is -1 (if val is negative) or 0 (if val is positive)
	 * norm of val will be 31-c
	 */
	for (; val != -1 && val != 0; c++)
		val >>= 1;

	return 31 - c;
}

/* This function returns a raw list of potential microphone clock and decimation modes for achieving
 * requested sample rates. The search is constrained by decimation HW capabililies and setup
 * parameters. The parameters such as microphone clock min/max and duty cycle requirements need be
 * checked from used microphone component datasheet.
 */
static void find_modes(struct intel_dmic_params *dmic, struct dmic_calc_decim_modes *modes,
		       uint32_t fs)
{
	int di = dmic->dmic_dai_index;
	int clkdiv_min;
	int clkdiv_max;
	int clkdiv;
	int c1;
	int du_min;
	int du_max;
	int pdmclk;
	int osr;
	int mfir;
	int mcic;
	int ioclk_test;
	int osr_min = DMIC_MIN_OSR;
	int j;
	int i = 0;

	/* Defaults, empty result */
	modes->num_of_modes = 0;

	/* The FIFO is not requested if sample rate is set to zero. Just return in such case with
	 * num_of_modes as zero.
	 */
	if (fs == 0) {
		fprintf(stderr, "find_modes(): fs not set\n");
		return;
	}

	/* Override DMIC_MIN_OSR for very high sample rates, use as minimum the nominal clock for
	 * the high rates.
	 */
	if (fs >= DMIC_HIGH_RATE_MIN_FS)
		osr_min = DMIC_HIGH_RATE_OSR_MIN;

	/* Check for sane pdm clock, min 100 kHz, max ioclk/2 */
	if (dmic->dmic_prm[di].pdmclk_max < DMIC_HW_PDM_CLK_MIN ||
	    dmic->dmic_prm[di].pdmclk_max > dmic->dmic_prm[di].io_clk / 2) {
		fprintf(stderr, "find_modes():  pdm clock max not in range\n");
		return;
	}
	if (dmic->dmic_prm[di].pdmclk_min < DMIC_HW_PDM_CLK_MIN ||
	    dmic->dmic_prm[di].pdmclk_min > dmic->dmic_prm[di].pdmclk_max) {
		fprintf(stderr, "find_modes():  pdm clock min not in range\n");
		return;
	}

	/* Check for sane duty cycle */
	if (dmic->dmic_prm[di].duty_min > dmic->dmic_prm[di].duty_max) {
		fprintf(stderr, "find_modes(): duty cycle min > max\n");
		return;
	}
	if (dmic->dmic_prm[di].duty_min < DMIC_HW_DUTY_MIN ||
	    dmic->dmic_prm[di].duty_min > DMIC_HW_DUTY_MAX) {
		fprintf(stderr, "find_modes():  pdm clock min not in range\n");
		return;
	}
	if (dmic->dmic_prm[di].duty_max < DMIC_HW_DUTY_MIN ||
	    dmic->dmic_prm[di].duty_max > DMIC_HW_DUTY_MAX) {
		fprintf(stderr, "find_modes(): pdm clock max not in range\n");
		return;
	}

	/* Min and max clock dividers */
	clkdiv_min = ceil_divide(dmic->dmic_prm[di].io_clk, dmic->dmic_prm[di].pdmclk_max);
	clkdiv_min = MAX(clkdiv_min, DMIC_HW_CIC_DECIM_MIN);
	clkdiv_max = dmic->dmic_prm[di].io_clk / dmic->dmic_prm[di].pdmclk_min;

	/* Loop possible clock dividers and check based on resulting oversampling ratio that CIC and
	 * FIR decimation ratios are feasible. The ratios need to be integers. Also the mic clock
	 * duty cycle need to be within limits.
	 */
	for (clkdiv = clkdiv_min; clkdiv <= clkdiv_max; clkdiv++) {
		/* Calculate duty cycle for this clock divider. Note that odd dividers cause non-50%
		 * duty cycle.
		 */
		c1 = clkdiv >> 1;
		du_min = 100 * c1 / clkdiv;
		du_max = 100 - du_min;

		/* Calculate PDM clock rate and oversampling ratio. */
		pdmclk = dmic->dmic_prm[di].io_clk / clkdiv;
		osr = pdmclk / fs;

		/* Check that OSR constraints is met and clock duty cycle does not exceed microphone
		 * specification. If exceed proceed to next clkdiv.
		 */
		if (osr < osr_min || du_min < dmic->dmic_prm[di].duty_min ||
		    du_max > dmic->dmic_prm[di].duty_max)
			continue;

		/* Loop FIR decimation factors candidates. If the integer divided decimation factors
		 * and clock dividers as multiplied with sample rate match the IO clock rate the
		 * division was exact and such decimation mode is possible. Then check that CIC
		 * decimation constraints are met. The passed decimation modes are added to array.
		 */
		for (j = 0; fir_list[j]; j++) {
			mfir = fir_list[j]->decim_factor;

			/* Skip if previous decimation factor was the same */
			if (j > 1 && fir_list[j - 1]->decim_factor == mfir)
				continue;

			mcic = osr / mfir;
			ioclk_test = fs * mfir * mcic * clkdiv;

			if (ioclk_test == dmic->dmic_prm[di].io_clk &&
			    mcic >= DMIC_HW_CIC_DECIM_MIN &&
			    mcic <= DMIC_HW_CIC_DECIM_MAX &&
			    i < DMIC_MAX_MODES) {
				modes->clkdiv[i] = clkdiv;
				modes->mcic[i] = mcic;
				modes->mfir[i] = mfir;
				i++;
			}
		}
	}

	modes->num_of_modes = i;
}

/* The previous raw modes list contains sane configuration possibilities. When there is request for
 * both FIFOs A and B operation this function returns list of compatible settings.
 */
static void match_modes(struct dmic_calc_matched_modes *c, struct dmic_calc_decim_modes *a,
			struct dmic_calc_decim_modes *b)
{
	int16_t idx[DMIC_MAX_MODES];
	int idx_length;
	int i;
	int n;
	int m;

	/* Check if previous search got results. */
	c->num_of_modes = 0;
	if (a->num_of_modes == 0 && b->num_of_modes == 0) {
		/* Nothing to do */
		return;
	}

	/* Ensure that num_of_modes is sane. */
	if (a->num_of_modes > DMIC_MAX_MODES ||
	    b->num_of_modes > DMIC_MAX_MODES)
		return;

	/* Check for request only for FIFO A or B. In such case pass list for A or B as such. */
	if (b->num_of_modes == 0) {
		c->num_of_modes = a->num_of_modes;
		for (i = 0; i < a->num_of_modes; i++) {
			c->clkdiv[i] = a->clkdiv[i];
			c->mcic[i] = a->mcic[i];
			c->mfir_a[i] = a->mfir[i];
			c->mfir_b[i] = 0; /* Mark FIR B as non-used */
		}
		return;
	}

	if (a->num_of_modes == 0) {
		c->num_of_modes = b->num_of_modes;
		for (i = 0; i < b->num_of_modes; i++) {
			c->clkdiv[i] = b->clkdiv[i];
			c->mcic[i] = b->mcic[i];
			c->mfir_b[i] = b->mfir[i];
			c->mfir_a[i] = 0; /* Mark FIR A as non-used */
		}
		return;
	}

	/* Merge a list of compatible modes */
	i = 0;
	for (n = 0; n < a->num_of_modes; n++) {
		/* Find all indices of values a->clkdiv[n] in b->clkdiv[] */
		idx_length = find_equal_int16(idx, b->clkdiv, a->clkdiv[n],
					      b->num_of_modes, 0);
		for (m = 0; m < idx_length; m++) {
			if (b->mcic[idx[m]] == a->mcic[n]) {
				c->clkdiv[i] = a->clkdiv[n];
				c->mcic[i] = a->mcic[n];
				c->mfir_a[i] = a->mfir[n];
				c->mfir_b[i] = b->mfir[idx[m]];
				i++;
			}
		}
		c->num_of_modes = i;
	}
}

/* Finds a suitable FIR decimation filter from the included set */
static struct pdm_decim *get_fir(struct intel_dmic_params *dmic,
				 struct dmic_calc_configuration *cfg, int mfir)
{
	int i = 0;
	int fs;
	int cic_fs;
	int fir_max_length;
	struct pdm_decim *fir = NULL;
	int di = dmic->dmic_dai_index;

	if (mfir <= 0)
		return fir;

	cic_fs = dmic->dmic_prm[di].io_clk / cfg->clkdiv / cfg->mcic;
	fs = cic_fs / mfir;
	/* FIR max. length depends on available cycles and coef RAM length. Exceeding this length
	 * sets HW overrun status and overwrite of other register.
	 */
	fir_max_length = MIN(DMIC_HW_FIR_LENGTH_MAX,
			     dmic->dmic_prm[di].io_clk / fs / 2 -
			     DMIC_FIR_PIPELINE_OVERHEAD);

	/* Loop until NULL */
	while (fir_list[i]) {
		if (fir_list[i]->decim_factor == mfir) {
			if (fir_list[i]->length <= fir_max_length) {
				/* Store pointer, break from loop to avoid a possible other mode
				 * with lower FIR length.
				 */
				fir = fir_list[i];
				break;
			}
		}
		i++;
	}

	return fir;
}

/* Calculate scale and shift to use for FIR coefficients. Scale is applied before write to HW coef
 * RAM. Shift will be programmed to HW register.
 */
static int fir_coef_scale(int32_t *fir_scale, int *fir_shift, int add_shift,
			  const int32_t coef[], int coef_length, int32_t gain)
{
	int32_t amax;
	int32_t new_amax;
	int32_t fir_gain;
	int shift;

	/* Multiply gain passed from CIC with output full scale. */
	fir_gain = Q_MULTSR_32X32((int64_t)gain, DMIC_HW_SENS_Q28,
				  DMIC_FIR_SCALE_Q, 28, DMIC_FIR_SCALE_Q);

	/* Find the largest FIR coefficient value. */
	amax = find_max_abs_int32((int32_t *)coef, coef_length);

	/* Scale max. tap value with FIR gain. */
	new_amax = Q_MULTSR_32X32((int64_t)amax, fir_gain, 31,
				  DMIC_FIR_SCALE_Q, DMIC_FIR_SCALE_Q);
	if (new_amax <= 0)
		return -EINVAL;

	/* Get left shifts count to normalize the fractional value as 32 bit. We need right shifts
	 * count for scaling so need to invert. The difference of Q31 vs. used Q format is added to
	 * get the correct normalization right shift value.
	 */
	shift = 31 - DMIC_FIR_SCALE_Q - norm_int32(new_amax);

	/* Add to shift for coef raw Q31 format shift and store to configuration. Ensure range (fail
	 * should not happen with OK coefficient set).
	 */
	*fir_shift = -shift + add_shift;
	if (*fir_shift < DMIC_HW_FIR_SHIFT_MIN ||
	    *fir_shift > DMIC_HW_FIR_SHIFT_MAX)
		return -EINVAL;

	/* Compensate shift into FIR coef scaler and store as Q4.20. */
	if (shift < 0)
		*fir_scale = fir_gain << -shift;
	else
		*fir_scale = fir_gain >> shift;

	return 0;
}

/* This function selects with a simple criteria one mode to set up the decimator. For the settings
 * chosen for FIFOs A and B output a lookup is done for FIR coefficients from the included
 * coefficients tables. For some decimation factors there may be several length coefficient sets. It
 * is due to possible restruction of decimation engine cycles per given sample rate. If the
 * coefficients length is exceeded the lookup continues. Therefore the list of coefficient set must
 * present the filters for a decimation factor in decreasing length order.
 *
 * Note: If there is no filter available an error is returned. The parameters should be reviewed for
 * such case. If still a filter is missing it should be added into the included set. FIR decimation
 * with a high factor usually needs compromizes into specifications and is not desirable.
 */
static int select_mode(struct intel_dmic_params *dmic, struct dmic_calc_configuration *cfg,
		       struct dmic_calc_matched_modes *modes)
{
	int32_t g_cic;
	int32_t fir_in_max;
	int32_t cic_out_max;
	int32_t gain_to_fir;
	int16_t idx[DMIC_MAX_MODES];
	int16_t *mfir;
	int mcic;
	int bits_cic;
	int ret;
	int n;
	int found = 0;

	/* If there are more than one possibilities select a mode with a preferred FIR decimation
	 * factor. If there are several select mode with highest ioclk divider to minimize
	 * microphone power consumption. The highest clock divisors are in the end of list so select
	 * the last of list. The minimum OSR criteria used in previous ensures that quality in the
	 * candidates should be sufficient.
	 */
	if (modes->num_of_modes == 0) {
		fprintf(stderr, "select_mode(): no modes available\n");
		return -EINVAL;
	}

	/* Valid modes presence is indicated with non-zero decimation factor in 1st element. If FIR
	 * A is not used get decimation factors from FIR B instead.
	 */
	if (modes->mfir_a[0] > 0)
		mfir = modes->mfir_a;
	else
		mfir = modes->mfir_b;

	/* Search fir_list[] decimation factors from start towards end. The found last configuration
	 * entry with searched decimation factor will be used.
	 */
	for (n = 0; fir_list[n]; n++) {
		found = find_equal_int16(idx, mfir, fir_list[n]->decim_factor,
					 modes->num_of_modes, 0);
		if (found)
			break;
	}

	if (!found) {
		fprintf(stderr, "select_mode(): No filter for decimation found\n");
		return -EINVAL;
	}
	n = idx[found - 1]; /* Option with highest clock divisor and lowest mic clock rate */

	/* Get microphone clock and decimation parameters for used mode from the list. */
	cfg->clkdiv = modes->clkdiv[n];
	cfg->mfir_a = modes->mfir_a[n];
	cfg->mfir_b = modes->mfir_b[n];
	cfg->mcic = modes->mcic[n];
	cfg->fir_a = NULL;
	cfg->fir_b = NULL;

	/* Find raw FIR coefficients to match the decimation factors of FIR A and B. */
	if (cfg->mfir_a > 0) {
		cfg->fir_a = get_fir(dmic, cfg, cfg->mfir_a);
		if (!cfg->fir_a) {
			fprintf(stderr, "select_mode(): can't find FIR coefficients, mfir_a = %d\n",
				cfg->mfir_a);
			return -EINVAL;
		}
	}

	if (cfg->mfir_b > 0) {
		cfg->fir_b = get_fir(dmic, cfg, cfg->mfir_b);
		if (!cfg->fir_b) {
			fprintf(stderr, "select_mode(): can't find FIR coefficients, mfir_b = %d\n",
				cfg->mfir_b);
			return -EINVAL;
		}
	}

	/* Calculate CIC shift from the decimation factor specific gain. The gain of HW decimator
	 * equals decimation factor to power of 5.
	 */
	mcic = cfg->mcic;
	g_cic = mcic * mcic * mcic * mcic * mcic;
	if (g_cic < 0) {
		/* Erroneous decimation factor and CIC gain */
		fprintf(stderr, "select_mode(): erroneous decimation factor and CIC gain\n");
		return -EINVAL;
	}

	bits_cic = 32 - norm_int32(g_cic);
	cfg->cic_shift = bits_cic - DMIC_HW_BITS_FIR_INPUT;

	/* Calculate remaining gain to FIR in Q format used for gain values. */
	fir_in_max = INT_MAX(DMIC_HW_BITS_FIR_INPUT);
	if (cfg->cic_shift >= 0)
		cic_out_max = g_cic >> cfg->cic_shift;
	else
		cic_out_max = g_cic << -cfg->cic_shift;

	gain_to_fir = (int32_t)((((int64_t)fir_in_max) << DMIC_FIR_SCALE_Q) /
		cic_out_max);

	/* Calculate FIR scale and shift */
	if (cfg->mfir_a > 0) {
		cfg->fir_a_length = cfg->fir_a->length;
		ret = fir_coef_scale(&cfg->fir_a_scale, &cfg->fir_a_shift,
				     cfg->fir_a->shift, cfg->fir_a->coef,
				     cfg->fir_a->length, gain_to_fir);
		if (ret < 0) {
			/* Invalid coefficient set found, should not happen. */
			fprintf(stderr, "select_mode(): invalid coefficient set found\n");
			return -EINVAL;
		}
	} else {
		cfg->fir_a_scale = 0;
		cfg->fir_a_shift = 0;
		cfg->fir_a_length = 0;
	}

	if (cfg->mfir_b > 0) {
		cfg->fir_b_length = cfg->fir_b->length;
		ret = fir_coef_scale(&cfg->fir_b_scale, &cfg->fir_b_shift,
				     cfg->fir_b->shift, cfg->fir_b->coef,
				     cfg->fir_b->length, gain_to_fir);
		if (ret < 0) {
			/* Invalid coefficient set found, should not happen. */
			fprintf(stderr, "select_mode(): invalid coefficient set found\n");
			return -EINVAL;
		}
	} else {
		cfg->fir_b_scale = 0;
		cfg->fir_b_shift = 0;
		cfg->fir_b_length = 0;
	}

	return 0;
}

/* The FIFO input packer mode (IPM) settings are somewhat different in HW versions. This helper
 * function returns a suitable IPM bit field value to use.
 */
static void ipm_helper1(struct intel_dmic_params *dmic, int *ipm)
{
	int di = dmic->dmic_dai_index;
	int pdm[DMIC_HW_CONTROLLERS];
	int i;

	/* Loop number of PDM controllers in the configuration. If mic A or B is enabled then a pdm
	 * controller is marked as active for this DAI.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (dmic->dmic_prm[di].pdm[i].enable_mic_a ||
		    dmic->dmic_prm[di].pdm[i].enable_mic_b)
			pdm[i] = 1;
		else
			pdm[i] = 0;
	}

	/* Set IPM to match active pdm controllers. */
	*ipm = 0;

	if (pdm[0] == 0 && pdm[1] > 0)
		*ipm = 1;

	if (pdm[0] > 0 && pdm[1] > 0)
		*ipm = 2;
}

static void ipm_helper2(struct intel_dmic_params *dmic, int source[], int *ipm)
{
	int di = dmic->dmic_dai_index;
	int pdm[DMIC_HW_CONTROLLERS];
	int i;
	int n = 0;

	for (i = 0; i < OUTCONTROLX_IPM_NUMSOURCES; i++)
		source[i] = 0;

	/* Loop number of PDM controllers in the configuration. If mic A or B is enabled then a pdm
	 * controller is marked as active. The function returns in array source[] the indice of
	 * enabled pdm controllers to be used for IPM configuration.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (dmic->dmic_prm[di].pdm[i].enable_mic_a ||
		    dmic->dmic_prm[di].pdm[i].enable_mic_b) {
			pdm[i] = 1;
			source[n] = i;
			n++;
		} else {
			pdm[i] = 0;
		}
	}

	/* IPM bit field is set to count of active pdm controllers. */
	*ipm = pdm[0];
	for (i = 1; i < DMIC_HW_CONTROLLERS; i++)
		*ipm += pdm[i];
}

/* Loop number of PDM controllers in the configuration. The function checks if the controller should
 * operate as stereo or mono left (A) or mono right (B) mode. Mono right mode is setup as channel
 * swapped mono left.
 */
static int stereo_helper(struct intel_dmic_params *dmic, int stereo[], int swap[])
{
	int cnt;
	int i;
	int swap_check;
	int ret = 0;

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		cnt = 0;
		if (dmic->dmic_prm[0].pdm[i].enable_mic_a ||
		    dmic->dmic_prm[1].pdm[i].enable_mic_a)
			cnt++;

		if (dmic->dmic_prm[0].pdm[i].enable_mic_b ||
		    dmic->dmic_prm[1].pdm[i].enable_mic_b)
			cnt++;

		/* Set stereo mode if both mic A anc B are enabled. */
		cnt >>= 1;
		stereo[i] = cnt;

		/* Swap channels if only mic B is used for mono processing. */
		swap[i] = (dmic->dmic_prm[0].pdm[i].enable_mic_b ||
			   dmic->dmic_prm[1].pdm[i].enable_mic_b) && !cnt;

		/* Check that swap does not conflict with other DAI request */
		swap_check = (dmic->dmic_prm[1].pdm[i].enable_mic_a ||
			      dmic->dmic_prm[0].pdm[i].enable_mic_a);

		if (swap_check && swap[i]) {
			ret = -EINVAL;
			break;
		}
	}
	return ret;
}

static int configure_registers(struct intel_dmic_params *dmic, struct dmic_calc_configuration *cfg)
{
	int stereo[DMIC_HW_CONTROLLERS];
	int swap[DMIC_HW_CONTROLLERS];
	uint32_t val = 0;
	int32_t ci;
	uint32_t cu;
	int ipm;
	int of0;
	int of1;
	int fir_decim;
	int fir_length;
	int length;
	int edge;
	int soft_reset;
	int cic_mute;
	int fir_mute;
	int i;
	int j;
	int ret;
	int mic;
	int chmap_bits;
	int di = dmic->dmic_dai_index;
	int dccomp = 1;
	int array_a = 0;
	int array_b = 0;
	int bfth = 3; /* Should be 3 for 8 entries, 1 is 2 entries */
	int th = 3; /* Used with TIE=1 */
	int source[OUTCONTROLX_IPM_NUMSOURCES];

	/*
	 * ts_group value describes which audio channels in the hw fifo are enabled. A 32 bit
	 * value is divided into 8 x 4 bit nibbles corresponding to 8 audio channels. Hex value 0xF
	 * means "not in use", any other value means the channel is enabled. For example 0xFFFFFFFF
	 * means no channels are enabled, 0xFFFFFF10 means channels 1 and 2 are enabled.
	 *
	 * ts_group array index corresponds to dmic hw fifos, that gather audio samples from pdm
	 * controllers. 1 pdm controller can host 2 mono dmics and usually pdm controllers are
	 * connected to 2 hw fifos -> we can for example run the dmics simultaneously with different
	 * sampling rates.
	 *
	 * Currently there is no evidence we would ever have more than 2 hw fifos, so ts_group[2]
	 * and ts_group[3] are not used for anything. Also the nibbles could be used for channel
	 * mapping the pdm channels arbitrarely into hw fifos, however currently it is used as
	 * binary not_enabled/enabled setting.
	 *
	 * if we have 2 dmics (stereo) it means we are using 1 pdm controller with possibly 2 hw
	 * fifos:
	 *        mic1      fifo0(2ch)
	 *            \    /
	 *             pdm0
	 *            /    \
	 *        mic2      fifo1(2ch)
	 *
	 * So in this case it makes only sense to control ts_group indexes 0 and 1 and their last 2
	 * nibbles (as we have only 2 channels).
	 *
	 * if we have 4 dmics, it means we are using 2 pdm controller with possibly 2 x 4 channel hw
	 * fifos:
	 *
	 *        mic1      fifo0(4ch)
	 *            \    /    /
	 *             pdm0    /
	 *            /    \  /
	 *        mic2      \/
	 *	  mic3      /\
	 *            \    /  \
	 *             pdm1    \
	 *            /    \    \
	 *        mic4      fifo1(4ch)
	 *
	 * So it makes sense to control ts_group indexes 0 and 1 and their last 4 nibbles.
	 *
	 * channel_pdm_mask defines which existing pdm controllers will be taken into use. So if
	 * either of mic a or b is enabled -> that particular pdm controller is in use. For example
	 * pdm0 in use/not_in_use is defined by setting bit 0 in channel_pdm_mask to 1/0.
	 *
	 * channel_ctrl_mask defines what mic channels are available in hw for a pdm controller. in
	 * theory pdm controller could have only 1 channel enabled, in practice there's always 2
	 * channels which are both enabled -> set bits 0 and 1.
	 */

	for (i = 0, mic = 0, chmap_bits = 4; i < DMIC_HW_CONTROLLERS; i++) {
		/* enable fifo channels (ts_group) based on mic_enable in dai definition */
		if (dmic->dmic_prm[di].pdm[i].enable_mic_a) {
			dmic->dmic_blob.ts_group[di] &= ~(0xF << (chmap_bits * mic));
			dmic->dmic_blob.ts_group[di] |= 0x0 << (chmap_bits * mic);
		}
		mic++;
		if (dmic->dmic_prm[di].pdm[i].enable_mic_b) {
			dmic->dmic_blob.ts_group[di] &= ~(0xF << (chmap_bits * mic));
			dmic->dmic_blob.ts_group[di] |= 0x1 << (chmap_bits * mic);
		}
		mic++;
	}

	/* set channel_pdm_mask to describe what pdm controllers are in use */
	for (i = 0; i < dmic->dmic_prm[di].num_pdm_active; i++)
		dmic->dmic_blob.channel_pdm_mask |= 1 << i;

	/* set always both mic channels enabled */
	dmic->dmic_blob.channel_ctrl_mask = 0x3;

	/* Normal start sequence */
	soft_reset = 0;
	cic_mute = 0;
	fir_mute = 0;

	/* OUTCONTROL0 and OUTCONTROL1 */
	of0 = (dmic->dmic_prm[0].fifo_bits == 32) ? 2 : 0;
	of1 = (dmic->dmic_prm[1].fifo_bits == 32) ? 2 : 0;

	if (dmic->dmic_prm[di].driver_version == 1) {
		if (di == 0) {
			ipm_helper1(dmic, &ipm);
			val = OUTCONTROL0_TIE(0) |
				OUTCONTROL0_SIP(0) |
				OUTCONTROL0_FINIT(0) |
				OUTCONTROL0_FCI(0) |
				OUTCONTROL0_BFTH(bfth) |
				OUTCONTROL0_OF(of0) |
				OUTCONTROL0_IPM_VER1(ipm) |
				OUTCONTROL0_TH(th);
		} else {
			ipm_helper1(dmic, &ipm);
			val = OUTCONTROL1_TIE(0) |
				OUTCONTROL1_SIP(0) |
				OUTCONTROL1_FINIT(0) |
				OUTCONTROL1_FCI(0) |
				OUTCONTROL1_BFTH(bfth) |
				OUTCONTROL1_OF(of1) |
				OUTCONTROL1_IPM_VER1(ipm) |
				OUTCONTROL1_TH(th);
		}
	}

	if (dmic->dmic_prm[di].driver_version == 2 || dmic->dmic_prm[di].driver_version == 3) {
		if (di == 0) {
			ipm_helper2(dmic, source, &ipm);
			val = OUTCONTROL0_TIE(0) |
				OUTCONTROL0_SIP(0) |
				OUTCONTROL0_FINIT(0) |
				OUTCONTROL0_FCI(0) |
				OUTCONTROL0_BFTH(bfth) |
				OUTCONTROL0_OF(of0) |
				OUTCONTROL0_IPM_VER2(ipm) |
				OUTCONTROL0_IPM_SOURCE_1(source[0]) |
				OUTCONTROL0_IPM_SOURCE_2(source[1]) |
				OUTCONTROL0_IPM_SOURCE_3(source[2]) |
				OUTCONTROL0_IPM_SOURCE_4(source[3]) |
				OUTCONTROL0_IPM_SOURCE_MODE(1) |
				OUTCONTROL0_TH(th);
		} else {
			ipm_helper2(dmic, source, &ipm);
			val = OUTCONTROL1_TIE(0) |
				OUTCONTROL1_SIP(0) |
				OUTCONTROL1_FINIT(0) |
				OUTCONTROL1_FCI(0) |
				OUTCONTROL1_BFTH(bfth) |
				OUTCONTROL1_OF(of1) |
				OUTCONTROL1_IPM_VER2(ipm) |
				OUTCONTROL1_IPM_SOURCE_1(source[0]) |
				OUTCONTROL1_IPM_SOURCE_2(source[1]) |
				OUTCONTROL1_IPM_SOURCE_3(source[2]) |
				OUTCONTROL1_IPM_SOURCE_4(source[3]) |
				OUTCONTROL1_IPM_SOURCE_MODE(1) |
				OUTCONTROL1_TH(th);
		}
	}

	dmic->dmic_blob.chan_ctrl_cfg[di] = val;

	ret = stereo_helper(dmic, stereo, swap);
	if (ret < 0) {
		fprintf(stderr, "configure_registers(): enable conflict\n");
		return ret;
	}

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		/* CIC */
		val = CIC_CONTROL_SOFT_RESET(soft_reset) |
			CIC_CONTROL_CIC_START_B(1) |
			CIC_CONTROL_CIC_START_A(1) |
			CIC_CONTROL_MIC_B_POLARITY(dmic->dmic_prm[di].pdm[i].polarity_mic_b) |
			CIC_CONTROL_MIC_A_POLARITY(dmic->dmic_prm[di].pdm[i].polarity_mic_a) |
			CIC_CONTROL_MIC_MUTE(cic_mute) |
			CIC_CONTROL_STEREO_MODE(stereo[i]);
		dmic->dmic_blob_pdm[i].cic_control = val;

		val = CIC_CONFIG_CIC_SHIFT(cfg->cic_shift + 8) |
			CIC_CONFIG_COMB_COUNT(cfg->mcic - 1);
		dmic->dmic_blob_pdm[i].cic_config = val;

		/* Mono right channel mic usage requires swap of PDM channels
		 * since the mono decimation is done with only left channel
		 * processing active.
		 */
		edge = dmic->dmic_prm[di].pdm[i].clk_edge;
		if (swap[i])
			edge = !edge;

		val = MIC_CONTROL_PDM_CLKDIV(cfg->clkdiv - 2) |
			MIC_CONTROL_PDM_SKEW(dmic->dmic_prm[di].pdm[i].skew) |
			MIC_CONTROL_CLK_EDGE(edge) |
			MIC_CONTROL_PDM_EN_B(1) |
			MIC_CONTROL_PDM_EN_A(1);
		dmic->dmic_blob_pdm[i].mic_control = val;

		/* if cfg->mfir_a */
		if (di == 0) {
			/* FIR A */
			fir_decim = MAX(cfg->mfir_a - 1, 0);
			fir_length = MAX(cfg->fir_a_length - 1, 0);
			val = FIR_CONTROL_A_START(1) |
				FIR_CONTROL_A_ARRAY_START_EN(array_a) |
				FIR_CONTROL_A_DCCOMP(dccomp) |
				FIR_CONTROL_A_MUTE(fir_mute) |
				FIR_CONTROL_A_STEREO(stereo[i]);
			dmic->dmic_blob_fir[i][di].fir_control = val;

			val = FIR_CONFIG_A_FIR_DECIMATION(fir_decim) |
				FIR_CONFIG_A_FIR_SHIFT(cfg->fir_a_shift) |
				FIR_CONFIG_A_FIR_LENGTH(fir_length);
			dmic->dmic_blob_fir[i][di].fir_config = val;

			val = DC_OFFSET_LEFT_A_DC_OFFS(DCCOMP_TC0);
			dmic->dmic_blob_fir[i][di].dc_offset_left = val;

			val = DC_OFFSET_RIGHT_A_DC_OFFS(DCCOMP_TC0);
			dmic->dmic_blob_fir[i][di].dc_offset_right = val;

			val = OUT_GAIN_LEFT_A_GAIN(0);
			dmic->dmic_blob_fir[i][di].out_gain_left = val;

			val = OUT_GAIN_RIGHT_A_GAIN(0);
			dmic->dmic_blob_fir[i][di].out_gain_right = val;

			/* Write coef RAM A with scaled coefficient in reverse order */
			length = cfg->fir_a_length;
			for (j = 0; j < length; j++) {
				ci = (int32_t)Q_MULTSR_32X32((int64_t)cfg->fir_a->coef[j],
							     cfg->fir_a_scale, 31,
							     DMIC_FIR_SCALE_Q, DMIC_HW_FIR_COEF_Q);
				cu = FIR_COEF_A(ci);
				/* blob_pdm[i].fir_coeffs[di][j] = cu; */
				dmic->dmic_fir_array.fir_coeffs[i][di][j] = cu;
			}
			dmic->dmic_fir_array.fir_len[0] = length;
			dmic->dmic_fir_array.fir_len[1] = 0;
		}

		if (di == 1) {
			/* FIR B */
			fir_decim = MAX(cfg->mfir_b - 1, 0);
			fir_length = MAX(cfg->fir_b_length - 1, 0);
			val = FIR_CONTROL_B_START(1) |
				FIR_CONTROL_B_ARRAY_START_EN(array_b) |
				FIR_CONTROL_B_DCCOMP(dccomp) |
				FIR_CONTROL_B_MUTE(fir_mute) |
				FIR_CONTROL_B_STEREO(stereo[i]);
			dmic->dmic_blob_fir[i][di].fir_control = val;

			val = FIR_CONFIG_B_FIR_DECIMATION(fir_decim) |
				FIR_CONFIG_B_FIR_SHIFT(cfg->fir_b_shift) |
				FIR_CONFIG_B_FIR_LENGTH(fir_length);
			dmic->dmic_blob_fir[i][di].fir_config = val;
			val = DC_OFFSET_LEFT_B_DC_OFFS(DCCOMP_TC0);
			dmic->dmic_blob_fir[i][di].dc_offset_left = val;

			val = DC_OFFSET_RIGHT_B_DC_OFFS(DCCOMP_TC0);
			dmic->dmic_blob_fir[i][di].dc_offset_right = val;

			val = OUT_GAIN_LEFT_B_GAIN(0);
			dmic->dmic_blob_fir[i][di].out_gain_left = val;

			val = OUT_GAIN_RIGHT_B_GAIN(0);
			dmic->dmic_blob_fir[i][di].out_gain_right = val;

			/* Write coef RAM B with scaled coefficient in reverse order */
			length = cfg->fir_b_length;
			for (j = 0; j < length; j++) {
				ci = (int32_t)Q_MULTSR_32X32((int64_t)cfg->fir_b->coef[j],
							     cfg->fir_b_scale, 31,
							     DMIC_FIR_SCALE_Q, DMIC_HW_FIR_COEF_Q);
				cu = FIR_COEF_B(ci);
				/* blob_pdm[i].fir_coeffs[di][j] = cu; */
				dmic->dmic_fir_array.fir_coeffs[i][di][j] = cu;
			}
			dmic->dmic_fir_array.fir_len[1] = length;
		}
	}

	return 0;
}

/* The decimation for PDM (pulse density modulation) stream is done in a programmable HW filter
 * engine. The input to configuration algorithm is needed sample rate, channels/enabled microphones,
 * microphone clock range, microphone clock duty cycle range, and system clock rate.
 *
 * The PDM bus clock divider, CIC and FIR decimation ratios are searched and configuration for
 * optimal power consumption, filtering requirements, and HW constraints is chosen. The FIR filter
 * for the chosen decimation is looked up from table and scaled to match the other decimation path
 * sensitivity.
 */
int dmic_calculate(struct intel_nhlt_params *nhlt)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;
	struct dmic_calc_matched_modes modes_ab;
	struct dmic_calc_decim_modes modes_a;
	struct dmic_calc_decim_modes modes_b;
	struct dmic_calc_configuration cfg;
	int ret = 0;
	int di;

	if (!dmic)
		return -EINVAL;

	di = dmic->dmic_dai_index;

	if (di >= DMIC_HW_FIFOS) {
		fprintf(stderr, "dmic_set_config(): dai->index exceeds number of FIFOs\n");
		ret = -EINVAL;
		goto out;
	}

	if (dmic->dmic_prm[di].num_pdm_active > DMIC_HW_CONTROLLERS) {
		fprintf(stderr, "dmic_set_config():controller count exceeds platform capability\n");
		ret = -EINVAL;
		goto out;
	}

	/* fifo bits 0 means fifo disabled */
	switch (dmic->dmic_prm[di].fifo_bits) {
	case 0:
	case 16:
	case 32:
		break;
	default:
		fprintf(stderr, "dmic_set_config(): fifo_bits EINVAL\n");
		ret = -EINVAL;
		goto out;
	}

	/* Match and select optimal decimators configuration for FIFOs A and B paths. This setup
	 * phase is still abstract. Successful completion points struct cfg to FIR coefficients and
	 * contains the scale value to use for FIR coefficient RAM write as well as the CIC and FIR
	 * shift values.
	 */
	find_modes(dmic, &modes_a, dmic->dmic_prm[0].fifo_fs);
	if (modes_a.num_of_modes == 0 && dmic->dmic_prm[0].fifo_fs > 0) {
		fprintf(stderr, "dmic_set_config(): No modes found for FIFO A\n");
		ret = -EINVAL;
		goto out;
	}

	find_modes(dmic, &modes_b, dmic->dmic_prm[1].fifo_fs);
	if (modes_b.num_of_modes == 0 && dmic->dmic_prm[1].fifo_fs > 0) {
		fprintf(stderr, "dmic_set_config(): No modes found for FIFO B\n");
		ret = -EINVAL;
		goto out;
	}

	match_modes(&modes_ab, &modes_a, &modes_b);
	ret = select_mode(dmic, &cfg, &modes_ab);
	if (ret < 0) {
		fprintf(stderr, "dmic_set_config(): select_mode() failed\n");
		ret = -EINVAL;
		goto out;
	}

	/* Struct reg contains a mirror of actual HW registers. Determine register bits
	 * configuration from decimator configuration and the requested parameters.
	 */
	ret = configure_registers(dmic, &cfg);
	if (ret < 0) {
		fprintf(stderr, "dmic_set_config(): cannot configure registers\n");
		ret = -EINVAL;
		goto out;
	}

	dmic_print_internal(dmic);

	dmic->dmic_count++;

out:
	return ret;
}

int dmic_get_params(struct intel_nhlt_params *nhlt, int index, uint32_t *sample_rate,
		    uint16_t *channel_count, uint32_t *bits_per_sample, uint8_t *array_type,
		    uint8_t *num_mics, uint8_t *extension, uint32_t *snr, uint32_t *sensitivity)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;
	uint32_t channels = 0;

	if (!dmic)
		return -EINVAL;

	/* check all pdm's for enabled mics */
	*channel_count = 0;
	if (dmic->dmic_prm[index].pdm[0].enable_mic_a)
		channels++;

	if (dmic->dmic_prm[index].pdm[0].enable_mic_b)
		channels++;

	if (dmic->dmic_prm[index].pdm[1].enable_mic_a)
		channels++;

	if (dmic->dmic_prm[index].pdm[1].enable_mic_b)
		channels++;

	*sample_rate = dmic->dmic_prm[index].fifo_fs;
	*channel_count = channels;
	*bits_per_sample = dmic->dmic_prm[index].fifo_bits;
	*num_mics = dmic->dmic_mic_config.num_mics;
	*extension = dmic->dmic_mic_config.extension;
	*array_type = dmic->dmic_mic_config.array_type;
	*snr = dmic->dmic_mic_config.snr;
	*sensitivity = dmic->dmic_mic_config.sensitivity;

	return 0;
}

int dmic_get_mic_params(struct intel_nhlt_params *nhlt, int index,
			uint8_t *type, uint8_t *panel, uint32_t *speaker_position_distance,
			uint32_t *horizontal_offset, uint32_t *vertical_offset,
			uint8_t *frequency_low_band, uint8_t *frequency_high_band,
			uint16_t *direction_angle, uint16_t *elevation_angle,
			uint16_t *vertical_angle_begin, uint16_t *vertical_angle_end,
			uint16_t *horizontal_angle_begin, uint16_t *horizontal_angle_end)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;

	if (!dmic)
		return -EINVAL;

	*type = dmic->dmic_mic_config.vendor[index].type;
	*panel = dmic->dmic_mic_config.vendor[index].panel;
	*speaker_position_distance = dmic->dmic_mic_config.vendor[index].speaker_position_distance;
	*horizontal_offset = dmic->dmic_mic_config.vendor[index].horizontal_offset;
	*vertical_offset = dmic->dmic_mic_config.vendor[index].vertical_offset;
	*frequency_low_band = dmic->dmic_mic_config.vendor[index].frequency_low_band;
	*frequency_high_band = dmic->dmic_mic_config.vendor[index].frequency_high_band;
	*direction_angle = dmic->dmic_mic_config.vendor[index].direction_angle;
	*elevation_angle = dmic->dmic_mic_config.vendor[index].elevation_angle;
	*vertical_angle_begin = dmic->dmic_mic_config.vendor[index].vertical_angle_begin;
	*vertical_angle_end = dmic->dmic_mic_config.vendor[index].vertical_angle_end;
	*horizontal_angle_begin = dmic->dmic_mic_config.vendor[index].horizontal_angle_begin;
	*horizontal_angle_end = dmic->dmic_mic_config.vendor[index].horizontal_angle_end;

	return 0;
}

int dmic_get_vendor_blob_size(struct intel_nhlt_params *nhlt, size_t *size)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;
	int i, fir_index_0, fir_index_1;

	if (!dmic || !dmic->dmic_count)
		return -EINVAL;

	*size = sizeof(struct dmic_intel_config_data);

	/* if either of the fir is 0 length, copy the existing fir twice */
	fir_index_0 = 0;
	fir_index_1 = 1;
	if (dmic->dmic_fir_array.fir_len[0] == 0) {
		fir_index_0 = 1;
		fir_index_1 = 1;
	}
	if (dmic->dmic_fir_array.fir_len[1] == 0) {
		fir_index_0 = 0;
		fir_index_1 = 0;
	}

	/* variable amount of pdm's */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		/* only copy the pdm data if it is enabled */
		if ((dmic->dmic_blob.channel_pdm_mask & BIT(i)) == 0)
			continue;

		*size += sizeof(struct dmic_intel_pdm_ctrl_cfg);
		*size += sizeof(struct dmic_intel_fir_config) * DMIC_HW_FIFOS;

		*size += dmic->dmic_fir_array.fir_len[fir_index_0] * sizeof(uint32_t);
		*size += dmic->dmic_fir_array.fir_len[fir_index_1] * sizeof(uint32_t);
	}

	return 0;
}

int dmic_get_vendor_blob_count(struct intel_nhlt_params *nhlt)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;

	if (!dmic || !dmic->dmic_count)
		return 0;

	return dmic->dmic_count;
}

int dmic_get_vendor_blob(struct intel_nhlt_params *nhlt, uint8_t *vendor_blob)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;
	int i, fir_index_0, fir_index_1;
	uint8_t *orig_blob = vendor_blob;
	size_t blob_size;

	if (!dmic || !dmic->dmic_count)
		return -EINVAL;

	/* top level struct */
	memcpy(vendor_blob, &dmic->dmic_blob, sizeof(struct dmic_intel_config_data));
	vendor_blob += sizeof(struct dmic_intel_config_data);

	/* if either of the fir is 0 length, copy the existing fir twice */
	fir_index_0 = 0;
	fir_index_1 = 1;
	if (dmic->dmic_fir_array.fir_len[0] == 0) {
		fir_index_0 = 1;
		fir_index_1 = 1;
	}
	if (dmic->dmic_fir_array.fir_len[1] == 0) {
		fir_index_0 = 0;
		fir_index_1 = 0;
	}

	/* variable amount of pdm's */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		/* only copy the pdm data if it is enabled */
		if ((dmic->dmic_blob.channel_pdm_mask & BIT(i)) == 0)
			continue;

		/* top level struct first pdm data */
		memcpy(vendor_blob, (uint8_t *)&dmic->dmic_blob_pdm[i],
		       sizeof(struct dmic_intel_pdm_ctrl_cfg));
		vendor_blob += sizeof(struct dmic_intel_pdm_ctrl_cfg);

		/* top level struct first pdm data first fir */
		memcpy(vendor_blob, (uint8_t *)&dmic->dmic_blob_fir[i][fir_index_0],
		       sizeof(struct dmic_intel_fir_config));
		vendor_blob += sizeof(struct dmic_intel_fir_config);

		/* top level struct first pdm data second fir */
		memcpy(vendor_blob, (uint8_t *)&dmic->dmic_blob_fir[i][fir_index_1],
		       sizeof(struct dmic_intel_fir_config));
		vendor_blob += sizeof(struct dmic_intel_fir_config);

		/* fir coeffs a */
		memcpy(vendor_blob, (uint8_t *)&dmic->dmic_fir_array.fir_coeffs[i][fir_index_0][0],
		       dmic->dmic_fir_array.fir_len[fir_index_0] * sizeof(uint32_t));
		vendor_blob += dmic->dmic_fir_array.fir_len[fir_index_0] * sizeof(uint32_t);

		/* fir coeffs b */
		memcpy(vendor_blob, (uint8_t *)&dmic->dmic_fir_array.fir_coeffs[i][fir_index_1][0],
		       dmic->dmic_fir_array.fir_len[fir_index_1] * sizeof(uint32_t));
		vendor_blob += dmic->dmic_fir_array.fir_len[fir_index_1] * sizeof(uint32_t);
	}

	dmic_get_vendor_blob_size(nhlt, &blob_size);
	dmic_print_bytes_as_hex((uint8_t *)orig_blob, blob_size);
	dmic_print_integers_as_hex((uint32_t *)orig_blob, blob_size / 4);

	return 0;
}

int dmic_set_params(struct intel_nhlt_params *nhlt, int dai_index, int driver_version,
		    int io_clk, int num_pdm_active, int fifo_word_length, int clk_min, int clk_max,
		    int duty_min, int duty_max, int sample_rate, int unmute_ramp_time)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;

	if (!dmic)
		return -EINVAL;

	if (dai_index >= DMIC_HW_FIFOS) {
		fprintf(stderr, "set_dmic_data illegal dai index\n");
		return -EINVAL;
	}

	dmic->dmic_dai_index = dai_index;
	dmic->dmic_prm[dai_index].driver_version = driver_version;
	dmic->dmic_prm[dai_index].io_clk = io_clk;
	dmic->dmic_prm[dai_index].num_pdm_active = num_pdm_active;
	dmic->dmic_prm[dai_index].fifo_bits = fifo_word_length;
	dmic->dmic_prm[dai_index].pdmclk_min = clk_min;
	dmic->dmic_prm[dai_index].pdmclk_max = clk_max;
	dmic->dmic_prm[dai_index].duty_min = duty_min;
	dmic->dmic_prm[dai_index].duty_max = duty_max;
	dmic->dmic_prm[dai_index].fifo_fs = sample_rate;
	dmic->dmic_prm[dai_index].unmute_ramp_time = unmute_ramp_time;

	return 0;
}

int dmic_set_pdm_params(struct intel_nhlt_params *nhlt, int pdm_index, int enable_a,
			int enable_b, int polarity_a, int polarity_b, int clk_edge, int skew)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;
	int di;

	if (!dmic)
		return -EINVAL;

	if (pdm_index >= DMIC_HW_CONTROLLERS) {
		fprintf(stderr, "set_pdm_data illegal pdm_index\n");
		return -EINVAL;
	}

	di = dmic->dmic_dai_index;

	dmic->dmic_prm[di].pdm[pdm_index].enable_mic_a = enable_a;
	dmic->dmic_prm[di].pdm[pdm_index].enable_mic_b = enable_b;
	dmic->dmic_prm[di].pdm[pdm_index].polarity_mic_a = polarity_a;
	dmic->dmic_prm[di].pdm[pdm_index].polarity_mic_b = polarity_b;
	dmic->dmic_prm[di].pdm[pdm_index].clk_edge = clk_edge;
	dmic->dmic_prm[di].pdm[pdm_index].skew = skew;

	return 0;
}

int dmic_set_ext_params(struct intel_nhlt_params *nhlt, uint32_t snr, uint32_t sensitivity)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;

	if (!dmic)
		return -EINVAL;

	dmic->dmic_mic_config.extension = 1;
	dmic->dmic_mic_config.snr = snr;
	dmic->dmic_mic_config.sensitivity = sensitivity;

	return 0;
}

int dmic_set_mic_params(struct intel_nhlt_params *nhlt, int index,
			uint8_t type, uint8_t panel, uint32_t speaker_position_distance,
			uint32_t horizontal_offset, uint32_t vertical_offset,
			uint8_t frequency_low_band, uint8_t frequency_high_band,
			uint16_t direction_angle, uint16_t elevation_angle,
			uint16_t vertical_angle_begin, uint16_t vertical_angle_end,
			uint16_t horizontal_angle_begin, uint16_t horizontal_angle_end)
{
	struct intel_dmic_params *dmic = (struct intel_dmic_params *)nhlt->dmic_params;

	if (!dmic)
		return -EINVAL;

	dmic->dmic_mic_config.vendor[index].type = type;
	dmic->dmic_mic_config.vendor[index].panel = panel;
	dmic->dmic_mic_config.vendor[index].speaker_position_distance = speaker_position_distance;
	dmic->dmic_mic_config.vendor[index].horizontal_offset = horizontal_offset;
	dmic->dmic_mic_config.vendor[index].vertical_offset = vertical_offset;
	dmic->dmic_mic_config.vendor[index].frequency_low_band = frequency_low_band;
	dmic->dmic_mic_config.vendor[index].frequency_high_band = frequency_high_band;
	dmic->dmic_mic_config.vendor[index].direction_angle = direction_angle;
	dmic->dmic_mic_config.vendor[index].elevation_angle = elevation_angle;
	dmic->dmic_mic_config.vendor[index].vertical_angle_begin = vertical_angle_begin;
	dmic->dmic_mic_config.vendor[index].vertical_angle_end = vertical_angle_end;
	dmic->dmic_mic_config.vendor[index].horizontal_angle_begin = horizontal_angle_begin;
	dmic->dmic_mic_config.vendor[index].horizontal_angle_end = horizontal_angle_end;

	dmic->dmic_mic_config.num_mics++;

	return 0;
}

/* init dmic parameters, should be called before parsing dais */
int dmic_init_params(struct intel_nhlt_params *nhlt)
{
	struct intel_dmic_params *dmic;
	int i;

	dmic = calloc(1, sizeof(struct intel_dmic_params));
	if (!dmic)
		return -ENOMEM;

	nhlt->dmic_params = dmic;
	/* set always to 1, some fw variants use this for choosing memory type */
	dmic->dmic_blob.gateway_attributes = 1;
	/* delay in ms to unmute mics after clock is started */
	dmic->dmic_blob.clock_on_delay = 16;

	for (i = 0; i < DMIC_TS_GROUP_SIZE; i++)
		dmic->dmic_blob.ts_group[i] = 0xFFFFFFFF; /* not enabled */

	dmic->dmic_count = 0;

	dmic->dmic_mic_config.num_mics = 0;
	dmic->dmic_mic_config.extension = 0;
	dmic->dmic_mic_config.array_type = 0;
	dmic->dmic_mic_config.snr = 0;
	dmic->dmic_mic_config.sensitivity = 0;

	return 0;
}
