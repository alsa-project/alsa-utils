/*
 * Copyright (C) 2000-2004 James Courtier-Dutton
 * Copyright (C) 2005 Nathan Hurst
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
 * Pink noise option added Nathan Hurst, 
 *   based on generator by Phil Burk (pink.c)
 *
 * Changelog:
 *   0.0.8 Added support for pink noise output.
 * Changelog:
 *   0.0.7 Added support for more than 6 channels.
 * Changelog:
 *   0.0.6 Added support for different sample formats.
 *
 * $Id: speaker_test.c,v 1.00 2003/11/26 19:43:38 jcdutton Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <ctype.h>
#include "bswap.h"
#include <signal.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
#include "pink.h"
#include "aconfig.h"
#include "gettext.h"
#include "version.h"
#include "os_compat.h"

#ifdef ENABLE_NLS
#include <locale.h>
#endif

#ifdef SND_CHMAP_API_VERSION
#define CONFIG_SUPPORT_CHMAP	1
#endif

enum {
  TEST_PINK_NOISE = 1,
  TEST_SINE,
  TEST_WAV,
  TEST_PATTERN,
};

#define MAX_CHANNELS	16

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define LE_SHORT(v)		(v)
#define LE_INT(v)		(v)
#define BE_SHORT(v)		bswap_16(v)
#define BE_INT(v)		bswap_32(v)
#else /* __BIG_ENDIAN */
#define COMPOSE_ID(a,b,c,d)	((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define LE_SHORT(v)		bswap_16(v)
#define LE_INT(v)		bswap_32(v)
#define BE_SHORT(v)		(v)
#define BE_INT(v)		(v)
#endif

#define ARRAY_SIZE(x) (int)(sizeof(x)/sizeof(x[0]))

static char              *device      = "default";       /* playback device */
static snd_pcm_format_t   format      = SND_PCM_FORMAT_S16; /* sample format */
static unsigned int       rate        = 48000;	            /* stream rate */
static unsigned int       channels    = 1;	            /* count of channels */
static unsigned int       speaker     = 0;	            /* count of channels */
static unsigned int       buffer_time = 0;	            /* ring buffer length in us */
static unsigned int       period_time = 0;	            /* period time in us */
static unsigned int       nperiods    = 4;                  /* number of periods */
static double             freq        = 440.0;              /* sinusoidal wave frequency in Hz */
static int                test_type   = TEST_PINK_NOISE;    /* Test type. 1 = noise, 2 = sine wave */
static float              generator_scale  = 0.8;           /* Scale to use for sine volume */
static snd_pcm_uframes_t  buffer_size;
static snd_pcm_uframes_t  period_size;
static const char *given_test_wav_file = NULL;
static char *wav_file_dir = SOUNDSDIR;
static int debug = 0;
static int force_frequency = 0;
static int in_aborting = 0;
static snd_pcm_t *pcm_handle = NULL;

#ifdef CONFIG_SUPPORT_CHMAP
static snd_pcm_chmap_t *channel_map;
static int channel_map_set;
static int *ordered_channels;
#endif

static const char *const channel_name[MAX_CHANNELS] = {
  /*  0 */ N_("Front Left"),
  /*  1 */ N_("Front Right"),
  /*  2 */ N_("Rear Left"),
  /*  3 */ N_("Rear Right"),
  /*  4 */ N_("Center"),
  /*  5 */ N_("LFE"),
  /*  6 */ N_("Side Left"),
  /*  7 */ N_("Side Right"),
  /*  8 */ N_("Channel 9"),
  /*  9 */ N_("Channel 10"),
  /* 10 */ N_("Channel 11"),
  /* 11 */ N_("Channel 12"),
  /* 12 */ N_("Channel 13"),
  /* 13 */ N_("Channel 14"),
  /* 14 */ N_("Channel 15"),
  /* 15 */ N_("Channel 16")
};

static const int	channels4[] = {
  0, /* Front Left  */
  1, /* Front Right */
  3, /* Rear Right  */
  2, /* Rear Left   */
};
static const int	channels6[] = {
  0, /* Front Left  */
  4, /* Center      */
  1, /* Front Right */
  3, /* Rear Right  */
  2, /* Rear Left   */
  5, /* LFE         */
};
static const int	channels8[] = {
  0, /* Front Left  */
  4, /* Center      */
  1, /* Front Right */
  7, /* Side Right  */
  3, /* Rear Right  */
  2, /* Rear Left   */
  6, /* Side Left   */
  5, /* LFE         */
};

#ifdef CONFIG_SUPPORT_CHMAP
/* circular clockwise and bottom-to-top order */
static const int channel_order[] = {
  [SND_CHMAP_FLW]  =  10,
  [SND_CHMAP_FL]   =  20,
  [SND_CHMAP_TFL]  =  30,
  [SND_CHMAP_FLC]  =  40,
  [SND_CHMAP_TFLC] =  50,
  [SND_CHMAP_FC]   =  60,
  [SND_CHMAP_TFC]  =  70,
  [SND_CHMAP_FRC]  =  80,
  [SND_CHMAP_TFRC] =  90,
  [SND_CHMAP_FR]   = 100,
  [SND_CHMAP_TFR]  = 110,
  [SND_CHMAP_FRW]  = 120,
  [SND_CHMAP_SR]   = 130,
  [SND_CHMAP_TSR]  = 140,
  [SND_CHMAP_RR]   = 150,
  [SND_CHMAP_TRR]  = 160,
  [SND_CHMAP_RRC]  = 170,
  [SND_CHMAP_RC]   = 180,
  [SND_CHMAP_TRC]  = 190,
  [SND_CHMAP_RLC]  = 200,
  [SND_CHMAP_RL]   = 210,
  [SND_CHMAP_TRL]  = 220,
  [SND_CHMAP_SL]   = 230,
  [SND_CHMAP_TSL]  = 240,
  [SND_CHMAP_BC]   = 250,
  [SND_CHMAP_TC]   = 260,
  [SND_CHMAP_LLFE] = 270,
  [SND_CHMAP_LFE]  = 280,
  [SND_CHMAP_RLFE] = 290,
  /* not in table  = 10000 */
  [SND_CHMAP_UNKNOWN] = 20000,
  [SND_CHMAP_NA]      = 30000,
};

static int chpos_cmp(const void *chnum1p, const void *chnum2p)
{
  int chnum1 = *(int *)chnum1p;
  int chnum2 = *(int *)chnum2p;
  int chpos1 = channel_map->pos[chnum1];
  int chpos2 = channel_map->pos[chnum2];
  int weight1 = 10000;
  int weight2 = 10000;

  if (chpos1 < ARRAY_SIZE(channel_order) && channel_order[chpos1])
    weight1 = channel_order[chpos1];
  if (chpos2 < ARRAY_SIZE(channel_order) && channel_order[chpos2])
    weight2 = channel_order[chpos2];

  if (weight1 == weight2) {
    /* order by channel number if both have the same position (e.g. UNKNOWN)
     * or if neither is in channel_order[] */
    return chnum1 - chnum2;
  }

  /* order according to channel_order[] */
  return weight1 - weight2;
}

static int *order_channels(void)
{
  /* create a (playback order => channel number) table with channels ordered
   * according to channel_order[] values */
  int i;
  int *ordered_chs;

  ordered_chs = calloc(channel_map->channels, sizeof(*ordered_chs));
  if (!ordered_chs)
    return NULL;

  for (i = 0; i < channel_map->channels; i++)
    ordered_chs[i] = i;

  qsort(ordered_chs, channel_map->channels, sizeof(*ordered_chs), chpos_cmp);

  return ordered_chs;
}
#endif

static int get_speaker_channel(int chn)
{
#ifdef CONFIG_SUPPORT_CHMAP
  if (channel_map_set || (ordered_channels && chn >= channel_map->channels))
    return chn;
  if (ordered_channels)
    return ordered_channels[chn];
#endif

  switch (channels) {
  case 4:
    chn = channels4[chn];
    break;
  case 6:
    chn = channels6[chn];
    break;
  case 8:
    chn = channels8[chn];
    break;
  }

  return chn;
}

static const char *get_channel_name(int chn)
{
#ifdef CONFIG_SUPPORT_CHMAP
  if (channel_map) {
    const char *name = NULL;
    if (chn < channel_map->channels)
      name = snd_pcm_chmap_long_name(channel_map->pos[chn]);
    return name ? name : "Unknown";
  }
#endif
  return gettext(channel_name[chn]);
}

static const int	supported_formats[] = {
  SND_PCM_FORMAT_S8,
  SND_PCM_FORMAT_S16_LE,
  SND_PCM_FORMAT_S16_BE,
  SND_PCM_FORMAT_FLOAT_LE,
  SND_PCM_FORMAT_S24_3LE,
  SND_PCM_FORMAT_S24_3BE,
  SND_PCM_FORMAT_S24_LE,
  SND_PCM_FORMAT_S24_BE,
  SND_PCM_FORMAT_S32_LE,
  SND_PCM_FORMAT_S32_BE,
  -1
};

typedef union {
  float f;
  int32_t i;
} value_t;

static void do_generate(uint8_t *frames, int channel, int count,
			value_t (*generate)(void *), void *arg)
{
  value_t res;
  int    chn;
  int8_t *samp8 = (int8_t*) frames;
  int16_t *samp16 = (int16_t*) frames;
  int32_t *samp32 = (int32_t*) frames;
  float   *samp_f = (float*) frames;

  while (count-- > 0) {
    for(chn=0;chn<channels;chn++) {
      if (chn==channel) {
	res = generate(arg);
      } else {
	res.i = 0;
      }

      switch (format) {
      case SND_PCM_FORMAT_S8:
	*samp8++ = res.i >> 24;
        break;
      case SND_PCM_FORMAT_S16_LE:
	*samp16++ = LE_SHORT(res.i >> 16);
        break;
      case SND_PCM_FORMAT_S16_BE:
	*samp16++ = BE_SHORT(res.i >> 16);
        break;
      case SND_PCM_FORMAT_FLOAT_LE:
	*samp_f++ = res.f;
        break;
      case SND_PCM_FORMAT_S24_3LE:
        res.i >>= 8;
        *samp8++ = LE_INT(res.i);
        *samp8++ = LE_INT(res.i) >> 8;
        *samp8++ = LE_INT(res.i) >> 16;
        break;
      case SND_PCM_FORMAT_S24_3BE:
        res.i >>= 8;
        *samp8++ = BE_INT(res.i);
        *samp8++ = BE_INT(res.i) >> 8;
        *samp8++ = BE_INT(res.i) >> 16;
        break;
      case SND_PCM_FORMAT_S24_LE:
        res.i >>= 8;
        *samp8++ = LE_INT(res.i);
        *samp8++ = LE_INT(res.i) >> 8;
        *samp8++ = LE_INT(res.i) >> 16;
        *samp8++ = 0;
        break;
      case SND_PCM_FORMAT_S24_BE:
        res.i >>= 8;
        *samp8++ = 0;
        *samp8++ = BE_INT(res.i);
        *samp8++ = BE_INT(res.i) >> 8;
        *samp8++ = BE_INT(res.i) >> 16;
        break;
      case SND_PCM_FORMAT_S32_LE:
	*samp32++ = LE_INT(res.i);
        break;
      case SND_PCM_FORMAT_S32_BE:
	*samp32++ = BE_INT(res.i);
        break;
      default:
        ;
      }
    }
  }
}

/*
 * Sine generator
 */
typedef struct {
  double phase;
  double max_phase;
  double step;
} sine_t;

static void init_sine(sine_t *sine)
{
  sine->phase = 0;
  sine->max_phase = 1.0 / freq;
  sine->step = 1.0 / (double)rate;
}

static value_t generate_sine(void *arg)
{
  sine_t *sine = arg;
  value_t res;

  res.f = sin((sine->phase * 2 * M_PI) / sine->max_phase - M_PI);
  res.f *= generator_scale;
  if (format != SND_PCM_FORMAT_FLOAT_LE)
    res.i = res.f * INT32_MAX;
  sine->phase += sine->step;
  if (sine->phase >= sine->max_phase)
    sine->phase -= sine->max_phase;
  return res;
}

/* Pink noise is a better test than sine wave because we can tell
 * where pink noise is coming from more easily that a sine wave.
 */
static value_t generate_pink_noise(void *arg)
{
  pink_noise_t *pink = arg;
  value_t res;

  res.f = generate_pink_noise_sample(pink) * generator_scale;
  if (format != SND_PCM_FORMAT_FLOAT_LE)
    res.i = res.f * INT32_MAX;
  return res;
}

/*
 * useful for tests
 */
static value_t generate_pattern(void *arg)
{
  value_t res;

  res.i = *(int *)arg;
  *(int *)arg = res.i + 1;
  if (format != SND_PCM_FORMAT_FLOAT_LE)
    res.f = (float)res.i / (float)INT32_MAX;
  return res;
}

static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access) {
  unsigned int rrate;
  int          err;
  snd_pcm_uframes_t     period_size_min;
  snd_pcm_uframes_t     period_size_max;
  snd_pcm_uframes_t     buffer_size_min;
  snd_pcm_uframes_t     buffer_size_max;

  /* choose all parameters */
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    fprintf(stderr, _("Broken configuration for playback: no configurations available: %s\n"), snd_strerror(err));
    return err;
  }

  /* set the interleaved read/write format */
  err = snd_pcm_hw_params_set_access(handle, params, access);
  if (err < 0) {
    fprintf(stderr, _("Access type not available for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* set the sample format */
  err = snd_pcm_hw_params_set_format(handle, params, format);
  if (err < 0) {
    fprintf(stderr, _("Sample format not available for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* set the count of channels */
  err = snd_pcm_hw_params_set_channels(handle, params, channels);
  if (err < 0) {
    fprintf(stderr, _("Channels count (%i) not available for playbacks: %s\n"), channels, snd_strerror(err));
    return err;
  }

  /* set the stream rate */
  rrate = rate;
  err = snd_pcm_hw_params_set_rate(handle, params, rate, 0);
  if (err < 0) {
    fprintf(stderr, _("Rate %iHz not available for playback: %s\n"), rate, snd_strerror(err));
    return err;
  }

  if (rrate != rate) {
    fprintf(stderr, _("Rate doesn't match (requested %iHz, get %iHz, err %d)\n"), rate, rrate, err);
    return -EINVAL;
  }

  printf(_("Rate set to %iHz (requested %iHz)\n"), rrate, rate);
  /* set the buffer time */
  err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
  err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
  err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min, NULL);
  err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max, NULL);
  printf(_("Buffer size range from %lu to %lu\n"),buffer_size_min, buffer_size_max);
  printf(_("Period size range from %lu to %lu\n"),period_size_min, period_size_max);
  if (period_time > 0) {
    printf(_("Requested period time %u us\n"), period_time);
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, NULL);
    if (err < 0) {
      fprintf(stderr, _("Unable to set period time %u us for playback: %s\n"),
	     period_time, snd_strerror(err));
      return err;
    }
  }
  if (buffer_time > 0) {
    printf(_("Requested buffer time %u us\n"), buffer_time);
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, NULL);
    if (err < 0) {
      fprintf(stderr, _("Unable to set buffer time %u us for playback: %s\n"),
	     buffer_time, snd_strerror(err));
      return err;
    }
  }
  if (! buffer_time && ! period_time) {
    buffer_size = buffer_size_max;
    if (! period_time)
      buffer_size = (buffer_size / nperiods) * nperiods;
    printf(_("Using max buffer size %lu\n"), buffer_size);
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
    if (err < 0) {
      fprintf(stderr, _("Unable to set buffer size %lu for playback: %s\n"),
	     buffer_size, snd_strerror(err));
      return err;
    }
  }
  if (! buffer_time || ! period_time) {
    printf(_("Periods = %u\n"), nperiods);
    err = snd_pcm_hw_params_set_periods_near(handle, params, &nperiods, NULL);
    if (err < 0) {
      fprintf(stderr, _("Unable to set nperiods %u for playback: %s\n"),
	     nperiods, snd_strerror(err));
      return err;
    }
  }

  /* write the parameters to device */
  err = snd_pcm_hw_params(handle, params);
  if (err < 0) {
    fprintf(stderr, _("Unable to set hw params for playback: %s\n"), snd_strerror(err));
    return err;
  }

  snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
  snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
  printf(_("was set period_size = %lu\n"),period_size);
  printf(_("was set buffer_size = %lu\n"),buffer_size);
  if (2*period_size > buffer_size) {
    fprintf(stderr, _("buffer to small, could not use\n"));
    return -EINVAL;
  }

  return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams) {
  int err;

  /* get the current swparams */
  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0) {
    fprintf(stderr, _("Unable to determine current swparams for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* start the transfer when a buffer is full */
  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size);
  if (err < 0) {
    fprintf(stderr, _("Unable to set start threshold mode for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* allow the transfer when at least period_size frames can be processed */
  err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
  if (err < 0) {
    fprintf(stderr, _("Unable to set avail min for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* write the parameters to the playback device */
  err = snd_pcm_sw_params(handle, swparams);
  if (err < 0) {
    fprintf(stderr, _("Unable to set sw params for playback: %s\n"), snd_strerror(err));
    return err;
  }

  return 0;
}

#ifdef CONFIG_SUPPORT_CHMAP
static int config_chmap(snd_pcm_t *handle, const char *mapstr)
{
  int err;

  if (mapstr) {
    channel_map = snd_pcm_chmap_parse_string(mapstr);
    if (!channel_map) {
      fprintf(stderr, _("Unable to parse channel map string: %s\n"), mapstr);
      return -EINVAL;
    }
    err = snd_pcm_set_chmap(handle, channel_map);
    if (err < 0) {
      fprintf(stderr, _("Unable to set channel map: %s\n"), mapstr);
      return err;
    }
    channel_map_set = 1;
    return 0;
  }

  channel_map = snd_pcm_get_chmap(handle);

  /* create a channel order table for default layouts */
  if (channel_map)
    ordered_channels = order_channels();

  return 0;
}
#endif

/*
 *   Underrun and suspend recovery
 */

static int xrun_recovery(snd_pcm_t *handle, int err) {
  if (err == -EPIPE) {	/* under-run */
    err = snd_pcm_prepare(handle);
    if (err < 0)
      fprintf(stderr, _("Can't recovery from underrun, prepare failed: %s\n"), snd_strerror(err));
    return 0;
  } 
  else if (err == -ESTRPIPE) {

    while ((err = snd_pcm_resume(handle)) == -EAGAIN)
      sleep(1);	/* wait until the suspend flag is released */

    if (err < 0) {
      err = snd_pcm_prepare(handle);
      if (err < 0)
        fprintf(stderr, _("Can't recovery from suspend, prepare failed: %s\n"), snd_strerror(err));
    }

    return 0;
  }

  return err;
}

/*
 * Handle WAV files
 */

static const char *wav_file[MAX_CHANNELS];
static int wav_file_size[MAX_CHANNELS];

struct wave_header {
  struct {
    uint32_t magic;
    uint32_t length;
    uint32_t type;
  } hdr;
  struct {
    uint32_t type;
    uint32_t length;
  } chunk1;
  struct {
    uint16_t format;
    uint16_t channels;
    uint32_t rate;
    uint32_t bytes_per_sec;
    uint16_t sample_size;
    uint16_t sample_bits;
  } body;
  struct {
    uint32_t type;
    uint32_t length;
  } chunk;
};

#define WAV_RIFF		COMPOSE_ID('R','I','F','F')
#define WAV_WAVE		COMPOSE_ID('W','A','V','E')
#define WAV_FMT			COMPOSE_ID('f','m','t',' ')
#define WAV_DATA		COMPOSE_ID('d','a','t','a')
#define WAV_PCM_CODE		1

static const char *search_for_file(const char *name)
{
  char *file;
  if (*name == '/')
    return strdup(name);
  file = malloc(strlen(wav_file_dir) + strlen(name) + 2);
  if (file)
    sprintf(file, "%s/%s", wav_file_dir, name);
  return file;
}

static int check_wav_file(int channel, const char *name)
{
  struct wave_header header;
  int fd;

  wav_file[channel] = search_for_file(name);
  if (! wav_file[channel]) {
    fprintf(stderr, _("No enough memory\n"));
    return -ENOMEM;
  }

  if ((fd = open(wav_file[channel], O_RDONLY)) < 0) {
    fprintf(stderr, _("Cannot open WAV file %s\n"), wav_file[channel]);
    return -EINVAL;
  }
  if (read(fd, &header, sizeof(header)) < (int)sizeof(header)) {
    fprintf(stderr, _("Invalid WAV file %s\n"), wav_file[channel]);
    goto error;
  }
  
  if (header.hdr.magic != WAV_RIFF || header.hdr.type != WAV_WAVE) {
    fprintf(stderr, _("Not a WAV file: %s\n"), wav_file[channel]);
    goto error;
  }
  if (header.body.format != LE_SHORT(WAV_PCM_CODE)) {
    fprintf(stderr, _("Unsupported WAV format %d for %s\n"),
	    LE_SHORT(header.body.format), wav_file[channel]);
    goto error;
  }
  if (header.body.channels != LE_SHORT(1)) {
    fprintf(stderr, _("%s is not a mono stream (%d channels)\n"),
	    wav_file[channel], LE_SHORT(header.body.channels)); 
    goto error;
  }
  if (header.body.rate != LE_INT(rate)) {
    fprintf(stderr, _("Sample rate doesn't match (%d) for %s\n"),
	    LE_INT(header.body.rate), wav_file[channel]);
    goto error;
  }
  if (header.body.sample_bits != LE_SHORT(16)) {
    fprintf(stderr, _("Unsupported sample format bits %d for %s\n"),
	    LE_SHORT(header.body.sample_bits), wav_file[channel]);
    goto error;
  }
  if (header.chunk.type != WAV_DATA) {
    fprintf(stderr, _("Invalid WAV file %s\n"), wav_file[channel]);
    goto error;
  }
  wav_file_size[channel] = LE_INT(header.chunk.length);
  close(fd);
  return 0;

 error:
  close(fd);
  return -EINVAL;
}

static int setup_wav_file(int chn)
{
  static const char *const wavs[MAX_CHANNELS] = {
    "Front_Left.wav",
    "Front_Right.wav",
    "Rear_Left.wav",
    "Rear_Right.wav",
    "Front_Center.wav",
    "Rear_Center.wav", /* FIXME: should be "Bass" or so */
    "Side_Left.wav",
    "Side_Right.wav",
    "Channel_9.wav",
    "Channel_10.wav",
    "Channel_11.wav",
    "Channel_12.wav",
    "Channel_13.wav",
    "Channel_14.wav",
    "Channel_15.wav",
    "Channel_16.wav"
  };

  if (given_test_wav_file)
    return check_wav_file(chn, given_test_wav_file);

#ifdef CONFIG_SUPPORT_CHMAP
  if (channel_map && chn < channel_map->channels) {
    int channel = channel_map->pos[chn] - SND_CHMAP_FL;
    if (channel >= 0 && channel < MAX_CHANNELS)
      return check_wav_file(chn, wavs[channel]);
  }
#endif

  return check_wav_file(chn, wavs[chn]);
}

static int read_wav(uint16_t *buf, int channel, int offset, int bufsize)
{
  static FILE *wavfp = NULL;
  int size;

  if (in_aborting)
    return -EFAULT;

  if (! wav_file[channel]) {
    fprintf(stderr, _("Undefined channel %d\n"), channel);
    return -EINVAL;
  }

  if (offset >= wav_file_size[channel])
   return 0; /* finished */

  if (! offset) {
    if (wavfp)
      fclose(wavfp);
    wavfp = fopen(wav_file[channel], "r");
    if (! wavfp)
      return -errno;
    if (fseek(wavfp, sizeof(struct wave_header), SEEK_SET) < 0)
      return -errno;
  }
  if (offset + bufsize > wav_file_size[channel])
    bufsize = wav_file_size[channel] - offset;
  bufsize /= channels;
  for (size = 0; size < bufsize; size += 2) {
    int chn;
    for (chn = 0; chn < channels; chn++) {
      if (chn == channel) {
	if (fread(buf, 2, 1, wavfp) != 1)
	  return size;
      }
      else
	*buf = 0;
      buf++;
    }
  }
  return size;
}


/*
 *   Transfer method - write only
 */

static int write_buffer(snd_pcm_t *handle, uint8_t *ptr, int cptr)
{
  int err;

  while (cptr > 0 && !in_aborting) {

    err = snd_pcm_writei(handle, ptr, cptr);

    if (err == -EAGAIN)
      continue;

    if (err < 0) {
      fprintf(stderr, _("Write error: %d,%s\n"), err, snd_strerror(err));
      if ((err = xrun_recovery(handle, err)) < 0) {
	fprintf(stderr, _("xrun_recovery failed: %d,%s\n"), err, snd_strerror(err));
	return err;
      }
      break;	/* skip one period */
    }

    ptr += snd_pcm_frames_to_bytes(handle, err);
    cptr -= err;
  }
  return 0;
}

static int pattern;
static sine_t sine;
static pink_noise_t pink;

static void init_loop(void)
{
  switch (test_type) {
  case TEST_PINK_NOISE:
    initialize_pink_noise(&pink, 16);
    break;
  case TEST_SINE:
    init_sine(&sine);
    break;
  case TEST_PATTERN:
    pattern = 0;
    break;
  }
}

static int write_loop(snd_pcm_t *handle, int channel, int periods, uint8_t *frames)
{
  int    err, n;

  fflush(stdout);
  if (test_type == TEST_WAV) {
    int bufsize = snd_pcm_frames_to_bytes(handle, period_size);
    n = 0;
    while ((err = read_wav((uint16_t *)frames, channel, n, bufsize)) > 0 && !in_aborting) {
      n += err;
      if ((err = write_buffer(handle, frames,
			      snd_pcm_bytes_to_frames(handle, err * channels))) < 0)
	break;
    }
    if (buffer_size > n && !in_aborting) {
      snd_pcm_drain(handle);
      snd_pcm_prepare(handle);
    }
    return err;
  }
    

  if (periods <= 0)
    periods = 1;

  for(n = 0; n < periods && !in_aborting; n++) {
    if (test_type == TEST_PINK_NOISE)
      do_generate(frames, channel, period_size, generate_pink_noise, &pink);
    else if (test_type == TEST_PATTERN)
      do_generate(frames, channel, period_size, generate_pattern, &pattern);
    else
      do_generate(frames, channel, period_size, generate_sine, &sine);

    if ((err = write_buffer(handle, frames, period_size)) < 0)
      return err;
  }
  if (buffer_size > n * period_size && !in_aborting) {
    snd_pcm_drain(handle);
    snd_pcm_prepare(handle);
  }
  return 0;
}

static int prg_exit(int code)
{
  if (pcm_handle)
    snd_pcm_close(pcm_handle);
  exit(code);
  return code;
}

static void signal_handler(int sig)
{
  if (in_aborting)
    return;

  in_aborting = 1;

  if (pcm_handle)
    snd_pcm_abort(pcm_handle);
  if (sig == SIGABRT) {
    pcm_handle = NULL;
    prg_exit(EXIT_FAILURE);
  }
  signal(sig, signal_handler);
}

static void help(void)
{
  const int *fmt;

  printf(
	 _("Usage: speaker-test [OPTION]... \n"
	   "-h,--help	help\n"
	   "-D,--device	playback device\n"
	   "-r,--rate	stream rate in Hz\n"
	   "-c,--channels	count of channels in stream\n"
	   "-f,--frequency	sine wave frequency in Hz\n"
	   "-F,--format	sample format\n"
	   "-b,--buffer	ring buffer size in us\n"
	   "-p,--period	period size in us\n"
	   "-P,--nperiods	number of periods\n"
	   "-t,--test	pink=use pink noise, sine=use sine wave, wav=WAV file\n"
	   "-l,--nloops	specify number of loops to test, 0 = infinite\n"
	   "-s,--speaker	single speaker test. Values 1=Left, 2=right, etc\n"
	   "-w,--wavfile	Use the given WAV file as a test sound\n"
	   "-W,--wavdir	Specify the directory containing WAV files\n"
	   "-m,--chmap	Specify the channel map to override\n"
	   "-X,--force-frequency	force frequencies outside the 30-8000hz range\n"
	   "-S,--scale	Scale of generated test tones in percent (default=80)\n"
	   "\n"));
  printf(_("Recognized sample formats are:"));
  for (fmt = supported_formats; *fmt >= 0; fmt++) {
    const char *s = snd_pcm_format_name(*fmt);
    if (s)
      printf(" %s", s);
  }

  printf("\n\n");
}

int main(int argc, char *argv[]) {
  snd_pcm_t            *handle;
  int                   err, morehelp;
  snd_pcm_hw_params_t  *hwparams;
  snd_pcm_sw_params_t  *swparams;
  uint8_t              *frames;
  int                   chn;
  const int	       *fmt;
  double		time1,time2,time3;
  unsigned int		n, nloops;
  struct   timeval	tv1,tv2;
  int			speakeroptset = 0;
#ifdef CONFIG_SUPPORT_CHMAP
  const char *chmap = NULL;
#endif

  static const struct option long_option[] = {
    {"help",      0, NULL, 'h'},
    {"device",    1, NULL, 'D'},
    {"rate",      1, NULL, 'r'},
    {"channels",  1, NULL, 'c'},
    {"frequency", 1, NULL, 'f'},
    {"format",    1, NULL, 'F'},
    {"buffer",    1, NULL, 'b'},
    {"period",    1, NULL, 'p'},
    {"nperiods",  1, NULL, 'P'},
    {"test",      1, NULL, 't'},
    {"nloops",    1, NULL, 'l'},
    {"speaker",   1, NULL, 's'},
    {"wavfile",   1, NULL, 'w'},
    {"wavdir",    1, NULL, 'W'},
    {"debug",	  0, NULL, 'd'},
    {"force-frequency",	  0, NULL, 'X'},
    {"scale",	  1, NULL, 'S'},
#ifdef CONFIG_SUPPORT_CHMAP
    {"chmap",	  1, NULL, 'm'},
#endif
    {NULL,        0, NULL, 0  },
  };

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  textdomain(PACKAGE);
#endif

  snd_pcm_hw_params_alloca(&hwparams);
  snd_pcm_sw_params_alloca(&swparams);
 
  nloops = 0;
  morehelp = 0;

  printf("\nspeaker-test %s\n\n", SND_UTIL_VERSION_STR);
  while (1) {
    int c;
    
    if ((c = getopt_long(argc, argv, "hD:r:c:f:F:b:p:P:t:l:s:w:W:d:XS:"
#ifdef CONFIG_SUPPORT_CHMAP
			 "m:"
#endif
			 , long_option, NULL)) < 0)
      break;
    
    switch (c) {
    case 'h':
      morehelp++;
      break;
    case 'D':
      device = strdup(optarg);
      break;
    case 'F':
      format = snd_pcm_format_value(optarg);
      for (fmt = supported_formats; *fmt >= 0; fmt++)
        if (*fmt == format)
          break;
      if (*fmt < 0) {
        fprintf(stderr, "Format %s is not supported...\n", snd_pcm_format_name(format));
        exit(EXIT_FAILURE);
      }
      break;
    case 'r':
      rate = atoi(optarg);
      rate = rate < 4000 ? 4000 : rate;
      rate = rate > 768000 ? 768000 : rate;
      break;
    case 'c':
      channels = atoi(optarg);
      channels = channels < 1 ? 1 : channels;
      channels = channels > 1024 ? 1024 : channels;
      break;
    case 'f':
      freq = atof(optarg);
      break;
    case 'b':
      buffer_time = atoi(optarg);
      buffer_time = buffer_time > 1000000 ? 1000000 : buffer_time;
      break;
    case 'p':
      period_time = atoi(optarg);
      period_time = period_time > 1000000 ? 1000000 : period_time;
      break;
    case 'P':
      nperiods = atoi(optarg);
      if (nperiods < 2 || nperiods > 1024) {
	fprintf(stderr, _("Invalid number of periods %d\n"), nperiods);
	exit(1);
      }
      break;
    case 't':
      if (*optarg == 'p')
	test_type = TEST_PINK_NOISE;
      else if (*optarg == 's')
	test_type = TEST_SINE;
      else if (*optarg == 'w')
	test_type = TEST_WAV;
      else if (*optarg == 't')
	test_type = TEST_PATTERN;
      else if (isdigit(*optarg)) {
	test_type = atoi(optarg);
	if (test_type < TEST_PINK_NOISE || test_type > TEST_PATTERN) {
	  fprintf(stderr, _("Invalid test type %s\n"), optarg);
	  exit(1);
	}
      } else {
	fprintf(stderr, _("Invalid test type %s\n"), optarg);
	exit(1);
      }
      break;
    case 'l':
      nloops = atoi(optarg);
      break;
    case 's':
      speaker = atoi(optarg);
      speaker = speaker < 1 ? 0 : speaker;
      speakeroptset = 1;
      break;
    case 'w':
      given_test_wav_file = optarg;
      break;
    case 'W':
      wav_file_dir = optarg;
      break;
    case 'd':
      debug = 1;
      break;
    case 'X':
      force_frequency = 1;
      break;
#ifdef CONFIG_SUPPORT_CHMAP
    case 'm':
      chmap = optarg;
      break;
#endif
    case 'S':
      generator_scale = atoi(optarg) / 100.0;
      break;
    default:
      fprintf(stderr, _("Unknown option '%c'\n"), c);
      exit(EXIT_FAILURE);
      break;
    }
  }

  if (morehelp) {
    help();
    exit(EXIT_SUCCESS);
  }

  if (speakeroptset) {
    speaker = speaker > channels ? 0 : speaker;
    if (speaker==0) {
      fprintf(stderr, _("Invalid parameter for -s option.\n"));
      exit(EXIT_FAILURE);
    }
  }

  if (!force_frequency) {
    freq = freq < 30.0 ? 30.0 : freq;
    freq = freq > 8000.0 ? 8000.0 : freq;
  } else {
    freq = freq < 1.0 ? 1.0 : freq;
  }

  if (test_type == TEST_WAV)
    format = SND_PCM_FORMAT_S16_LE; /* fixed format */

  printf(_("Playback device is %s\n"), device);
  printf(_("Stream parameters are %iHz, %s, %i channels\n"), rate, snd_pcm_format_name(format), channels);
  switch (test_type) {
  case TEST_PINK_NOISE:
    printf(_("Using 16 octaves of pink noise\n"));
    break;
  case TEST_SINE:
    printf(_("Sine wave rate is %.4fHz\n"), freq);
    break;
  case TEST_WAV:
    printf(_("WAV file(s)\n"));
    break;

  }

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGABRT, signal_handler);

  if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    printf(_("Playback open error: %d,%s\n"), err,snd_strerror(err));
    prg_exit(EXIT_FAILURE);
  }
  pcm_handle = handle;

  if ((err = set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    printf(_("Setting of hwparams failed: %s\n"), snd_strerror(err));
    prg_exit(EXIT_FAILURE);
  }
  if ((err = set_swparams(handle, swparams)) < 0) {
    printf(_("Setting of swparams failed: %s\n"), snd_strerror(err));
    prg_exit(EXIT_FAILURE);
  }

#ifdef CONFIG_SUPPORT_CHMAP
  err = config_chmap(handle, chmap);
  if (err < 0)
    prg_exit(EXIT_FAILURE);
#endif

  if (debug) {
    snd_output_t *log;
    err = snd_output_stdio_attach(&log, stderr, 0);
    if (err >= 0) {
      snd_pcm_dump(handle, log);
      snd_output_close(log);
    }
  }

  frames = malloc(snd_pcm_frames_to_bytes(handle, period_size));
  if (frames == NULL) {
    fprintf(stderr, _("No enough memory\n"));
    prg_exit(EXIT_FAILURE);
  }

  init_loop();

  if (speaker==0) {

    if (test_type == TEST_WAV) {
      for (chn = 0; chn < channels; chn++) {
	if (setup_wav_file(get_speaker_channel(chn)) < 0)
	  prg_exit(EXIT_FAILURE);
      }
    }

    for (n = 0; (! nloops || n < nloops) && !in_aborting; n++) {

      gettimeofday(&tv1, NULL);
      for(chn = 0; chn < channels; chn++) {
	int channel = get_speaker_channel(chn);
        printf(" %d - %s\n", channel, get_channel_name(channel));

        err = write_loop(handle, channel, ((rate*3)/period_size), frames);

        if (err < 0) {
          fprintf(stderr, _("Transfer failed: %s\n"), snd_strerror(err));
          prg_exit(EXIT_SUCCESS);
        }
      }
      gettimeofday(&tv2, NULL);
      time1 = (double)tv1.tv_sec + ((double)tv1.tv_usec / 1000000.0);
      time2 = (double)tv2.tv_sec + ((double)tv2.tv_usec / 1000000.0);
      time3 = time2 - time1;
      printf(_("Time per period = %lf\n"), time3 );
    }
  } else {
    chn = get_speaker_channel(speaker - 1);

    if (test_type == TEST_WAV) {
      if (setup_wav_file(chn) < 0)
	prg_exit(EXIT_FAILURE);
    }

    printf("  - %s\n", get_channel_name(chn));
    err = write_loop(handle, chn, ((rate*5)/period_size), frames);

    if (err < 0) {
      fprintf(stderr, _("Transfer failed: %s\n"), snd_strerror(err));
    }
  }

  snd_pcm_drain(handle);

  free(frames);
#ifdef CONFIG_SUPPORT_CHMAP
  free(ordered_channels);
#endif

  return prg_exit(EXIT_SUCCESS);
}
