#define ST2095_MAX_PEAK -9.5    // dB
#define ST2095_HPFC     10.0    // Highpass filter cutoff in Hz
#define ST2095_LPFC     22400.0 // Lowpass filter cutoff in Hz

typedef struct
{
  float maxAmp;
  int samplesPerPeriod;
  int randStep;
  int randMax;
  int seed;
  float scaleFactor;
  float w0t;
  float k;
  float k2;
  float LpFc;
  // biquad coefficients
  float hp1_a1, hp1_a2;
  float hp1_b0, hp1_b1, hp1_b2;
  float hp2_a1, hp2_a2;
  float hp2_b0, hp2_b1, hp2_b2;
  float lp1_a1, lp1_a2;
  float lp1_b0, lp1_b1, lp1_b2;
  float lp2_a1, lp2_a2;
  float lp2_b0, lp2_b1, lp2_b2;
  // delay-line variables for bandpass filter
  float hp1w1, hp1w2;
  float hp2w1, hp2w2;
  float lp1w1, lp1w2;
  float lp2w1, lp2w2;
  // delay-line variables for pink filter network
  float lp1, lp2, lp3, lp4, lp5, lp6;
  // statistics accumulator
  float accum;
} st2095_noise_t;

void initialize_st2095_noise( st2095_noise_t *st2095, int sample_rate );
float generate_st2095_noise_sample( st2095_noise_t *st2095 );

void reset_st2095_noise_measurement( st2095_noise_t *st2095 );
float compute_st2095_noise_measurement( st2095_noise_t *st2095, int period );
