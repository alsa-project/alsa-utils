/*
 * Copyright (C) 2000-2004 James Courtier-Dutton
 *
 * This file is part of the speaker-test tool.
 *
 * This small program sends a simple sinusoidal wave to your speakers.
 *
 * speaker-test is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * speaker-test is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * Main program by James Courtier-Dutton (including some source code fragments from the alsa project.)
 * Some cleanup from Daniel Caujolle-Bert <segfault@club-internet.fr>
 *
 * $Id: speaker_test.c,v 1.00 2003/11/26 19:43:38 jcdutton Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include "aconfig.h"

#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>

static char              *device      = "plughw:0,0";       /* playback device */
static snd_pcm_format_t   format      = SND_PCM_FORMAT_S16; /* sample format */
static unsigned int       rate        = 48000;	            /* stream rate */
static unsigned int       channels    = 1;	            /* count of channels */
static unsigned int       speaker     = 0;	            /* count of channels */
static unsigned int       buffer_time = 500000;	            /* ring buffer length in us */
static unsigned int       period_time = 100000;	            /* period time in us */
static double             freq        = 440;                /* sinusoidal wave frequency in Hz */
static snd_output_t      *output      = NULL;
static snd_pcm_uframes_t  buffer_size;
static snd_pcm_uframes_t  period_size;
static const char        *channel_name[] = {
  "Front Left" ,
  "Front Right" ,
  "Rear Left" ,
  "Rear Right" ,
  "Center" ,
  "LFE" 
};

static void generate_sine(signed short *samples, int channel, int count, double *_phase) {
  double phase = *_phase;
  double max_phase = 1.0 / freq;
  double step = 1.0 / (double)rate;
  double res;
  int    chn, ires;

  while (count-- > 0) {
    res = sin((phase * 2 * M_PI) / max_phase - M_PI) * 32767;
    /* printf("%e\n",res); */
    ires = res;

    for(chn=0;chn<channels;chn++) {
      if (chn==channel)
	*samples++ = ires;
      else
	*samples++ = 0;
    }

    phase += step;
    if (phase >= max_phase)
      phase -= max_phase;
  }

  *_phase = phase;
}

/* FIXME: Implement, because it is a better test than sine wave
 * because we can tell where pink noise is coming from more easily that a sine wave
 */
#if 0
static void generate_pink_noise( snd_pcm_uframes_t offset, int count, double *_phase) {
}
#endif

static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access) {
  unsigned int rrate;
  int          err, dir;
  snd_pcm_uframes_t     period_size_min;
  snd_pcm_uframes_t     period_size_max;
  snd_pcm_uframes_t     buffer_size_min;
  snd_pcm_uframes_t     buffer_size_max;
  snd_pcm_uframes_t     buffer_time_to_size;



  /* choose all parameters */
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
    return err;
  }

  /* set the interleaved read/write format */
  err = snd_pcm_hw_params_set_access(handle, params, access);
  if (err < 0) {
    printf("Access type not available for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* set the sample format */
  err = snd_pcm_hw_params_set_format(handle, params, format);
  if (err < 0) {
    printf("Sample format not available for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* set the count of channels */
  err = snd_pcm_hw_params_set_channels(handle, params, channels);
  if (err < 0) {
    printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
    return err;
  }

  /* set the stream rate */
  rrate = rate;
  err = snd_pcm_hw_params_set_rate(handle, params, rate, 0);
  if (err < 0) {
    printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
    return err;
  }

  if (rrate != rate) {
    printf("Rate doesn't match (requested %iHz, get %iHz, err %d)\n", rate, rrate, err);
    return -EINVAL;
  }

  printf("Rate set to %iHz (requested %iHz)\n", rrate, rate);
  /* set the buffer time */
  buffer_time_to_size = ( (snd_pcm_uframes_t)buffer_time * rate) / 1000000;
  err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
  err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
  dir=0;
  err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min,&dir);
  dir=0;
  err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max,&dir);
  printf("Buffer size range from %lu to %lu\n",buffer_size_min, buffer_size_max);
  printf("Period size range from %lu to %lu\n",period_size_min, period_size_max);
  printf("Buffer time size %lu\n",buffer_time_to_size);

  buffer_size = buffer_time_to_size;
  if (buffer_size_max < buffer_size) buffer_size = buffer_size_max;
  if (buffer_size_min > buffer_size) buffer_size = buffer_size_min;
  period_size=buffer_size/8;
  buffer_size = period_size*8;
  printf("To choose buffer_size = %lu\n",buffer_size);
  printf("To choose period_size = %lu\n",period_size);
  dir=0;
  err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, &dir);
  if (err < 0) {
    printf("Unable to set period size %lu for playback: %s\n", period_size, snd_strerror(err));
    return err;
  }
  dir=0;
  err = snd_pcm_hw_params_get_period_size(params, &period_size, &dir);
  if (err < 0)  printf("Unable to get period size for playback: %s\n", snd_strerror(err));
                                                                                                                             
  dir=0;
  err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
  if (err < 0) {
    printf("Unable to set buffer size %lu for playback: %s\n", buffer_size, snd_strerror(err));
    return err;
  }
  err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
  printf("was set period_size = %lu\n",period_size);
  printf("was set buffer_size = %lu\n",buffer_size);
  if (2*period_size > buffer_size) {
    printf("buffer to small, could not use\n");
    return err;
  }


  /* write the parameters to device */
  err = snd_pcm_hw_params(handle, params);
  if (err < 0) {
    printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
    return err;
  }

  return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams) {
  int err;

  /* get the current swparams */
  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0) {
    printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* start the transfer when a period is full */
  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, period_size);
  if (err < 0) {
    printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* allow the transfer when at least period_size samples can be processed */
  err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
  if (err < 0) {
    printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* align all transfers to 1 sample */
  err = snd_pcm_sw_params_set_xfer_align(handle, swparams, 1);
  if (err < 0) {
    printf("Unable to set transfer align for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* write the parameters to the playback device */
  err = snd_pcm_sw_params(handle, swparams);
  if (err < 0) {
    printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
    return err;
  }

  return 0;
}

/*
 *   Underrun and suspend recovery
 */

static int xrun_recovery(snd_pcm_t *handle, int err) {
  if (err == -EPIPE) {	/* under-run */
    err = snd_pcm_prepare(handle);
    if (err < 0)
      printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
    return 0;
  } 
  else if (err == -ESTRPIPE) {

    while ((err = snd_pcm_resume(handle)) == -EAGAIN)
      sleep(1);	/* wait until the suspend flag is released */

    if (err < 0) {
      err = snd_pcm_prepare(handle);
      if (err < 0)
        printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
    }

    return 0;
  }

  return err;
}

/*
 *   Transfer method - write only
 */

static int write_loop(snd_pcm_t *handle, int channel, int periods, signed short *samples) {
  double phase = 0;
  signed short *ptr;
  int    err, cptr, n;

  for(n = 0; n < periods; n++) {

    generate_sine(samples, channel, period_size, &phase);
    ptr = samples;
    cptr = period_size;

    while (cptr > 0) {

      err = snd_pcm_writei(handle, ptr, cptr);

      if (err == -EAGAIN)
        continue;

      if (err < 0) {
        if (xrun_recovery(handle, err) < 0) {
          printf("Write error: %s\n", snd_strerror(err));
	  return -1;
        }
        break;	/* skip one period */
      }

      ptr += (err * channels);
      cptr -= err;
    }
  }

  return 0;
}

static void help(void) {

  printf(
      "Usage: speaker-test [OPTION]... \n"
      "-h,--help	help\n"
      "-D,--device	playback device\n"
      "-r,--rate	stream rate in Hz\n"
      "-c,--channels	count of channels in stream\n"
      "-f,--frequency	sine wave frequency in Hz\n"
      "-b,--buffer	ring buffer size in us\n"
      "-p,--period	period size in us\n"
      "-s,--speaker	single speaker test. Values 1=Left or 2=right\n"
      "\n");
#if 0
  printf("Recognized sample formats are:");
  for (k = 0; k < SND_PCM_FORMAT_LAST; ++k) {
    const char *s = snd_pcm_format_name(k);
    if (s)
      printf(" %s", s);
  }

  printf("\n\n");
#endif

}

int main(int argc, char *argv[]) {
  snd_pcm_t            *handle;
  int                   err, morehelp;
  snd_pcm_hw_params_t  *hwparams;
  snd_pcm_sw_params_t  *swparams;
  signed short         *samples;
  int                   chn;
  struct option         long_option[] = {
    {"help",      0, NULL, 'h'},
    {"device",    1, NULL, 'D'},
    {"rate",      1, NULL, 'r'},
    {"channels",  1, NULL, 'c'},
    {"frequency", 1, NULL, 'f'},
    {"buffer",    1, NULL, 'b'},
    {"period",    1, NULL, 'p'},
    {"speaker",    1, NULL, 's'},
    {NULL,        0, NULL, 0  },
  };

  snd_pcm_hw_params_alloca(&hwparams);
  snd_pcm_sw_params_alloca(&swparams);
  
  morehelp = 0;

  printf("\nspeaker-test %s\n\n",VERSION);
  while (1) {
    int c;
    
    if ((c = getopt_long(argc, argv, "hD:r:c:f:b:p:s:", long_option, NULL)) < 0)
      break;
    
    switch (c) {
    case 'h':
      morehelp++;
      break;
    case 'D':
      device = strdup(optarg);
      break;
    case 'r':
      rate = atoi(optarg);
      rate = rate < 4000 ? 4000 : rate;
      rate = rate > 196000 ? 196000 : rate;
      break;
    case 'c':
      channels = atoi(optarg);
      channels = channels < 1 ? 1 : channels;
      channels = channels > 1024 ? 1024 : channels;
      break;
    case 'f':
      freq = atoi(optarg);
      freq = freq < 50 ? 50 : freq;
      freq = freq > 5000 ? 5000 : freq;
      break;
    case 'b':
      buffer_time = atoi(optarg);
      buffer_time = buffer_time < 1000 ? 1000 : buffer_time;
      buffer_time = buffer_time > 1000000 ? 1000000 : buffer_time;
      break;
    case 'p':
      period_time = atoi(optarg);
      period_time = period_time < 1000 ? 1000 : period_time;
      period_time = period_time > 1000000 ? 1000000 : period_time;
      break;
    case 's':
      speaker = atoi(optarg);
      speaker = speaker < 1 ? 0 : speaker;
      speaker = speaker > channels ? 0 : speaker;
      if (speaker==0) {
        printf("Invalid parameter for -s option.\n");
        exit(EXIT_FAILURE);
      }  
      break;
    default:
      printf("Unknown option '%c'\n", c);
      exit(EXIT_FAILURE);
      break;
    }
  }

  if (morehelp) {
    help();
    exit(EXIT_SUCCESS);
  }

  err = snd_output_stdio_attach(&output, stdout, 0);
  if (err < 0) {
    printf("Output failed: %s\n", snd_strerror(err));
    exit(EXIT_FAILURE);
  }

  printf("Playback device is %s\n", device);
  printf("Stream parameters are %iHz, %s, %i channels\n", rate, snd_pcm_format_name(format), channels);
  printf("Sine wave rate is %.4fHz\n", freq);

  if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    printf("Playback open error: %s\n", snd_strerror(err));
    exit(EXIT_FAILURE);
  }

  if ((err = set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    printf("Setting of hwparams failed: %s\n", snd_strerror(err));
    exit(EXIT_FAILURE);
  }
  if ((err = set_swparams(handle, swparams)) < 0) {
    printf("Setting of swparams failed: %s\n", snd_strerror(err));
    exit(EXIT_FAILURE);
  }

  samples = malloc((period_size * channels * snd_pcm_format_width(format)) / 8);
  if (samples == NULL) {
    printf("No enough memory\n");
    exit(EXIT_FAILURE);
  }
  if (speaker==0) {
    while (1) {

      for(chn = 0; chn < channels; chn++) {
        printf("  - %s\n", channel_name[chn]);

        err = write_loop(handle, chn, ((rate*5)/period_size), samples);

        if (err < 0) {
          printf("Transfer failed: %s\n", snd_strerror(err));
	  exit(EXIT_FAILURE);
        }
      }
    }
  } else {
    printf("  - %s\n", channel_name[speaker-1]);
    err = write_loop(handle, speaker-1, ((rate*5)/period_size), samples);

    if (err < 0) {
      printf("Transfer failed: %s\n", snd_strerror(err));
    }
  }


  free(samples);
  snd_pcm_close(handle);

  exit(EXIT_SUCCESS);
}
