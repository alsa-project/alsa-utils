/*
  st2095.c

  Generate Bandlimited Pink Noise (-18.5dB AES FS)
  Using the SMPTE ST 2095:1-2015 standard

  Based on pseudo-code from the above SMPTE standard, which bore the credit
  "Revised 2015-01-04 by Calvert Dayton"

  Copyleft 2023 Rick Sayre - No rights reserved.
*/

#include "aconfig.h"
#include <stdio.h>
#include <math.h>
#include "st2095.h"

/************************************************************/


void reset_st2095_noise_measurement( st2095_noise_t *st2095 ) {
    st2095->accum = 0.;
}

float compute_st2095_noise_measurement( st2095_noise_t *st2095, int period ) {
    return(10. * log10f(st2095->accum / (float)period) + 3.01);
}

void initialize_st2095_noise( st2095_noise_t *st2095, int sample_rate) {
    // Periodicity in samples must be a power of two, <= 2^31
    // Typical values are 524288, 1048576, 2097152 or 4194304
    if (sample_rate > 48000) {
	// Special case LCG step for 1024K samples @ 88.2K or 96k
	st2095->samplesPerPeriod = 1048576;
	st2095->randStep = 163841;
    } else {
	st2095->samplesPerPeriod = 524288;
	st2095->randStep = 52737;
    }

    // set up LCG PRNG
    st2095->randMax = st2095->samplesPerPeriod - 1;
    st2095->seed = 0;
    st2095->scaleFactor = 2.0 / (float)st2095->randMax;

    st2095->maxAmp = powf(10.0, ST2095_MAX_PEAK / 20.0);

    // Calculate omegaT for matched Z transform highpass filters
    st2095->w0t = 2.0 * M_PI * ST2095_HPFC / (float)sample_rate;

    //  Limit LpFc <= Nyquist (actually lower, based on 48 vs 22.4 KHz spec cutoff)
    //          The spec says the filter begins at 22.4KHz, if we ask for a Nyquist-impossible
    //          sampling rate, compute something with the same relationship
    st2095->LpFc = ST2095_LPFC;
    float rateratio = 48000. / ST2095_LPFC;
    if (st2095->LpFc > sample_rate/rateratio)
	st2095->LpFc = sample_rate/rateratio;

    // Calculate k and k^2 for bilinear transform lowpass filters
    st2095->k = tanf(( 2.0 * M_PI * st2095->LpFc / (float)sample_rate ) / 2.0);
    st2095->k2 = st2095->k * st2095->k;

    // Calculate biquad coefficients for bandpass filter components
    st2095->hp1_a1 = -2.0 * expf(-0.3826835 * st2095->w0t) * cosf(0.9238795 * st2095->w0t);
    st2095->hp1_a2 = expf(2.0 * -0.3826835 * st2095->w0t);
    st2095->hp1_b0 = (1.0 - st2095->hp1_a1 + st2095->hp1_a2) / 4.0;
    st2095->hp1_b1 = -2.0 * st2095->hp1_b0;
    st2095->hp1_b2 = st2095->hp1_b0;

    st2095->hp2_a1 = -2.0 * expf(-0.9238795 * st2095->w0t) * cosf(0.3826835 * st2095->w0t);
    st2095->hp2_a2 = expf(2.0 * -0.9238795 * st2095->w0t);
    st2095->hp2_b0 = (1.0 - st2095->hp2_a1 + st2095->hp2_a2) / 4.0;
    st2095->hp2_b1 = -2.0 * st2095->hp2_b0;
    st2095->hp2_b2 = st2095->hp2_b0;

    st2095->lp1_a1 = (2.0 * (st2095->k2 - 1.0)) / (st2095->k2 + (st2095->k / 1.306563) + 1.0);
    st2095->lp1_a2 = (st2095->k2 - (st2095->k / 1.306563) + 1.0) / (st2095->k2 + (st2095->k / 1.306563) + 1.0);
    st2095->lp1_b0 = st2095->k2 / (st2095->k2 + (st2095->k / 1.306563) + 1.0);
    st2095->lp1_b1 = 2.0 * st2095->lp1_b0;
    st2095->lp1_b2 = st2095->lp1_b0;

    st2095->lp2_a1 = (2.0 * (st2095->k2 - 1.0)) / (st2095->k2 + (st2095->k / 0.541196) + 1.0);
    st2095->lp2_a2 = (st2095->k2 - (st2095->k / 0.541196) + 1.0) / (st2095->k2 + (st2095->k / 0.541196) + 1.0);
    st2095->lp2_b0 = st2095->k2 / (st2095->k2 + (st2095->k / 0.541196) + 1.0);
    st2095->lp2_b1 = 2.0 * st2095->lp2_b0;
    st2095->lp2_b2 = st2095->lp2_b0;

    // initialize delay lines for bandpass filter
    st2095->hp1w1 = 0.0;
    st2095->hp1w2 = 0.0;
    st2095->hp2w1 = 0.0;
    st2095->hp2w2 = 0.0;
    st2095->lp1w1 = 0.0;
    st2095->lp1w2 = 0.0;
    st2095->lp2w1 = 0.0;
    st2095->lp2w2 = 0.0;

    // initialize delay lines for pink filter network
    st2095->lp1 = 0.0;
    st2095->lp2 = 0.0;
    st2095->lp3 = 0.0;
    st2095->lp4 = 0.0;
    st2095->lp5 = 0.0;
    st2095->lp6 = 0.0;

    // cycle the generator for one complete time series to populate filter-bank delay lines
    for (int i=0; i<st2095->samplesPerPeriod; i++)
	generate_st2095_noise_sample(st2095);
    st2095->accum = 0.0;
}

float generate_st2095_noise_sample( st2095_noise_t *st2095 ) {
    float white, w, pink;

    // Generate a pseudorandom integer in the range 0 <= seed <= randMax.
    //# Bitwise AND with randMax zeroes out any unwanted high order bits.
    st2095->seed = (1664525 * st2095->seed + st2095->randStep) & st2095->randMax;
    // Scale to a real number in the range -1.0 <= white <= 1.0
    white = (float)st2095->seed * st2095->scaleFactor - 1.0;

    // Run pink filter; a parallel network of first-order LP filters, scaled to
    // produce an output signal with target RMS = -21.5 dB FS (-18.5 dB AES FS)
    // when bandpass filter cutoff frequencies are 10 Hz and 22.4 kHz.
    st2095->lp1 = 0.9994551 * st2095->lp1 + 0.00198166688621989 * white;
    st2095->lp2 = 0.9969859 * st2095->lp2 + 0.00263702334184061 * white;
    st2095->lp3 = 0.9844470 * st2095->lp3 + 0.00643213710202331 * white;
    st2095->lp4 = 0.9161757 * st2095->lp4 + 0.01438952538362820 * white;
    st2095->lp5 = 0.6563399 * st2095->lp5 + 0.02698408541064610 * white;
    pink = st2095->lp1 + st2095->lp2 + st2095->lp3 +
	st2095->lp4 + st2095->lp5 + st2095->lp6 + white * 0.0342675832159306;
    st2095->lp6 = white * 0.0088766118009356;

    // Run bandpass filter; a series network of 4 biquad filters
    // Biquad filters implemented in Direct Form II
    w = pink - st2095->hp1_a1 * st2095->hp1w1 - st2095->hp1_a2 * st2095->hp1w2;
    pink = st2095->hp1_b0 * w + st2095->hp1_b1 * st2095->hp1w1 + st2095->hp1_b2 * st2095->hp1w2;
    st2095->hp1w2 = st2095->hp1w1;
    st2095->hp1w1 = w;

    w = pink - st2095->hp2_a1 * st2095->hp2w1 - st2095->hp2_a2 * st2095->hp2w2;
    pink = st2095->hp2_b0 * w + st2095->hp2_b1 * st2095->hp2w1 + st2095->hp2_b2 * st2095->hp2w2;
    st2095->hp2w2 = st2095->hp2w1;
    st2095->hp2w1 = w;

    w = pink - st2095->lp1_a1 * st2095->lp1w1 - st2095->lp1_a2 * st2095->lp1w2;
    pink = st2095->lp1_b0 * w + st2095->lp1_b1 * st2095->lp1w1 + st2095->lp1_b2 * st2095->lp1w2;
    st2095->lp1w2 = st2095->lp1w1;
    st2095->lp1w1 = w;

    w = pink - st2095->lp2_a1 * st2095->lp2w1 - st2095->lp2_a2 * st2095->lp2w2;
    pink = st2095->lp2_b0 * w + st2095->lp2_b1 * st2095->lp2w1 + st2095->lp2_b2 * st2095->lp2w2;
    st2095->lp2w2 = st2095->lp2w1;
    st2095->lp2w1 = w;

    // Limit peaks to +/-MaxAmp
    if (pink > st2095->maxAmp)
	pink = st2095->maxAmp;
    else if (pink < -st2095->maxAmp)
	pink = -st2095->maxAmp;

    // accumulate squared amplitude for RMS computation
    st2095->accum += (pink * pink);
    return(pink);
}
