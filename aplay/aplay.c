/*
 *  aplay.c - plays and records
 *
 *      CREATIVE LABS VOICE-files
 *      Microsoft WAVE-files
 *      SPARC AUDIO .AU-files
 *      Raw Data
 *
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 *  Based on vplay program by Michael Beck
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/asoundlib.h>
#include "aconfig.h"
#include "formats.h"
#include "version.h"

#define DEFAULT_SPEED 		8000

#define FORMAT_DEFAULT		-1
#define FORMAT_RAW		0
#define FORMAT_VOC		1
#define FORMAT_WAVE		2
#define FORMAT_AU		3

/* global data */

char *command;
snd_pcm_t *pcm_handle;
struct snd_pcm_channel_info cinfo;
snd_pcm_format_t rformat, format;
int timelimit = 0;
int quiet_mode = 0;
int verbose_mode = 0;
int format_change = 0;
int active_format = FORMAT_DEFAULT;
int mode = SND_PCM_MODE_BLOCK;
int direction = SND_PCM_OPEN_PLAYBACK;
int channel = SND_PCM_CHANNEL_PLAYBACK;
int mmap_flag = 0;
int frag = 0;
int frags = 0;
char *audiobuf = NULL;
snd_pcm_mmap_control_t *mmap_control = NULL;
char *mmap_data = NULL;
long mmap_size = 0;
int buffer_size = -1;
char silence = 0;

int count;
int vocmajor, vocminor;

/* functions */

int (*fcn_info)(snd_pcm_t *handle, snd_pcm_channel_info_t *info);
int (*fcn_params)(snd_pcm_t *handle, snd_pcm_channel_params_t *params);
int (*fcn_setup)(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup);
int (*fcn_status)(snd_pcm_t *handle, snd_pcm_channel_status_t *status);
int (*fcn_flush)(snd_pcm_t *handle, int channel);
ssize_t (*fcn_write)(snd_pcm_t *handle, const void *buffer, size_t size);
ssize_t (*fcn_read)(snd_pcm_t *handle, void *buffer, size_t size);

/* needed prototypes */

static void playback(char *filename);
static void capture(char *filename);

static void begin_voc(int fd, u_long count);
static void end_voc(int fd);
static void begin_wave(int fd, u_long count);
static void end_wave(int fd);
static void begin_au(int fd, u_long count);

struct fmt_capture {
	void (*start) (int fd, u_long count);
	void (*end) (int fd);
	char *what;
} fmt_rec_table[] = {
	{	NULL,		end_wave,	"raw data"	},
	{	begin_voc,	end_voc,	"VOC"		},
	{	begin_wave,	end_wave,	"WAVE"		},
	{	begin_au,	end_wave,	"Sparc Audio"	}
};

struct {
	char *name;
	int format;
} formats[] = {
	{ "s8", SND_PCM_SFMT_S8 },
	{ "u8", SND_PCM_SFMT_U8 },
	{ "s16l", SND_PCM_SFMT_S16_LE },
	{ "s16b", SND_PCM_SFMT_S16_BE },
	{ "u16l", SND_PCM_SFMT_U16_LE },
	{ "u16b", SND_PCM_SFMT_U16_BE },
	{ "s24l", SND_PCM_SFMT_S24_LE },
	{ "s24b", SND_PCM_SFMT_S24_BE },
	{ "u24l", SND_PCM_SFMT_U24_LE },
	{ "u24b", SND_PCM_SFMT_U24_BE },
	{ "s32l", SND_PCM_SFMT_S32_LE },
	{ "s32b", SND_PCM_SFMT_S32_BE },
	{ "u32l", SND_PCM_SFMT_U32_LE },
	{ "u32b", SND_PCM_SFMT_U32_BE },
	{ "f32l", SND_PCM_SFMT_FLOAT_LE },
	{ "f32b", SND_PCM_SFMT_FLOAT_BE },
	{ "f64l", SND_PCM_SFMT_FLOAT64_LE },
	{ "f64b", SND_PCM_SFMT_FLOAT64_BE },
	{ "iec958l", SND_PCM_SFMT_IEC958_SUBFRAME_LE },
	{ "iec958b", SND_PCM_SFMT_IEC958_SUBFRAME_BE },
	{ "mulaw", SND_PCM_SFMT_MU_LAW },
	{ "alaw", SND_PCM_SFMT_A_LAW },
	{ "adpcm", SND_PCM_SFMT_IMA_ADPCM },
	{ "mpeg", SND_PCM_SFMT_MPEG },
	{ "gsm", SND_PCM_SFMT_GSM },
	{ "special", SND_PCM_SFMT_SPECIAL }
};

#define NUMFORMATS (sizeof(formats)/sizeof(formats[0]))

static void check_new_format(snd_pcm_format_t * format)
{
	if (cinfo.min_rate > format->rate || cinfo.max_rate < format->rate) {
		fprintf(stderr, "%s: unsupported rate %iHz (valid range is %iHz-%iHz)\n", command, format->rate, cinfo.min_rate, cinfo.max_rate);
		exit(1);
	}
	if (!(cinfo.formats & (1 << format->format))) {
		fprintf(stderr, "%s: requested format %s isn't supported with hardware\n", command, snd_pcm_get_format_name(format->format));
		exit(1);
	}
}

static void usage(char *command)
{
	int i;
	fprintf(stderr,
		"Usage: %s [switches] [filename] <filename> ...\n"
		"Available switches:\n"
		"\n"
		"  -h,--help     help\n"
		"  -V,--version  print current version\n"
		"  -l            list all soundcards and digital audio devices\n"
		"  -c <card>     select card # or card id (0-%i), defaults to 0\n"
		"  -d <device>   select device #, defaults to 0\n"
		"  -q            quiet mode\n"
		"  -v            file format Voc\n"
		"  -w            file format Wave\n"
		"  -r            file format Raw\n"
		"  -S            stereo\n"
		"  -s <Hz>       speed (Hz)\n"
		"  -f <format>   data format\n"
		"  -m            set CD-ROM quality (44100Hz,stereo,16-bit linear, little endian)\n"
		"  -M            set DAT quality (48000Hz,stereo,16-bit linear, little endian)\n"
		"  -t <secs>     timelimit (seconds)\n"
		"  -e		 stream mode\n"
		"  -E            mmap mode\n"
		,command, snd_cards()-1);
	fprintf(stderr, "\nRecognized data formats are:");
	for (i = 0; i < NUMFORMATS; ++i)
		fprintf(stderr, " %s", formats[i].name);
	fprintf(stderr, "\nSome of this may not be available on selected hardware\n");
}

static void device_list(void)
{
	snd_ctl_t *handle;
	int card, err, dev, idx;
	unsigned int mask;
	struct snd_ctl_hw_info info;
	snd_pcm_info_t pcminfo;
	snd_pcm_channel_info_t chninfo;

	mask = snd_cards_mask();
	if (!mask) {
		printf("%s: no soundcards found...\n", command);
		return;
	}
	for (card = 0; card < SND_CARDS; card++) {
		if (!(mask & (1 << card)))
			continue;
		if ((err = snd_ctl_open(&handle, card)) < 0) {
			printf("Error: control open (%i): %s\n", card, snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
			printf("Error: control hardware info (%i): %s\n", card, snd_strerror(err));
			snd_ctl_close(handle);
			continue;
		}
		for (dev = 0; dev < info.pcmdevs; dev++) {
			if ((err = snd_ctl_pcm_info(handle, dev, &pcminfo)) < 0) {
				printf("Error: control digital audio info (%i): %s\n", card, snd_strerror(err));
				continue;
			}
			printf("%s: %i [%s] / #%i: %s\n",
			       info.name,
			       card + 1,
			       info.id,
			       dev,
			       pcminfo.name);
			printf("  Directions: %s%s%s\n",
			       pcminfo.flags & SND_PCM_INFO_PLAYBACK ? "playback " : "",
			       pcminfo.flags & SND_PCM_INFO_CAPTURE ? "capture " : "",
			       pcminfo.flags & SND_PCM_INFO_DUPLEX ? "duplex " : "");
			printf("  Playback subdevices: %i\n", pcminfo.playback + 1);
			printf("  Capture subdevices: %i\n", pcminfo.capture + 1);
			if (pcminfo.flags & SND_PCM_INFO_PLAYBACK) {
				for (idx = 0; idx <= pcminfo.playback; idx++) {
					memset(&chninfo, 0, sizeof(chninfo));
					chninfo.channel = SND_PCM_CHANNEL_PLAYBACK;
					if ((err = snd_ctl_pcm_channel_info(handle, dev, idx, &chninfo)) < 0) {
						printf("Error: control digital audio playback info (%i): %s\n", card, snd_strerror(err));
					} else {
						printf("  Playback subdevice #%i: %s\n", idx, chninfo.subname);
					}
				}
			}
			if (pcminfo.flags & SND_PCM_INFO_CAPTURE) {
				for (idx = 0; idx <= pcminfo.capture; idx++) {
					memset(&chninfo, 0, sizeof(chninfo));
					chninfo.channel = SND_PCM_CHANNEL_CAPTURE;
					if ((err = snd_ctl_pcm_channel_info(handle, dev, 0, &chninfo)) < 0) {
						printf("Error: control digital audio capture info (%i): %s\n", card, snd_strerror(err));
					} else {
						printf("  Capture subdevice #%i: %s\n", idx, chninfo.subname);
					}
				}
			}
		}
		snd_ctl_close(handle);
	}
}

static void version(void)
{
	fprintf(stderr, "%s: version " SND_UTIL_VERSION_STR " by Jaroslav Kysela <perex@suse.cz>\n", command);
}

int main(int argc, char *argv[])
{
	int card, dev, tmp, err, c;

	card = 0;
	dev = 0;
	command = argv[0];
	active_format = FORMAT_DEFAULT;
	if (strstr(argv[0], "arecord")) {
		direction = SND_PCM_OPEN_CAPTURE;
		channel = SND_PCM_CHANNEL_CAPTURE;
		active_format = FORMAT_WAVE;
		command = "Arecord";
	} else if (strstr(argv[0], "aplay")) {
		direction = SND_PCM_OPEN_PLAYBACK;
		channel = SND_PCM_CHANNEL_PLAYBACK;
		command = "Aplay";
	} else {
		fprintf(stderr, "Error: command should be named either arecord or aplay\n");
		return 1;
	}

	buffer_size = -1;
	memset(&rformat, 0, sizeof(rformat));
	rformat.interleave = 1;
	rformat.format = SND_PCM_SFMT_U8;
	rformat.rate = DEFAULT_SPEED;
	rformat.voices = 1;

	if (argc > 1 && !strcmp(argv[1], "--help")) {
		usage(command);
		return 0;
	}
	if (argc > 1 && !strcmp(argv[1], "--version")) {
		version();
		return 0;
	}
	while ((c = getopt(argc, argv, "hlc:d:qs:So:t:vrwxB:c:p:mMVeEf:")) != EOF)
		switch (c) {
		case 'h':
			usage(command);
			return 0;
		case 'l':
			device_list();
			return 0;
		case 'c':
			card = snd_card_name(optarg);
			if (card < 0) {
				fprintf(stderr, "Error: soundcard '%s' not found\n", optarg);
				return 1;
			}
			break;
		case 'd':
			dev = atoi(optarg);
			if (dev < 0 || dev > 32) {
				fprintf(stderr, "Error: device %i is invalid\n", dev);
				return 1;
			}
			break;
		case 'S':
			rformat.voices = 2;
			break;
		case 'o':
			tmp = atoi(optarg);
			if (tmp < 1 || tmp > 32) {
				fprintf(stderr, "Error: value %i for voices is invalid\n", tmp);
				return 1;
			}
			break;
		case 'q':
			quiet_mode = 1;
			break;
		case 'r':
			active_format = FORMAT_RAW;
			break;
		case 'v':
			active_format = FORMAT_VOC;
			break;
		case 'w':
			active_format = FORMAT_WAVE;
			break;
		case 's':
			tmp = atoi(optarg);
			if (tmp < 300)
				tmp *= 1000;
			rformat.rate = tmp;
			if (tmp < 2000 || tmp > 128000) {
				fprintf(stderr, "Error: bad speed value %i\n", tmp);
				return 1;
			}
			break;
		case 't':
			timelimit = atoi(optarg);
			break;
		case 'x':
			verbose_mode = 1;
			quiet_mode = 0;
			break;
		case 'f':
			for (tmp = 0; tmp < NUMFORMATS; ++tmp) {
				if (!strcmp(optarg, formats[tmp].name)) {
					rformat.format = formats[tmp].format;
					active_format = FORMAT_RAW;
					break;
				}
			}
			if (tmp == NUMFORMATS) {
				fprintf(stderr, "Error: wrong extended format '%s'\n", optarg);
				return 1;
			}
			break;
		case 'm':
		case 'M':
			rformat.format = SND_PCM_SFMT_S16_LE;
			rformat.rate = c == 'M' ? 48000 : 44100;
			rformat.voices = 2;
			break;
		case 'V':
			version();
			return 0;
		case 'e':
			if (!mmap_flag) {
				mode = SND_PCM_MODE_STREAM;
				if (direction == SND_PCM_OPEN_CAPTURE)
					direction = SND_PCM_OPEN_STREAM_CAPTURE;
				if (direction == SND_PCM_OPEN_PLAYBACK)
					direction = SND_PCM_OPEN_STREAM_PLAYBACK;
			}
			break;
		case 'E':
			if (mode == SND_PCM_MODE_BLOCK)
				mmap_flag = 1;
			break;
		default:
			usage(command);
			return 1;
		}

	if (!quiet_mode)
		version();

	fcn_info = snd_pcm_plugin_info;
	fcn_params = snd_pcm_plugin_params;
	fcn_setup = snd_pcm_plugin_setup;
	fcn_status = snd_pcm_plugin_status;
	fcn_flush = snd_pcm_plugin_flush;
	fcn_write = snd_pcm_plugin_write;
	fcn_read = snd_pcm_plugin_read;
	if (mmap_flag) {
		fcn_info = snd_pcm_channel_info;
		fcn_params = snd_pcm_channel_params;
		fcn_setup = snd_pcm_channel_setup;
		fcn_status = snd_pcm_channel_status;
		fcn_flush = snd_pcm_flush_channel;
		fcn_write = snd_pcm_write;
		fcn_read = snd_pcm_read;
	}

	if (!quiet_mode) {
		char *cardname;
		
		if ((err = snd_card_get_longname(card, &cardname)) < 0) {
			fprintf(stderr, "Error: unable to obtain longname: %s\n", snd_strerror(err));
			return 1;
		}
		fprintf(stderr, "Using soundcard '%s'\n", cardname);
		free(cardname);
	}
	if ((err = snd_pcm_open(&pcm_handle, card, dev, direction)) < 0) {
		fprintf(stderr, "Error: audio open error: %s\n", snd_strerror(err));
		return 1;
	}
	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.channel = channel;
	if ((err = fcn_info(pcm_handle, &cinfo)) < 0) {
		fprintf(stderr, "Error: channel info error: %s\n", snd_strerror(err));
		return 1;
	}

	buffer_size = 1024;
	format = rformat;

	audiobuf = (char *)malloc(1024);
	if (audiobuf == NULL) {
		fprintf(stderr, "Error: not enough memory\n");
		return 1;
	}

	if (optind > argc - 1) {
		if (channel == SND_PCM_CHANNEL_PLAYBACK)
			playback(NULL);
		else
			capture(NULL);
	} else {
		while (optind <= argc - 1) {
			if (channel == SND_PCM_CHANNEL_PLAYBACK)
				playback(argv[optind++]);
			else
				capture(argv[optind++]);
		}
	}
	snd_pcm_close(pcm_handle);
	return 0;
}

/*
 * Test, if it is a .VOC file and return >=0 if ok (this is the length of rest)
 *                                       < 0 if not 
 */
static int test_vocfile(void *buffer)
{
	VocHeader *vp = buffer;

	if (strstr(vp->magic, VOC_MAGIC_STRING)) {
		vocminor = vp->version & 0xFF;
		vocmajor = vp->version / 256;
		if (vp->version != (0x1233 - vp->coded_ver))
			return -2;	/* coded version mismatch */
		return vp->headerlen - sizeof(VocHeader);	/* 0 mostly */
	}
	return -1;		/* magic string fail */
}

/*
 * test, if it's a .WAV file, 0 if ok (and set the speed, stereo etc.)
 *                            < 0 if not
 */
static int test_wavefile(void *buffer)
{
	WaveHeader *wp = buffer;

	if (wp->main_chunk == WAV_RIFF && wp->chunk_type == WAV_WAVE &&
	    wp->sub_chunk == WAV_FMT && wp->data_chunk == WAV_DATA) {
		if (wp->format != WAV_PCM_CODE) {
			fprintf(stderr, "%s: can't play not PCM-coded WAVE-files\n", command);
			exit(1);
		}
		if (wp->modus < 1 || wp->modus > 32) {
			fprintf(stderr, "%s: can't play WAVE-files with %d tracks\n",
				command, wp->modus);
			exit(1);
		}
		format.voices = wp->modus;
		switch (wp->bit_p_spl) {
		case 8:
			format.format = SND_PCM_SFMT_U8;
			break;
		case 16:
			format.format = SND_PCM_SFMT_S16_LE;
			break;
		default:
			fprintf(stderr, "%s: can't play WAVE-files with sample %d bits wide\n",
				command, wp->bit_p_spl);
		}
		format.rate = wp->sample_fq;
		count = wp->data_length;
		check_new_format(&format);
		return 0;
	}
	return -1;
}

/*

 */

static int test_au(int fd, void *buffer)
{
	AuHeader *ap = buffer;

	if (ntohl(ap->magic) != AU_MAGIC)
		return -1;
	if (ntohl(ap->hdr_size) > 128 || ntohl(ap->hdr_size) < 24)
		return -1;
	count = ntohl(ap->data_size);
	switch (ntohl(ap->encoding)) {
	case AU_FMT_ULAW:
		format.format = SND_PCM_SFMT_MU_LAW;
		break;
	case AU_FMT_LIN8:
		format.format = SND_PCM_SFMT_U8;
		break;
	case AU_FMT_LIN16:
		format.format = SND_PCM_SFMT_U16_LE;
		break;
	default:
		return -1;
	}
	format.rate = ntohl(ap->sample_rate);
	if (format.rate < 2000 || format.rate > 256000)
		return -1;
	format.voices = ntohl(ap->channels);
	if (format.voices < 1 || format.voices > 128)
		return -1;
	if (read(fd, buffer + sizeof(AuHeader), ntohl(ap->hdr_size) - sizeof(AuHeader)) < 0) {
		fprintf(stderr, "%s: read error\n", command);
		exit(1);
	}
	check_new_format(&format);
	return 0;
}

/*
 *  writing zeros from the zerobuf to simulate silence,
 *  perhaps it's enough to use a long var instead of zerobuf ?
 */
static void write_zeros(unsigned x)
{
	unsigned l;
	char *buf;

	buf = (char *) malloc(buffer_size);
	if (!buf) {
		fprintf(stderr, "%s: can allocate buffer for zeros\n", command);
		return;		/* not fatal error */
	}
	memset(buf, 128, buffer_size);
	while (x > 0) {
		l = x;
		if (l > buffer_size)
			l = buffer_size;
		if (fcn_write(pcm_handle, buf, l) != l) {
			fprintf(stderr, "%s: write error\n", command);
			exit(1);
		}
		x -= l;
	}
}

static void set_format(void)
{
	unsigned int bps;	/* bytes per second */
	unsigned int size;	/* fragment size */
	struct snd_pcm_channel_params params;
	struct snd_pcm_channel_setup setup;

	if (!format_change)
		return;
	bps = format.rate * format.voices;
	silence = 0x00;
	switch (format.format) {
	case SND_PCM_SFMT_U8:
		silence = 0x80;
		break;
	case SND_PCM_SFMT_U16_LE:
	case SND_PCM_SFMT_U16_BE:
		bps <<= 1;
		silence = 0x80;
		break;
	case SND_PCM_SFMT_S8:
		silence = 0x00;
		break;
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_S16_BE:
		bps <<= 1;
		silence = 0x00;
		break;
	case SND_PCM_SFMT_IMA_ADPCM:
		bps >>= 2;
		silence = 0x00;
		break;
	}
	bps >>= 2;		/* ok.. this buffer should be 0.25 sec */
	if (bps < 16)
		bps = 16;
	size = 1;
	while ((size << 1) < bps)
		size <<= 1;

	if (mmap_flag)
		snd_pcm_munmap(pcm_handle, channel);
	fcn_flush(pcm_handle, channel);		/* to be in right state */
	memset(&params, 0, sizeof(params));
	params.mode = mode;
	params.channel = channel;
	memcpy(&params.format, &format, sizeof(format));
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		params.start_mode = SND_PCM_START_FULL;
	} else {
		params.start_mode = SND_PCM_START_DATA;
	}
	params.stop_mode = SND_PCM_STOP_STOP;
	if (mode == SND_PCM_MODE_BLOCK) {
		params.buf.block.frag_size = size;
		// params.buf.block.frag_size = 128;
		params.buf.block.frags_max = -1;		/* little trick (playback only) */
		// params.buf.block.frags_max = 1;
		params.buf.block.frags_min = 1;
	} else {
		params.buf.stream.queue_size = 1024 * 1024;	/* maximum */
		// params.buf.stream.queue_size = 8192;
		params.buf.stream.fill = SND_PCM_FILL_SILENCE;
		params.buf.stream.max_fill = 1024;
	}
	if (fcn_params(pcm_handle, &params) < 0) {
		fprintf(stderr, "%s: unable to set channel params\n", command);
		exit(1);
	}
	if (mmap_flag) {
		if (snd_pcm_mmap(pcm_handle, channel, &mmap_control, (void **)&mmap_data)<0) {
			fprintf(stderr, "%s: unable to mmap memory\n", command);
			exit(1);
		}
	}
	if (snd_pcm_plugin_prepare(pcm_handle, channel) < 0) {
		fprintf(stderr, "%s: unable to prepare channel\n", command);
		exit(1);
	}
	memset(&setup, 0, sizeof(setup));
	setup.mode = mode;
	setup.channel = channel;
	if (fcn_setup(pcm_handle, &setup) < 0) {
		fprintf(stderr, "%s: unable to obtain setup\n", command);
		exit(1);
	}
	frags = setup.buf.block.frags;
	buffer_size = mode == SND_PCM_MODE_BLOCK ?
				setup.buf.block.frag_size :
				setup.buf.stream.queue_size;
	audiobuf = (char *)realloc(audiobuf, buffer_size);
	if (audiobuf == NULL) {
		fprintf(stderr, "%s: not enough memory\n", command);
		exit(1);
	}
	// printf("real buffer_size = %i, frags = %i, total = %i\n", buffer_size, setup.buf.block.frags, setup.buf.block.frags * buffer_size);
	format_change = 0;
}

/*
 *  ok, let's play a .voc file
 */

static void voc_play(int fd, int ofs, char *name)
{
	int l;
	VocBlockType *bp;
	VocVoiceData *vd;
	VocExtBlock *eb;
	u_long nextblock, in_buffer;
	u_char *data = audiobuf;
	char was_extended = 0, output = 0;
	u_short *sp, repeat = 0;
	u_long silence;
	int filepos = 0;

#define COUNT(x)	nextblock -= x; in_buffer -=x ;data += x
#define COUNT1(x)	in_buffer -=x ;data += x

	if (!quiet_mode) {
		fprintf(stderr, "Playing Creative Labs Voice file '%s'...\n", name);
	}
	/* first we waste the rest of header, ugly but we don't need seek */
	while (ofs > buffer_size) {
		if (read(fd, audiobuf, buffer_size) != buffer_size) {
			fprintf(stderr, "%s: read error\n", command);
			exit(1);
		}
		ofs -= buffer_size;
	}
	if (ofs)
		if (read(fd, audiobuf, ofs) != ofs) {
			fprintf(stderr, "%s: read error\n", command);
			exit(1);
		}
	format.format = SND_PCM_SFMT_U8;
	format.voices = 1;
	format.rate = DEFAULT_SPEED;
	format_change = 1;
	set_format();

	in_buffer = nextblock = 0;
	while (1) {
	      Fill_the_buffer:	/* need this for repeat */
		if (in_buffer < 32) {
			/* move the rest of buffer to pos 0 and fill the audiobuf up */
			if (in_buffer)
				memcpy(audiobuf, data, in_buffer);
			data = audiobuf;
			if ((l = read(fd, audiobuf + in_buffer, buffer_size - in_buffer)) > 0)
				in_buffer += l;
			else if (!in_buffer) {
				/* the file is truncated, so simulate 'Terminator' 
				   and reduce the datablock for save landing */
				nextblock = audiobuf[0] = 0;
				if (l == -1) {
					perror(name);
					exit(-1);
				}
			}
		}
		while (!nextblock) {	/* this is a new block */
			if (in_buffer < sizeof(VocBlockType))
				return;
			bp = (VocBlockType *) data;
			COUNT1(sizeof(VocBlockType));
			nextblock = VOC_DATALEN(bp);
			if (output && !quiet_mode)
				fprintf(stderr, "\n");	/* write /n after ASCII-out */
			output = 0;
			switch (bp->type) {
			case 0:
#if 0
				d_printf("Terminator\n");
#endif
				return;		/* VOC-file stop */
			case 1:
				vd = (VocVoiceData *) data;
				COUNT1(sizeof(VocVoiceData));
				/* we need a SYNC, before we can set new SPEED, STEREO ... */

				if (!was_extended) {
					format.rate = (int) (vd->tc);
					format.rate = 1000000 / (256 - format.rate);
#if 0
					d_printf("Voice data %d Hz\n", dsp_speed);
#endif
					if (vd->pack) {		/* /dev/dsp can't it */
						fprintf(stderr, "%s: can't play packed .voc files\n", command);
						return;
					}
					if (format.voices == 2)		/* if we are in Stereo-Mode, switch back */
						format.voices = 1;
				} else {	/* there was extended block */
					format.voices = 2;
					was_extended = 0;
				}
				format_change = 1;
				set_format();
				break;
			case 2:	/* nothing to do, pure data */
#if 0
				d_printf("Voice continuation\n");
#endif
				break;
			case 3:	/* a silence block, no data, only a count */
				sp = (u_short *) data;
				COUNT1(sizeof(u_short));
				format.rate = (int) (*data);
				COUNT1(1);
				format.rate = 1000000 / (256 - format.rate);
				format_change = 1;
				set_format();
				silence = (((u_long) * sp) * 1000) / format.rate;
#if 0
				d_printf("Silence for %d ms\n", (int) silence);
#endif
				write_zeros(*sp);
				snd_pcm_flush_playback(pcm_handle);
				break;
			case 4:	/* a marker for syncronisation, no effect */
				sp = (u_short *) data;
				COUNT1(sizeof(u_short));
#if 0
				d_printf("Marker %d\n", *sp);
#endif
				break;
			case 5:	/* ASCII text, we copy to stderr */
				output = 1;
#if 0
				d_printf("ASCII - text :\n");
#endif
				break;
			case 6:	/* repeat marker, says repeatcount */
				/* my specs don't say it: maybe this can be recursive, but
				   I don't think somebody use it */
				repeat = *(u_short *) data;
				COUNT1(sizeof(u_short));
#if 0
				d_printf("Repeat loop %d times\n", repeat);
#endif
				if (filepos >= 0) {	/* if < 0, one seek fails, why test another */
					if ((filepos = lseek(fd, 0, 1)) < 0) {
						fprintf(stderr, "%s: can't play loops; %s isn't seekable\n",
							command, name);
						repeat = 0;
					} else {
						filepos -= in_buffer;	/* set filepos after repeat */
					}
				} else {
					repeat = 0;
				}
				break;
			case 7:	/* ok, lets repeat that be rewinding tape */
				if (repeat) {
					if (repeat != 0xFFFF) {
#if 0
						d_printf("Repeat loop %d\n", repeat);
#endif
						--repeat;
					}
#if 0
					else
						d_printf("Neverending loop\n");
#endif
					lseek(fd, filepos, 0);
					in_buffer = 0;	/* clear the buffer */
					goto Fill_the_buffer;
				}
#if 0
				else
					d_printf("End repeat loop\n");
#endif
				break;
			case 8:	/* the extension to play Stereo, I have SB 1.0 :-( */
				was_extended = 1;
				eb = (VocExtBlock *) data;
				COUNT1(sizeof(VocExtBlock));
				format.rate = (int) (eb->tc);
				format.rate = 256000000L / (65536 - format.rate);
				format.voices = eb->mode == VOC_MODE_STEREO ? 2 : 1;
				if (format.voices == 2)
					format.rate = format.rate >> 1;
				if (eb->pack) {		/* /dev/dsp can't it */
					fprintf(stderr, "%s: can't play packed .voc files\n", command);
					return;
				}
#if 0
				d_printf("Extended block %s %d Hz\n",
					 (eb->mode ? "Stereo" : "Mono"), dsp_speed);
#endif
				break;
			default:
				fprintf(stderr, "%s: unknown blocktype %d. terminate.\n",
					command, bp->type);
				return;
			}	/* switch (bp->type) */
		}		/* while (! nextblock)  */
		/* put nextblock data bytes to dsp */
		l = in_buffer;
		if (nextblock < l)
			l = nextblock;
		if (l) {
			if (output && !quiet_mode) {
				if (write(2, data, l) != l) {	/* to stderr */
					fprintf(stderr, "%s: write error\n", command);
					exit(1);
				}
			} else {
				if (fcn_write(pcm_handle, data, l) != l) {
					fprintf(stderr, "%s: write error\n", command);
					exit(1);
				}
			}
			COUNT(l);
		}
	}			/* while(1) */
}
/* that was a big one, perhaps somebody split it :-) */

/* setting the globals for playing raw data */
static void init_raw_data(void)
{
	format = rformat;
}

/* calculate the data count to read from/to dsp */
static u_long calc_count(void)
{
	u_long count;

	if (!timelimit)
		count = 0x7fffffff;
	else {
		count = timelimit * format.rate * format.voices;
		switch (format.format) {
		case SND_PCM_SFMT_S16_LE:
		case SND_PCM_SFMT_S16_BE:
		case SND_PCM_SFMT_U16_LE:
		case SND_PCM_SFMT_U16_BE:
			count *= 2;
			break;
		case SND_PCM_SFMT_IMA_ADPCM:
			count /= 4;
			break;
		}
	}
	return count;
}

/* write a .VOC-header */
static void begin_voc(int fd, u_long cnt)
{
	VocHeader vh;
	VocBlockType bt;
	VocVoiceData vd;
	VocExtBlock eb;

	strncpy(vh.magic, VOC_MAGIC_STRING, 20);
	vh.magic[19] = 0x1A;
	vh.headerlen = sizeof(VocHeader);
	vh.version = VOC_ACTUAL_VERSION;
	vh.coded_ver = 0x1233 - VOC_ACTUAL_VERSION;

	if (write(fd, &vh, sizeof(VocHeader)) != sizeof(VocHeader)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(1);
	}
	if (format.voices > 1) {
		/* write a extended block */
		bt.type = 8;
		bt.datalen = 4;
		bt.datalen_m = bt.datalen_h = 0;
		if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
			fprintf(stderr, "%s: write error\n", command);
			exit(1);
		}
		eb.tc = (u_short) (65536 - 256000000L / (format.rate << 1));
		eb.pack = 0;
		eb.mode = 1;
		if (write(fd, &eb, sizeof(VocExtBlock)) != sizeof(VocExtBlock)) {
			fprintf(stderr, "%s: write error\n", command);
			exit(1);
		}
	}
	bt.type = 1;
	cnt += sizeof(VocVoiceData);	/* Voice_data block follows */
	bt.datalen = (u_char) (cnt & 0xFF);
	bt.datalen_m = (u_char) ((cnt & 0xFF00) >> 8);
	bt.datalen_h = (u_char) ((cnt & 0xFF0000) >> 16);
	if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(1);
	}
	vd.tc = (u_char) (256 - (1000000 / format.rate));
	vd.pack = 0;
	if (write(fd, &vd, sizeof(VocVoiceData)) != sizeof(VocVoiceData)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(1);
	}
}

/* write a WAVE-header */
static void begin_wave(int fd, u_long cnt)
{
	WaveHeader wh;
	int bits;

	bits = 8;
	switch (format.format) {
	case SND_PCM_SFMT_U8:
		bits = 8;
		break;
	case SND_PCM_SFMT_S16_LE:
		bits = 16;
		break;
	default:
		fprintf(stderr, "%s: Wave doesn't support %s format...\n", command, snd_pcm_get_format_name(format.format));
		exit(1);
	}
	wh.main_chunk = WAV_RIFF;
	wh.length = cnt + sizeof(WaveHeader) - 8;
	wh.chunk_type = WAV_WAVE;
	wh.sub_chunk = WAV_FMT;
	wh.sc_len = 16;
	wh.format = WAV_PCM_CODE;
	wh.modus = format.voices;
	wh.sample_fq = format.rate;
#if 0
	wh.byte_p_spl = (samplesize == 8) ? 1 : 2;
	wh.byte_p_sec = dsp_speed * wh.modus * wh.byte_p_spl;
#else
	wh.byte_p_spl = wh.modus * ((bits + 7) / 8);
	wh.byte_p_sec = wh.byte_p_spl * format.rate;
#endif
	wh.bit_p_spl = bits;
	wh.data_chunk = WAV_DATA;
	wh.data_length = cnt;
	if (write(fd, &wh, sizeof(WaveHeader)) != sizeof(WaveHeader)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(1);
	}
}

/* write a Au-header */
static void begin_au(int fd, u_long cnt)
{
	AuHeader ah;

	ah.magic = htonl(AU_MAGIC);
	ah.hdr_size = htonl(24);
	ah.data_size = htonl(cnt);
	switch (format.format) {
	case SND_PCM_SFMT_MU_LAW:
		ah.encoding = htonl(AU_FMT_ULAW);
		break;
	case SND_PCM_SFMT_U8:
		ah.encoding = htonl(AU_FMT_LIN8);
		break;
	case SND_PCM_SFMT_S16_LE:
		ah.encoding = htonl(AU_FMT_LIN16);
		break;
	default:
		fprintf(stderr, "%s: Sparc Audio doesn't support %s format...\n", command, snd_pcm_get_format_name(format.format));
		exit(1);
	}
	ah.sample_rate = htonl(format.rate);
	ah.channels = htonl(format.voices);
	if (write(fd, &ah, sizeof(AuHeader)) != sizeof(AuHeader)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(1);
	}
}

/* closing .VOC */
static void end_voc(int fd)
{
	char dummy = 0;		/* Write a Terminator */
	if (write(fd, &dummy, 1) != 1) {
		fprintf(stderr, "%s: write error", command);
		exit(1);
	}
	if (fd != 1)
		close(fd);
}

static void end_wave(int fd)
{				/* only close output */
	if (fd != 1)
		close(fd);
}

static void header(int rtype, char *name)
{
	if (!quiet_mode) {
		fprintf(stderr, "%s %s '%s' : ",
			(channel == SND_PCM_CHANNEL_PLAYBACK) ? "Playing" : "Recording",
			fmt_rec_table[rtype].what,
			name);
		fprintf(stderr, "%s, ", snd_pcm_get_format_name(format.format));
		fprintf(stderr, "Rate %d Hz, ", format.rate);
		if (format.voices == 1)
			fprintf(stderr, "Mono");
		else if (format.voices == 2)
			fprintf(stderr, "Stereo");
		else
			fprintf(stderr, "Voices %i", format.voices);
		fprintf(stderr, "\n");
	}
}

/* playback write error hander */

void playback_write_error(void)
{
	snd_pcm_channel_status_t status;
	
	memset(&status, 0, sizeof(status));
	status.channel = channel;
	if (fcn_status(pcm_handle, &status)<0) {
		fprintf(stderr, "playback channel status error\n");
		exit(1);
	}
	if (status.status == SND_PCM_STATUS_UNDERRUN) {
		printf("underrun at position %u!!!\n", status.scount);
		if (snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK)<0) {
			fprintf(stderr, "underrun: playback channel prepare error\n");
			exit(1);
		}
		frag = 0;
		return;		/* ok, data should be accepted again */
	}
	fprintf(stderr, "write error\n");
	exit(1);
}

/* capture read error hander */

void capture_read_error(void)
{
	snd_pcm_channel_status_t status;
	
	memset(&status, 0, sizeof(status));
	status.channel = channel;
	if (fcn_status(pcm_handle, &status)<0) {
		fprintf(stderr, "capture channel status error\n");
		exit(1);
	}
	if (status.status == SND_PCM_STATUS_OVERRUN) {
		printf("overrun at position %u!!!\n", status.scount);
		if (snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_CAPTURE)<0) {
			fprintf(stderr, "overrun: capture channel prepare error\n");
			exit(1);
		}
		frag = 0;
		return;		/* ok, data should be accepted again */
	}
	fprintf(stderr, "read error\n");
	exit(1);
}

/* playing raw data */

void playback_go(int fd, int loaded, u_long count, int rtype, char *name)
{
	int l, r;
	u_long c;

	header(rtype, name);
	format_change = 1;
	set_format();

	while (count) {
		l = loaded;
		loaded = 0;
		do {
			c = count;

			if (c + l > buffer_size)
				c = buffer_size - l;
				
			if ((r = read(fd, audiobuf + l, c)) <= 0)
				break;
			l += r;
		} while (mode != SND_PCM_MODE_STREAM || l < buffer_size);
		if (l > 0) {
#if 0
			sleep(1);
#endif
			if (mmap_flag) {
				if (l != buffer_size)
					memset(audiobuf + l, silence, buffer_size - l);
				while (mmap_control->fragments[frag].data) {
					switch (mmap_control->status.status) {
					case SND_PCM_STATUS_PREPARED:
						if (snd_pcm_channel_go(pcm_handle, SND_PCM_CHANNEL_PLAYBACK)<0) {
							fprintf(stderr, "%s: unable to start playback\n", command);
							exit(1);
						}
						break;
					case SND_PCM_STATUS_RUNNING:
						break;
					case SND_PCM_STATUS_UNDERRUN:
						playback_write_error();
						break;
					default:
						fprintf(stderr, "%s: bad status (mmap) = %i\n", command, mmap_control->status.status);
					}
					usleep(10000);
				}
				memcpy(mmap_data + mmap_control->fragments[frag].addr, audiobuf, buffer_size);
				mmap_control->fragments[frag].data = 1;
				frag++; frag %= frags;
			} else if (mode == SND_PCM_MODE_BLOCK) {
				if (l != buffer_size)
					memset(audiobuf + l, silence, buffer_size - l);
				while (fcn_write(pcm_handle, audiobuf, buffer_size) != buffer_size)
					playback_write_error();
				count -= l;
			} else {
				char *buf = audiobuf;
				while (l > 0) {
					while ((r = fcn_write(pcm_handle, buf, l)) < 0) {
						if (r == -EAGAIN) {
							r = 0;
							break;
						}
						playback_write_error();
					}
#if 0
					{
						static int x = 1024*1024;
						if (r > 0 && r < x) {
							x = r;
							printf("smallest - %i\n", x);
						}
					}			
#endif				
					l -= r;
					count -= r;
					buf += r;
					if (r < 32)
						usleep(10000);
				}
			}
		} else {
			if (l == -1)
				perror(name);
			count = 0;	/* Stop */
		}
	}			/* while (count) */
	fcn_flush(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
}

/* captureing raw data, this proc handels WAVE files and .VOCs (as one block) */

void capture_go(int fd, int loaded, u_long count, int rtype, char *name)
{
	int l;
	u_long c;

	header(rtype, name);
	format_change = 1;
	set_format();

	while (count) {
		c = count;
		if (mmap_flag) {
			while (!mmap_control->fragments[frag].data) {
				switch (mmap_control->status.status) {
				case SND_PCM_STATUS_PREPARED:
					if (snd_pcm_channel_go(pcm_handle, SND_PCM_CHANNEL_CAPTURE)<0) {
						fprintf(stderr, "%s: unable to start capture\n", command);
						exit(1);
					}
					break;
				case SND_PCM_STATUS_RUNNING:
					break;
				case SND_PCM_STATUS_OVERRUN:
					capture_read_error();
					break;
				default:
					fprintf(stderr, "%s: bad status (mmap) = %i\n", command, mmap_control->status.status);
				}
				usleep(10000);
			}
			if (c > buffer_size)
				c = buffer_size;
			if (write(fd, mmap_data + mmap_control->fragments[frag].addr, c) != c) {
				perror(name);
				exit(-1);
			}
			mmap_control->fragments[frag].data = 0;
			frag++; frag %= frags;
			count -= c;
		} else {
			if ((l = fcn_read(pcm_handle, audiobuf, buffer_size)) > 0) {
#if 0
				{
					static int x = 1024*1024;
					if (l < x) {
						x = l;
						printf("smallest - %i\n", x);
					}
				}			
#endif
				if (c > l)
					c = l;
				if (write(fd, audiobuf, c) != c) {
					perror(name);
					exit(-1);
				}
				count -= c;
			}
			if (l == -EAGAIN)
				l = 0;
			if (l < 0) {
				fprintf(stderr, "read error: %s\n", snd_strerror(l));
				exit(-1);
			}
			if (l == 0)
				usleep(10000);
		}
	}
}

/*
 *  let's play or capture it (capture_type says VOC/WAVE/raw)
 */

static void playback(char *name)
{
	int fd, ofs;

	fcn_flush(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
	if (!name || !strcmp(name, "-")) {
		fd = 0;
		name = "stdin";
	} else {
		if ((fd = open(name, O_RDONLY, 0)) == -1) {
			perror(name);
			exit(1);
		}
	}
	/* read the file header */
	if (read(fd, audiobuf, sizeof(AuHeader)) != sizeof(AuHeader)) {
		fprintf(stderr, "%s: read error", command);
		exit(1);
	}
	if (test_au(fd, audiobuf) >= 0) {
		rformat.format = SND_PCM_SFMT_MU_LAW;
		playback_go(fd, 0, count, FORMAT_AU, name);
		goto __end;
	}
	if (read(fd, audiobuf + sizeof(AuHeader),
		 sizeof(VocHeader) - sizeof(AuHeader)) !=
		 sizeof(VocHeader) - sizeof(AuHeader)) {
		fprintf(stderr, "%s: read error", command);
		exit(1);
	}
	if ((ofs = test_vocfile(audiobuf)) >= 0) {
		voc_play(fd, ofs, name);
		goto __end;
	}
	/* read bytes for WAVE-header */
	if (read(fd, audiobuf + sizeof(VocHeader),
		 sizeof(WaveHeader) - sizeof(VocHeader)) !=
	    sizeof(WaveHeader) - sizeof(VocHeader)) {
		fprintf(stderr, "%s: read error", command);
		exit(1);
	}
	if (test_wavefile(audiobuf) >= 0) {
		playback_go(fd, 0, count, FORMAT_WAVE, name);
	} else {
		/* should be raw data */
		check_new_format(&rformat);
		init_raw_data();
		count = calc_count();
		playback_go(fd, sizeof(WaveHeader), count, FORMAT_RAW, name);
	}
      __end:
	if (fd != 0)
		close(fd);
}

static void capture(char *name)
{
	int fd;

	snd_pcm_flush_capture(pcm_handle);
	if (!name || !strcmp(name, "-")) {
		fd = 1;
		name = "stdout";
	} else {
		remove(name);
		if ((fd = open(name, O_WRONLY | O_CREAT, 0644)) == -1) {
			perror(name);
			exit(1);
		}
	}
	count = calc_count() & 0xFFFFFFFE;
	/* WAVE-file should be even (I'm not sure), but wasting one byte
	   isn't a problem (this can only be in 8 bit mono) */
	if (fmt_rec_table[active_format].start)
		fmt_rec_table[active_format].start(fd, count);
	check_new_format(&rformat);
	capture_go(fd, 0, count, active_format, name);
	fmt_rec_table[active_format].end(fd);
}
