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
#include <assert.h>
#include <sys/poll.h>
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

static ssize_t (*read_func)(snd_pcm_t *handle, void *buffer, size_t size);
static ssize_t (*write_func)(snd_pcm_t *handle, const void *buffer, size_t size);

static char *command;
static snd_pcm_t *pcm_handle;
static snd_pcm_channel_info_t cinfo;
static snd_pcm_format_t rformat, format;
static snd_pcm_channel_setup_t setup;
static int timelimit = 0;
static int quiet_mode = 0;
static int verbose_mode = 0;
static int format_change = 0;
static int active_format = FORMAT_DEFAULT;
static int mode = SND_PCM_MODE_BLOCK;
static int direction = SND_PCM_OPEN_PLAYBACK;
static int channel = SND_PCM_CHANNEL_PLAYBACK;
static int mmap_flag = 0;
static snd_pcm_mmap_control_t *mmap_control = NULL;
static char *mmap_data = NULL;
static int noplugin = 0;
static int nonblock = 0;
static char *audiobuf = NULL;
static int align = 1;
static int buffer_size = -1;
/* 250 ms */
static int frag_length = 250;
static int show_setup = 0;
static int buffer_pos = 0;

static int count;
static int vocmajor, vocminor;

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

typedef struct {
	int value;
	const char* desc;
} assoc_t;

static const char *assoc(int value, assoc_t *alist)
{
	while (alist->desc) {
		if (value == alist->value)
			return alist->desc;
		alist++;
	}
	return "UNKNOWN";
}

#define CHN(v) { SND_PCM_CHANNEL_##v, #v }
#define MODE(v) { SND_PCM_MODE_##v, #v }
#define FMT(v) { SND_PCM_SFMT_##v, #v }
#define XRUN(v) { SND_PCM_XRUN_##v, #v }
#define START(v) { SND_PCM_START_##v, #v }
#define FILL(v) { SND_PCM_FILL_##v, #v }
#define END { 0, NULL }

static assoc_t chns[] = { CHN(PLAYBACK), CHN(CAPTURE), END };
static assoc_t modes[] = { MODE(STREAM), MODE(BLOCK), END };
static assoc_t fmts[] = { FMT(S8), FMT(U8),
			  FMT(S16_LE), FMT(S16_BE), FMT(U16_LE), FMT(U16_BE), 
			  FMT(S24_LE), FMT(S24_BE), FMT(U24_LE), FMT(U24_BE), 
			  FMT(S32_LE), FMT(S32_BE), FMT(U32_LE), FMT(U32_BE),
			  FMT(FLOAT_LE), FMT(FLOAT_BE), FMT(FLOAT64_LE), FMT(FLOAT64_BE),
			  FMT(IEC958_SUBFRAME_LE), FMT(IEC958_SUBFRAME_BE),
			  FMT(MU_LAW), FMT(A_LAW), FMT(IMA_ADPCM),
			  FMT(MPEG), FMT(GSM), FMT(SPECIAL), END };
static assoc_t starts[] = { START(GO), START(DATA), START(FULL), END };
static assoc_t xruns[] = { XRUN(FLUSH), XRUN(DRAIN), END };
static assoc_t fills[] = { FILL(NONE), FILL(SILENCE_WHOLE), FILL(SILENCE), END };
static assoc_t onoff[] = { {0, "OFF"}, {1, "ON"}, {-1, "ON"}, END };

static void check_new_format(snd_pcm_format_t * format)
{
        if (cinfo.rates & (SND_PCM_RATE_CONTINUOUS|SND_PCM_RATE_KNOT)) {
                if (format->rate < cinfo.min_rate ||
                    format->rate > cinfo.max_rate) {
			fprintf(stderr, "%s: unsupported rate %iHz (valid range is %iHz-%iHz)\n", command, format->rate, cinfo.min_rate, cinfo.max_rate);
			exit(EXIT_FAILURE);
		}
        } else {
		unsigned int r;
                switch (format->rate) {
                case 8000:      r = SND_PCM_RATE_8000; break;
                case 11025:     r = SND_PCM_RATE_11025; break;
                case 16000:     r = SND_PCM_RATE_16000; break;
                case 22050:     r = SND_PCM_RATE_22050; break;
                case 32000:     r = SND_PCM_RATE_32000; break;
                case 44100:     r = SND_PCM_RATE_44100; break;
                case 48000:     r = SND_PCM_RATE_48000; break;
                case 88200:     r = SND_PCM_RATE_88200; break;
                case 96000:     r = SND_PCM_RATE_96000; break;
                case 176400:    r = SND_PCM_RATE_176400; break;
                case 192000:    r = SND_PCM_RATE_192000; break;
                default:        r = 0; break;
                }
                if (!(cinfo.rates & r)) {
			fprintf(stderr, "%s: unsupported rate %iHz\n", command, format->rate);
			exit(EXIT_FAILURE);
		}
	}
	if (cinfo.min_voices > format->voices || cinfo.max_voices < format->voices) {
		fprintf(stderr, "%s: unsupported number of voices %i (valid range is %i-%i)\n", command, format->voices, cinfo.min_voices, cinfo.max_voices);
		exit(EXIT_FAILURE);
	}
	if (!(cinfo.formats & (1 << format->format))) {
		fprintf(stderr, "%s: unsupported format %s\n", command, snd_pcm_get_format_name(format->format));
		exit(EXIT_FAILURE);
	}
	if (format->voices > 1) {
		if (format->interleave) {
			if (!(cinfo.flags & SND_PCM_CHNINFO_INTERLEAVE)) {
				fprintf(stderr, "%s: unsupported interleaved format\n", command);
				exit(EXIT_FAILURE);
			}
		} else if (!(cinfo.flags & SND_PCM_CHNINFO_NONINTERLEAVE)) {
			fprintf(stderr, "%s: unsupported non interleaved format\n", command);
			exit(EXIT_FAILURE);
		}
	}
}

static void usage(char *command)
{
	assoc_t *f;
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
		"  -o <voices>   voices (1-N)\n"
		"  -s <Hz>       speed (Hz)\n"
		"  -f <format>   data format\n"
		"  -F <msec>     fragment length\n"
		"  -m            set CD-ROM quality (44100Hz,stereo,16-bit linear,little endian)\n"
		"  -M            set DAT quality (48000Hz,stereo,16-bit linear,little endian)\n"
		"  -t <secs>     timelimit (seconds)\n"
		"  -e            stream mode\n"
		"  -E            mmap mode\n"
		"  -N            Don't use plugins\n"
		"  -B            Nonblocking mode\n"
		"  -S            Show setup\n"
		,command, snd_cards()-1);
	fprintf(stderr, "\nRecognized data formats are:");
	for (f = fmts; f->desc; ++f)
		fprintf(stderr, " %s", f->desc);
	fprintf(stderr, " (some of these may not be available on selected hardware)\n");
}

static void device_list(void)
{
	snd_ctl_t *handle;
	int card, err, dev, idx;
	unsigned int mask;
	snd_ctl_hw_info_t info;
	snd_pcm_info_t pcminfo;
	snd_pcm_channel_info_t chninfo;

	mask = snd_cards_mask();
	if (!mask) {
		fprintf(stderr, "%s: no soundcards found...\n", command);
		return;
	}
	for (card = 0; card < SND_CARDS; card++) {
		if (!(mask & (1 << card)))
			continue;
		if ((err = snd_ctl_open(&handle, card)) < 0) {
			fprintf(stderr, "Error: control open (%i): %s\n", card, snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
			fprintf(stderr, "Error: control hardware info (%i): %s\n", card, snd_strerror(err));
			snd_ctl_close(handle);
			continue;
		}
		for (dev = 0; dev < info.pcmdevs; dev++) {
			if ((err = snd_ctl_pcm_info(handle, dev, &pcminfo)) < 0) {
				fprintf(stderr, "Error: control digital audio info (%i): %s\n", card, snd_strerror(err));
				continue;
			}
			fprintf(stderr, "%s: %i [%s] / #%i: %s\n",
			       info.name,
			       card + 1,
			       info.id,
			       dev,
			       pcminfo.name);
			fprintf(stderr, "  Directions: %s%s%s\n",
			       pcminfo.flags & SND_PCM_INFO_PLAYBACK ? "playback " : "",
			       pcminfo.flags & SND_PCM_INFO_CAPTURE ? "capture " : "",
			       pcminfo.flags & SND_PCM_INFO_DUPLEX ? "duplex " : "");
			fprintf(stderr, "  Playback subdevices: %i\n", pcminfo.playback + 1);
			fprintf(stderr, "  Capture subdevices: %i\n", pcminfo.capture + 1);
			if (pcminfo.flags & SND_PCM_INFO_PLAYBACK) {
				for (idx = 0; idx <= pcminfo.playback; idx++) {
					memset(&chninfo, 0, sizeof(chninfo));
					chninfo.channel = SND_PCM_CHANNEL_PLAYBACK;
					if ((err = snd_ctl_pcm_channel_info(handle, dev, SND_PCM_CHANNEL_PLAYBACK, idx, &chninfo)) < 0) {
						fprintf(stderr, "Error: control digital audio playback info (%i): %s\n", card, snd_strerror(err));
					} else {
						fprintf(stderr, "  Playback subdevice #%i: %s\n", idx, chninfo.subname);
					}
				}
			}
			if (pcminfo.flags & SND_PCM_INFO_CAPTURE) {
				for (idx = 0; idx <= pcminfo.capture; idx++) {
					memset(&chninfo, 0, sizeof(chninfo));
					chninfo.channel = SND_PCM_CHANNEL_CAPTURE;
					if ((err = snd_ctl_pcm_channel_info(handle, dev, SND_PCM_CHANNEL_CAPTURE, 0, &chninfo)) < 0) {
						fprintf(stderr, "Error: control digital audio capture info (%i): %s\n", card, snd_strerror(err));
					} else {
						fprintf(stderr, "  Capture subdevice #%i: %s\n", idx, chninfo.subname);
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
	while ((c = getopt(argc, argv, "hlc:d:qs:o:t:vrwxc:p:mMVeEf:NBF:S")) != EOF)
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
		case 'o':
			tmp = atoi(optarg);
			if (tmp < 1 || tmp > 32) {
				fprintf(stderr, "Error: value %i for voices is invalid\n", tmp);
				return 1;
			}
			rformat.voices = tmp;
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
		case 'f': {
			assoc_t *f;
			for (f = fmts; f->desc; ++f) {
				if (!strcasecmp(optarg, f->desc)) {
					break;
				}
			}
			if (!f->desc) {
				fprintf(stderr, "Error: wrong extended format '%s'\n", optarg);
				exit(EXIT_FAILURE);
			}
			rformat.format = f->value;
			active_format = FORMAT_RAW;
			break;
		}
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
			mode = SND_PCM_MODE_STREAM;
			break;
		case 'E':
			mmap_flag = 1;
			break;
		case 'N':
			noplugin = 1;
			break;
		case 'B':
			nonblock = 1;
			break;
		case 'F':
			frag_length = atoi(optarg);
			break;
		case 'S':
			show_setup = 1;
			break;
		default:
			usage(command);
			return 1;
		}

	if (!quiet_mode)
		version();

	if (!quiet_mode) {
		char *cardname;
		
		if ((err = snd_card_get_longname(card, &cardname)) < 0) {
			fprintf(stderr, "Error: unable to obtain longname: %s\n", snd_strerror(err));
			return 1;
		}
		fprintf(stderr, "Using soundcard '%s'\n", cardname);
		free(cardname);
	}

	if (noplugin)
		err = snd_pcm_open(&pcm_handle, card, dev, direction);
	else
		err = snd_pcm_plug_open(&pcm_handle, card, dev, direction);
	if (err < 0) {
		fprintf(stderr, "Error: audio open error: %s\n", snd_strerror(err));
		return 1;
	}
	if (nonblock) {
		err = snd_pcm_channel_nonblock(pcm_handle, channel, 1);
		if (err < 0) {
			fprintf(stderr, "nonblock setting error: %s\n", snd_strerror(err));
			return 1;
		}
	}
	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.channel = channel;
	if ((err = snd_pcm_channel_info(pcm_handle, &cinfo)) < 0) {
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

	if (mmap_flag) {
		write_func = snd_pcm_mmap_write;
		read_func = snd_pcm_mmap_read;
	} else {
		write_func = snd_pcm_write;
		read_func = snd_pcm_read;
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
	return EXIT_SUCCESS;
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
			exit(EXIT_FAILURE);
		}
		if (wp->modus < 1 || wp->modus > 32) {
			fprintf(stderr, "%s: can't play WAVE-files with %d tracks\n",
				command, wp->modus);
			exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
	}
	check_new_format(&format);
	return 0;
}

static void setup_print(snd_pcm_channel_setup_t *setup)
{
	fprintf(stderr, "channel: %s\n", assoc(setup->channel, chns));
	fprintf(stderr, "mode: %s\n", assoc(setup->mode, modes));
	fprintf(stderr, "format: %s\n", assoc(setup->format.format, fmts));
	fprintf(stderr, "voices: %d\n", setup->format.voices);
	fprintf(stderr, "rate: %d\n", setup->format.rate);
	// digital
	fprintf(stderr, "start_mode: %s\n", assoc(setup->start_mode, starts));
	fprintf(stderr, "xrun_mode: %s\n", assoc(setup->xrun_mode, xruns));
	fprintf(stderr, "time: %s\n", assoc(setup->time, onoff));
	// ust_time
	// sync
	fprintf(stderr, "buffer_size: %d\n", setup->buffer_size);
	fprintf(stderr, "frag_size: %d\n", setup->frag_size);
	fprintf(stderr, "frags: %d\n", setup->frags);
	fprintf(stderr, "frag_boundary: %d\n", setup->frag_boundary);
	fprintf(stderr, "pos_boundary: %d\n", setup->pos_boundary);
	fprintf(stderr, "msbits_per_sample: %d\n", setup->msbits_per_sample);
	if (setup->mode == SND_PCM_MODE_STREAM) {
		fprintf(stderr, "bytes_min: %d\n", setup->buf.stream.bytes_min);
		fprintf(stderr, "bytes_align: %d\n", setup->buf.stream.bytes_align);
		fprintf(stderr, "bytes_xrun_max: %d\n", setup->buf.stream.bytes_xrun_max);
		fprintf(stderr, "fill: %s\n", assoc(setup->buf.stream.fill, fills));
		fprintf(stderr, "bytes_fill_max: %d\n", setup->buf.stream.bytes_fill_max);
	} else if (setup->mode == SND_PCM_MODE_BLOCK) {
		fprintf(stderr, "frags_min: %d\n", setup->buf.block.frags_min);
		fprintf(stderr, "frags_xrun_max: %d\n", setup->buf.block.frags_xrun_max);
	}
}

static void set_format(void)
{
	snd_pcm_channel_params_t params;

	if (!format_change)
		return;
	align = (snd_pcm_format_physical_width(format.format) + 7) / 8;

	if (mmap_flag)
		snd_pcm_munmap(pcm_handle, channel);
	snd_pcm_channel_flush(pcm_handle, channel);		/* to be in right state */

	memset(&params, 0, sizeof(params));
	params.mode = mode;
	params.channel = channel;
	memcpy(&params.format, &format, sizeof(format));
	if (channel == SND_PCM_CHANNEL_PLAYBACK) {
		params.start_mode = SND_PCM_START_FULL;
	} else {
		params.start_mode = SND_PCM_START_DATA;
	}
	params.xrun_mode = SND_PCM_XRUN_FLUSH;
	params.frag_size = snd_pcm_format_bytes_per_second(&format) / 1000.0 * frag_length;
	params.buffer_size = params.frag_size * 4;
	if (mode == SND_PCM_MODE_BLOCK) {
		params.buf.block.frags_min = 1;
		params.buf.block.frags_xrun_max = 0;
	} else {
		params.buf.stream.fill = SND_PCM_FILL_SILENCE;
		params.buf.stream.bytes_fill_max = 1024;
		params.buf.stream.bytes_min = 1024;
		params.buf.stream.bytes_xrun_max = 0;
	}
	if (snd_pcm_channel_params(pcm_handle, &params) < 0) {
		fprintf(stderr, "%s: unable to set channel params\n", command);
		exit(EXIT_FAILURE);
	}
	if (mmap_flag) {
		if (snd_pcm_mmap(pcm_handle, channel, &mmap_control, (void **)&mmap_data)<0) {
			fprintf(stderr, "%s: unable to mmap memory\n", command);
			exit(EXIT_FAILURE);
		}
	}
	if (snd_pcm_channel_prepare(pcm_handle, channel) < 0) {
		fprintf(stderr, "%s: unable to prepare channel\n", command);
		exit(EXIT_FAILURE);
	}
	memset(&setup, 0, sizeof(setup));
	setup.channel = channel;
	if (snd_pcm_channel_setup(pcm_handle, &setup) < 0) {
		fprintf(stderr, "%s: unable to obtain setup\n", command);
		exit(EXIT_FAILURE);
	}

	if (show_setup)
		setup_print(&setup);

	buffer_size = setup.frag_size;
	audiobuf = (char *)realloc(audiobuf, buffer_size > 1024 ? buffer_size : 1024);
	if (audiobuf == NULL) {
		fprintf(stderr, "%s: not enough memory\n", command);
		exit(EXIT_FAILURE);
	}
	// fprintf(stderr, "real buffer_size = %i, frags = %i, total = %i\n", buffer_size, setup.buf.block.frags, setup.buf.block.frags * buffer_size);
	format_change = 0;
}

/* playback write error hander */

void playback_write_error(void)
{
	snd_pcm_channel_status_t status;
	
	memset(&status, 0, sizeof(status));
	status.channel = channel;
	if (snd_pcm_channel_status(pcm_handle, &status)<0) {
		fprintf(stderr, "playback channel status error\n");
		exit(EXIT_FAILURE);
	}
	if (status.status == SND_PCM_STATUS_XRUN) {
		fprintf(stderr, "underrun at position %u!!!\n", status.pos_io);
		if (snd_pcm_channel_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK)<0) {
			fprintf(stderr, "underrun: playback channel prepare error\n");
			exit(EXIT_FAILURE);
		}
		return;		/* ok, data should be accepted again */
	}
	fprintf(stderr, "write error\n");
	exit(EXIT_FAILURE);
}

/* capture read error hander */

void capture_read_error(void)
{
	snd_pcm_channel_status_t status;
	
	memset(&status, 0, sizeof(status));
	status.channel = channel;
	if (snd_pcm_channel_status(pcm_handle, &status)<0) {
		fprintf(stderr, "capture channel status error\n");
		exit(EXIT_FAILURE);
	}
	if (status.status == SND_PCM_STATUS_RUNNING)
		return;		/* everything is ok, but the driver is waiting for data */
	if (status.status == SND_PCM_STATUS_XRUN) {
		fprintf(stderr, "overrun at position %u!!!\n", status.pos_io);
		if (snd_pcm_channel_prepare(pcm_handle, SND_PCM_CHANNEL_CAPTURE)<0) {
			fprintf(stderr, "overrun: capture channel prepare error\n");
			exit(EXIT_FAILURE);
		}
		return;		/* ok, data should be accepted again */
	}
	fprintf(stderr, "read error\n");
	exit(EXIT_FAILURE);
}

/*
 *  write function
 */

static ssize_t pcm_write(u_char *data, size_t count)
{
	char *buf = data;
	ssize_t result = count, r;

	count += align - 1;
	count -= count % align;
	if (mode == SND_PCM_MODE_BLOCK) {
		if (count != buffer_size)
			snd_pcm_format_set_silence(format.format, buf + count, buffer_size - count);
		while (1) {
			int bytes = write_func(pcm_handle, buf, buffer_size);
			if (bytes == -EAGAIN || bytes == 0) {
				struct pollfd pfd;
				pfd.fd = snd_pcm_file_descriptor(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
				pfd.events = POLLOUT | POLLERR;
				poll(&pfd, 1, 1000);
			} else if (bytes == -EPIPE) {
				playback_write_error();
			} else if (bytes != buffer_size) {
				fprintf(stderr, "write error: %s\n", snd_strerror(bytes));
				exit(EXIT_FAILURE);
			} else break;
		}
	} else {
		while (count > 0) {
			struct pollfd pfd;
			r = write_func(pcm_handle, buf, count);
			if (r == -EPIPE) {
				playback_write_error();
				continue;
			}
			if (r < 0 && r != -EAGAIN) {
				fprintf(stderr, "write error: %s\n", snd_strerror(r));
				exit(EXIT_FAILURE);
			}
			if (r != count) {
				pfd.fd = snd_pcm_file_descriptor(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
				pfd.events = POLLOUT | POLLERR;
#ifdef FIXED_STREAM_POLL
				poll(&pfd, 1, 1000);
#else
				poll(&pfd, 1, 50);
#endif
			}
			if (r > 0) {
				count -= r;
				buf += r;
			}
		}
	}
	return result;
}

/*
 *  read function
 */

static ssize_t pcm_read(u_char *data, size_t count)
{
	ssize_t r;
	size_t result = 0;

	while (result < count) {
		r = read_func(pcm_handle, audiobuf + result, count - result);
		if (r == -EAGAIN || (r >= 0 && r < count - result)) {
			struct pollfd pfd;
			pfd.fd = snd_pcm_file_descriptor(pcm_handle, SND_PCM_CHANNEL_CAPTURE);
			pfd.events = POLLIN | POLLERR;
#ifndef FIXED_STREAM_POLL
			if (mode == SND_PCM_MODE_STREAM)
				poll(&pfd, 1, 50);
			else
#endif
				poll(&pfd, 1, 1000);
		} else if (r == -EPIPE) {
			capture_read_error();
		} else if (r < 0) {
			fprintf(stderr, "read error: %s\n", snd_strerror(r));
			exit(EXIT_FAILURE);
		}
		if (r > 0)
			result += r;
	}
	return result;
}

/*
 *  ok, let's play a .voc file
 */

static ssize_t voc_pcm_write(u_char *data, size_t count)
{
	ssize_t result = count, r;
	size_t size;

	while (count > 0) {
		size = count;
		if (size > buffer_size - buffer_pos)
			size = buffer_size - buffer_pos;
		memcpy(audiobuf + buffer_pos, data, size);
		data += size;
		count -= size;
		buffer_pos += size;
		if (buffer_pos == buffer_size) {
			if ((r = pcm_write(audiobuf, buffer_size)) != buffer_size)
				return r;
			buffer_pos = 0;
		}
	}
	return result;
}

/*
 *  writing zeros from the zerobuf to simulate silence,
 *  perhaps it's enough to use a long var instead of zerobuf ?
 */
static void voc_write_zeros(unsigned x)
{
	unsigned l;
	char *buf;

	buf = (char *) malloc(buffer_size);
	if (buf == NULL) {
		fprintf(stderr, "%s: can allocate buffer for zeros\n", command);
		return;		/* not fatal error */
	}
	snd_pcm_format_set_silence(format.format, buf, buffer_size);
	while (x > 0) {
		l = x;
		if (l > buffer_size)
			l = buffer_size;
		if (voc_pcm_write(buf, l) != l) {
			fprintf(stderr, "%s: write error\n", command);
			exit(EXIT_FAILURE);
		}
		x -= l;
	}
}

static void voc_pcm_flush(void)
{
	if (buffer_pos > 0) {
		if (mode == SND_PCM_MODE_BLOCK) {
			if (snd_pcm_format_set_silence(format.format, audiobuf + buffer_pos, buffer_size - buffer_pos) < 0)
				fprintf(stderr, "voc_pcm_flush - silence error\n");
			buffer_pos = buffer_size;
		}
		if (pcm_write(audiobuf, buffer_pos) != buffer_pos)
			fprintf(stderr, "voc_pcm_flush error\n");
	}
	snd_pcm_channel_flush(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
}

static void voc_play(int fd, int ofs, char *name)
{
	int l;
	VocBlockType *bp;
	VocVoiceData *vd;
	VocExtBlock *eb;
	u_long nextblock, in_buffer;
	u_char *data, *buf;
	char was_extended = 0, output = 0;
	u_short *sp, repeat = 0;
	u_long silence;
	int filepos = 0;

#define COUNT(x)	nextblock -= x; in_buffer -= x; data += x
#define COUNT1(x)	in_buffer -= x; data += x

	data = buf = (u_char *)malloc(64 * 1024);
	buffer_pos = 0;
	if (data == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(EXIT_FAILURE);
	}
	if (!quiet_mode) {
		fprintf(stderr, "Playing Creative Labs Voice file '%s'...\n", name);
	}
	/* first we waste the rest of header, ugly but we don't need seek */
	while (ofs > buffer_size) {
		if (read(fd, buf, buffer_size) != buffer_size) {
			fprintf(stderr, "%s: read error\n", command);
			exit(EXIT_FAILURE);
		}
		ofs -= buffer_size;
	}
	if (ofs) {
		if (read(fd, buf, ofs) != ofs) {
			fprintf(stderr, "%s: read error\n", command);
			exit(EXIT_FAILURE);
		}
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
			/* move the rest of buffer to pos 0 and fill the buf up */
			if (in_buffer)
				memcpy(buf, data, in_buffer);
			data = buf;
			if ((l = read(fd, buf + in_buffer, buffer_size - in_buffer)) > 0)
				in_buffer += l;
			else if (!in_buffer) {
				/* the file is truncated, so simulate 'Terminator' 
				   and reduce the datablock for safe landing */
				nextblock = buf[0] = 0;
				if (l == -1) {
					perror(name);
					exit(EXIT_FAILURE);
				}
			}
		}
		while (!nextblock) {	/* this is a new block */
			if (in_buffer < sizeof(VocBlockType))
				goto __end;
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
				voc_write_zeros(*sp);
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
					exit(EXIT_FAILURE);
				}
			} else {
				if (voc_pcm_write(data, l) != l) {
					fprintf(stderr, "%s: write error\n", command);
					exit(EXIT_FAILURE);
				}
			}
			COUNT(l);
		}
	}			/* while(1) */
      __end:
        voc_pcm_flush();
        free(buf);
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

	if (!timelimit) {
		count = 0x7fffffff;
	} else {
		count = snd_pcm_format_size(format.format,
					    timelimit * format.rate *
					    format.voices);
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
		exit(EXIT_FAILURE);
	}
	if (format.voices > 1) {
		/* write a extended block */
		bt.type = 8;
		bt.datalen = 4;
		bt.datalen_m = bt.datalen_h = 0;
		if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
			fprintf(stderr, "%s: write error\n", command);
			exit(EXIT_FAILURE);
		}
		eb.tc = (u_short) (65536 - 256000000L / (format.rate << 1));
		eb.pack = 0;
		eb.mode = 1;
		if (write(fd, &eb, sizeof(VocExtBlock)) != sizeof(VocExtBlock)) {
			fprintf(stderr, "%s: write error\n", command);
			exit(EXIT_FAILURE);
		}
	}
	bt.type = 1;
	cnt += sizeof(VocVoiceData);	/* Voice_data block follows */
	bt.datalen = (u_char) (cnt & 0xFF);
	bt.datalen_m = (u_char) ((cnt & 0xFF00) >> 8);
	bt.datalen_h = (u_char) ((cnt & 0xFF0000) >> 16);
	if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(EXIT_FAILURE);
	}
	vd.tc = (u_char) (256 - (1000000 / format.rate));
	vd.pack = 0;
	if (write(fd, &vd, sizeof(VocVoiceData)) != sizeof(VocVoiceData)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
	}
	ah.sample_rate = htonl(format.rate);
	ah.channels = htonl(format.voices);
	if (write(fd, &ah, sizeof(AuHeader)) != sizeof(AuHeader)) {
		fprintf(stderr, "%s: write error\n", command);
		exit(EXIT_FAILURE);
	}
}

/* closing .VOC */
static void end_voc(int fd)
{
	char dummy = 0;		/* Write a Terminator */
	if (write(fd, &dummy, 1) != 1) {
		fprintf(stderr, "%s: write error", command);
		exit(EXIT_FAILURE);
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

/* playing raw data */

void playback_go(int fd, int loaded, u_long count, int rtype, char *name)
{
	int l, r;
	u_long c;

	header(rtype, name);
	format_change = 1;
	set_format();

	l = 0;
	while (loaded > buffer_size) {
		if (pcm_write(audiobuf + l, buffer_size) <= 0)
			return;
		l += buffer_size;
		loaded -= l;
	}

	l = loaded;
	while (count > 0) {
		do {
			c = count;
			if (c + l > buffer_size)
				c = buffer_size - l;
			
			if (c == 0)
				break;
			r = read(fd, audiobuf + l, c);
			if (r <= 0)
				break;
			l += r;
		} while (mode != SND_PCM_MODE_STREAM && l < buffer_size);
		l = pcm_write(audiobuf, l);
		if (l <= 0)
			break;
		count -= l;
		l = 0;
	}
	snd_pcm_channel_flush(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
}

/* captureing raw data, this proc handels WAVE files and .VOCs (as one block) */

void capture_go(int fd, int loaded, u_long count, int rtype, char *name)
{
	size_t c;
	ssize_t r;

	header(rtype, name);
	format_change = 1;
	set_format();

	while (count > 0) {
		c = count;
		if (c > buffer_size)
			c = buffer_size;
		if ((r = pcm_read(audiobuf, c)) <= 0)
			break;
		if (write(fd, audiobuf, r) != r) {
			perror(name);
			exit(EXIT_FAILURE);
		}
		count -= r;
	}
}

/*
 *  let's play or capture it (capture_type says VOC/WAVE/raw)
 */

static void playback(char *name)
{
	int fd, ofs;

	snd_pcm_channel_flush(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
	if (!name || !strcmp(name, "-")) {
		fd = 0;
		name = "stdin";
	} else {
		if ((fd = open(name, O_RDONLY, 0)) == -1) {
			perror(name);
			exit(EXIT_FAILURE);
		}
	}
	/* read the file header */
	if (read(fd, audiobuf, sizeof(AuHeader)) != sizeof(AuHeader)) {
		fprintf(stderr, "%s: read error", command);
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
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

	snd_pcm_capture_flush(pcm_handle);
	if (!name || !strcmp(name, "-")) {
		fd = 1;
		name = "stdout";
	} else {
		remove(name);
		if ((fd = open(name, O_WRONLY | O_CREAT, 0644)) == -1) {
			perror(name);
			exit(EXIT_FAILURE);
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
