/*
 *  aplay.c - plays and records
 *
 *      CREATIVE LABS CHANNEL-files
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
#include <sys/asoundlib.h>
#include <assert.h>
#include <sys/poll.h>
#include <sys/uio.h>
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

static snd_pcm_sframes_t (*readi_func)(snd_pcm_t *handle, void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writei_func)(snd_pcm_t *handle, const void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*readn_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writen_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);

static char *command;
static snd_pcm_t *handle;
static struct {
	snd_pcm_format_t format;
	unsigned int channels;
	unsigned int rate;
} hwparams, rhwparams;
static int timelimit = 0;
static int quiet_mode = 0;
static int file_type = FORMAT_DEFAULT;
static unsigned int sleep_min = 0;
static int open_mode = 0;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int mmap_flag = 0;
static int interleaved = 1;
static int nonblock = 0;
static char *audiobuf = NULL;
static int chunk_size = -1;
static int period_time = -1;
static int buffer_time = -1;
static int avail_min = -1;
static int verbose = 0;
static int buffer_pos = 0;
static size_t bits_per_sample, bits_per_frame;
static size_t chunk_bytes;
static snd_control_type_t digtype = SND_CONTROL_TYPE_NONE;
static snd_aes_iec958_t spdif;
static snd_output_t *log;

static int count;
static int vocmajor, vocminor;

/* needed prototypes */

static void playback(char *filename);
static void capture(char *filename);
static void playbackv(char **filenames, unsigned int count);
static void capturev(char **filenames, unsigned int count);

static void begin_voc(int fd, size_t count);
static void end_voc(int fd);
static void begin_wave(int fd, size_t count);
static void end_wave(int fd);
static void begin_au(int fd, size_t count);

struct fmt_capture {
	void (*start) (int fd, size_t count);
	void (*end) (int fd);
	char *what;
} fmt_rec_table[] = {
	{	NULL,		end_wave,	"raw data"	},
	{	begin_voc,	end_voc,	"VOC"		},
	{	begin_wave,	end_wave,	"WAVE"		},
	{	begin_au,	end_wave,	"Sparc Audio"	}
};

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define error(...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	putc('\n', stderr); \
} while (0)
#else
#define error(args...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, ##args); \
	putc('\n', stderr); \
} while (0)
#endif	

static void usage(char *command)
{
	snd_pcm_format_t k;
	fprintf(stderr, "\
Usage: %s [OPTION]... [FILE]...

--help                   help
--version                print current version
-l, --list-devices       list all soundcards and digital audio devices
-L, --list-pcms          list all PCMs defined
-D, --device=NAME        select PCM by name
-q, --quiet              quiet mode
-t, --file-type TYPE     file type (voc, wav or raw)
-c, --channels=#         channels
-f, --format=FORMAT      sample format (case insensitive)
-r, --rate=#             sample rate
-d, --duration=#         interrupt after # seconds
-s, --sleep-min=#        min ticks to sleep
-M, --mmap               mmap stream
-N, --nonblock           nonblocking mode
-F, --period-time=#      distance between interrupts is # microseconds
-B, --buffer-time=#      buffer duration is # microseconds
-A, --avail-min=#        min available space for wakeup is # microseconds
-X, --xfer-min=#	 min xfer size is # microseconds
-v, --verbose            show PCM structure and setup
-I, --separate-channels  one file for each channel
-P, --iec958p            AES IEC958 professional
-C, --iec958c            AES IEC958 consumer
", command);
	fprintf(stderr, "Recognized sample formats are:");
	for (k = 0; k < SND_PCM_FORMAT_LAST; ++(unsigned long) k) {
		const char *s = snd_pcm_format_name(k);
		if (s)
			fprintf(stderr, " %s", s);
	}
	fprintf(stderr, "\nSome of these may not be available on selected hardware\n");
	fprintf(stderr, "The availabled format shortcuts are:\n");
	fprintf(stderr, "cd (16 bit little endian, 44100, stereo)\n");
	fprintf(stderr, "dat (16 bit little endian, 48000, stereo)\n");
}

static void device_list(void)
{
	snd_ctl_t *handle;
	int card, err, dev, idx;
	snd_ctl_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_ctl_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);

	card = -1;
	if (snd_card_next(&card) < 0 || card < 0) {
		error("no soundcards found...");
		return;
	}
	while (card >= 0) {
		char name[32];
		sprintf(name, "hw:%d", card);
		if ((err = snd_ctl_open(&handle, name)) < 0) {
			error("control open (%i): %s", card, snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_info(handle, info)) < 0) {
			error("control hardware info (%i): %s", card, snd_strerror(err));
			snd_ctl_close(handle);
			continue;
		}
		dev = -1;
		while (1) {
			unsigned int count;
			if (snd_ctl_pcm_next_device(handle, &dev)<0)
				error("snd_ctl_pcm_next_device");
			if (dev < 0)
				break;
			snd_pcm_info_set_device(pcminfo, dev);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, stream);
			if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
				if (err != -ENOENT)
					error("control digital audio info (%i): %s", card, snd_strerror(err));
				continue;
			}
			fprintf(stderr, "card %i: %s [%s], device %i: %s [%s]\n",
				card, snd_ctl_info_get_id(info), snd_ctl_info_get_name(info),
				dev,
				snd_pcm_info_get_id(pcminfo),
				snd_pcm_info_get_name(pcminfo));
			count = snd_pcm_info_get_subdevices_count(pcminfo);
			fprintf(stderr, "  Subdevices: %i/%i\n", snd_pcm_info_get_subdevices_avail(pcminfo), count);
			for (idx = 0; idx < count; idx++) {
				snd_pcm_info_set_subdevice(pcminfo, idx);
				if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
					error("control digital audio playback info (%i): %s", card, snd_strerror(err));
				} else {
					fprintf(stderr, "  Subdevice #%i: %s\n", idx, snd_pcm_info_get_subdevice_name(pcminfo));
				}
			}
		}
		snd_ctl_close(handle);
		if (snd_card_next(&card) < 0) {
			error("snd_card_next");
			break;
		}
	}
}

static void pcm_list(void)
{
	snd_config_t *conf;
	snd_output_t *out;
	int err = snd_config_update();
	if (err < 0)
		error("snd_pcm_update: %s", snd_strerror(err));
	err = snd_output_stdio_attach(&out, stderr, 0);
	assert(err >= 0);
	err = snd_config_search(snd_config, "pcm", &conf);
	if (err < 0)
		return;
	fprintf(stderr, "PCM list:");
	snd_config_save(conf, out);
	snd_output_close(out);
}

static void version(void)
{
	fprintf(stderr, "%s: version " SND_UTIL_VERSION_STR " by Jaroslav Kysela <perex@suse.cz>", command);
}

#define OPT_HELP 1
#define OPT_VERSION 2

int main(int argc, char *argv[])
{
	int option_index;
	char *short_options = "lLD:qt:c:f:r:d:s:MNF:A:X:B:vIPC";
	static struct option long_options[] = {
		{"help", 0, 0, OPT_HELP},
		{"version", 0, 0, OPT_VERSION},
		{"list-devices", 0, 0, 'l'},
		{"list-pcms", 0, 0, 'L'},
		{"device", 1, 0, 'D'},
		{"quiet", 0, 0, 'q'},
		{"file-type", 1, 0, 't'},
		{"channels", 1, 0, 'c'},
		{"format", 1, 0, 'f'},
		{"rate", 1, 0, 'r'},
		{"duration", 1, 0 ,'d'},
		{"sleep-min", 0, 0, 's'},
		{"mmap", 0, 0, 'M'},
		{"nonblock", 0, 0, 'N'},
		{"period_time", 1, 0, 'F'},
		{"avail-min", 1, 0, 'A'},
		{"xfer-min", 1, 0, 'X'},
		{"buffer-time", 1, 0, 'B'},
		{"verbose", 0, 0, 'v'},
		{"iec958c", 0, 0, 'C'},
		{"iec958p", 0, 0, 'P'},
		{"separate-channels", 0, 0, 'I'},
		{0, 0, 0, 0}
	};
	char *pcm_name = "plug:0,0";
	int tmp, err, c;
	snd_pcm_info_t *info;

	err = snd_output_stdio_attach(&log, stderr, 0);
	assert(err >= 0);

	command = argv[0];
	file_type = FORMAT_DEFAULT;
	if (strstr(argv[0], "arecord")) {
		stream = SND_PCM_STREAM_CAPTURE;
		file_type = FORMAT_WAVE;
		command = "arecord";
	} else if (strstr(argv[0], "aplay")) {
		stream = SND_PCM_STREAM_PLAYBACK;
		command = "aplay";
	} else {
		error("command should be named either arecord or aplay");
		return 1;
	}

	chunk_size = -1;
	rhwparams.format = SND_PCM_FORMAT_U8;
	rhwparams.rate = DEFAULT_SPEED;
	rhwparams.channels = 1;
	memset(&spdif, 0, sizeof(spdif));

	while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (c) {
		case OPT_HELP:
			usage(command);
			return 0;
		case OPT_VERSION:
			version();
			return 0;
		case 'l':
			device_list();
			return 0;
		case 'L':
			pcm_list();
			return 0;
		case 'D':
			pcm_name = optarg;
			break;
		case 'q':
			quiet_mode = 1;
			break;
		case 't':
			if (strcasecmp(optarg, "raw") == 0)
				file_type = FORMAT_RAW;
			else if (strcasecmp(optarg, "voc") == 0)
				file_type = FORMAT_VOC;
			else if (strcasecmp(optarg, "wav") == 0)
				file_type = FORMAT_WAVE;
			else {
				error("unrecognized file format %s", optarg);
				return 1;
			}
			break;
		case 'c':
			rhwparams.channels = atoi(optarg);
			if (rhwparams.channels < 1 || rhwparams.channels > 32) {
				error("value %i for channels is invalid", rhwparams.channels);
				return 1;
			}
			break;
		case 'f':
			if (strcasecmp(optarg, "cd") == 0) {
				rhwparams.format = SND_PCM_FORMAT_S16_LE;
				rhwparams.rate = 44100;
				rhwparams.channels = 2;
			} else if (strcasecmp(optarg, "dat") == 0) {
				rhwparams.format = SND_PCM_FORMAT_S16_LE;
				rhwparams.rate = 48000;
				rhwparams.channels = 2;
			} else {
				rhwparams.format = snd_pcm_format_value(optarg);
				if (rhwparams.format == SND_PCM_FORMAT_UNKNOWN) {
					error("wrong extended format '%s'", optarg);
					exit(EXIT_FAILURE);
				}
			}
			break;
		case 'r':
			tmp = atoi(optarg);
			if (tmp < 300)
				tmp *= 1000;
			rhwparams.rate = tmp;
			if (tmp < 2000 || tmp > 128000) {
				error("bad speed value %i", tmp);
				return 1;
			}
			break;
		case 'd':
			timelimit = atoi(optarg);
			break;
		case 's':
			sleep_min = atoi(optarg);
			break;
		case 'N':
			nonblock = 1;
			open_mode |= SND_PCM_NONBLOCK;
			break;
		case 'F':
			period_time = atoi(optarg);
			break;
		case 'B':
			buffer_time = atoi(optarg);
			break;
		case 'A':
			avail_min = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'M':
			mmap_flag = 1;
			break;
		case 'I':
			interleaved = 0;
			break;
		case 'C':
			digtype = SND_CONTROL_TYPE_IEC958;
			spdif.status[0] = IEC958_AES0_NONAUDIO |
					     IEC958_AES0_CON_EMPHASIS_NONE;
			spdif.status[1] = IEC958_AES1_CON_ORIGINAL |
					     IEC958_AES1_CON_PCM_CODER;
			spdif.status[2] = 0;
			spdif.status[3] = IEC958_AES3_CON_FS_48000;
			break;
		case 'P':
			digtype = SND_CONTROL_TYPE_IEC958;
			spdif.status[0] = IEC958_AES0_PROFESSIONAL |
					     IEC958_AES0_NONAUDIO |
					     IEC958_AES0_PRO_EMPHASIS_NONE |
					     IEC958_AES0_PRO_FS_48000;
			spdif.status[1] = IEC958_AES1_PRO_MODE_NOTID |
					     IEC958_AES1_PRO_USERBITS_NOTID;
			spdif.status[2] = IEC958_AES2_PRO_WORDLEN_NOTID;
			spdif.status[3] = 0;
			break;
		default:
			fprintf(stderr, "Try `%s --help' for more information.\n", command);
			return 1;
		}
	}

	err = snd_pcm_open(&handle, pcm_name, stream, open_mode);
	if (err < 0) {
		error("audio open error: %s", snd_strerror(err));
		return 1;
	}

	snd_pcm_info_alloca(&info);
	if ((err = snd_pcm_info(handle, info)) < 0) {
		error("info error: %s", snd_strerror(err));
		return 1;
	}

	if (digtype != SND_CONTROL_TYPE_NONE) {
		snd_control_t *ctl;
		snd_ctl_t *ctl_handle;
		char ctl_name[12];
		int ctl_card;
		snd_control_alloca(&ctl);
		snd_control_set_interface(ctl, SND_CONTROL_IFACE_PCM);
		snd_control_set_device(ctl, snd_pcm_info_get_device(info));
		snd_control_set_subdevice(ctl, snd_pcm_info_get_subdevice(info));
		snd_control_set_name(ctl, "IEC958 (S/PDIF) Stream");
		snd_control_set_iec958(ctl, &spdif);
		ctl_card = snd_pcm_info_get_card(info);
		if (ctl_card < 0) {
			error("Unable to setup the IEC958 (S/PDIF) interface - PCM has no assigned card");
			goto __diga_end;
		}
		sprintf(ctl_name, "hw:%d", ctl_card);
		if ((err = snd_ctl_open(&ctl_handle, ctl_name)) < 0) {
			error("Unable to open the control interface '%s': %s", ctl_name, snd_strerror(err));
			goto __diga_end;
		}
		if ((err = snd_ctl_cwrite(ctl_handle, ctl)) < 0) {
			error("Unable to update the IEC958 control: %s", snd_strerror(err));
			goto __diga_end;
		}
		snd_ctl_close(ctl_handle);
	      __diga_end:
	}

	if (nonblock) {
		err = snd_pcm_nonblock(handle, 1);
		if (err < 0) {
			error("nonblock setting error: %s", snd_strerror(err));
			return 1;
		}
	}

	chunk_size = 1024;
	hwparams = rhwparams;

	audiobuf = (char *)malloc(1024);
	if (audiobuf == NULL) {
		error("not enough memory");
		return 1;
	}

	if (mmap_flag) {
		writei_func = snd_pcm_mmap_writei;
		readi_func = snd_pcm_mmap_readi;
		writen_func = snd_pcm_mmap_writen;
		readn_func = snd_pcm_mmap_readn;
	} else {
		writei_func = snd_pcm_writei;
		readi_func = snd_pcm_readi;
		writen_func = snd_pcm_writen;
		readn_func = snd_pcm_readn;
	}

	if (interleaved) {
		if (optind > argc - 1) {
			if (stream == SND_PCM_STREAM_PLAYBACK)
				playback(NULL);
			else
				capture(NULL);
		} else {
			while (optind <= argc - 1) {
				if (stream == SND_PCM_STREAM_PLAYBACK)
					playback(argv[optind++]);
				else
					capture(argv[optind++]);
			}
		}
	} else {
		if (stream == SND_PCM_STREAM_PLAYBACK)
			playbackv(&argv[optind], argc - optind);
		else
			capturev(&argv[optind], argc - optind);
	}
	snd_pcm_close(handle);
	free(audiobuf);
	snd_output_close(log);
	return EXIT_SUCCESS;
}

/*
 * Safe read (for pipes)
 */
 
ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t result = 0, res;

	while (count > 0) {
		if ((res = read(fd, buf, count)) == 0)
			break;
		if (res < 0)
			return result > 0 ? result : res;
		count -= res;
		result += res;
		(char *)buf += res;
	}
	return result;
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
 * helper for test_wavefile
 */

size_t test_wavefile_read(int fd, char *buffer, size_t *size, size_t reqsize, int line)
{
	if (*size >= reqsize)
		return *size;
	if (safe_read(fd, buffer + *size, reqsize - *size) != reqsize - *size) {
		error("read error (called from line %i)", line);
		exit(EXIT_FAILURE);
	}
	return *size = reqsize;
}


/*
 * test, if it's a .WAV file, > 0 if ok (and set the speed, stereo etc.)
 *                            == 0 if not
 * Value returned is bytes to be discarded.
 */
static ssize_t test_wavefile(int fd, char *buffer, size_t size)
{
	WaveHeader *h = (WaveHeader *)buffer;
	WaveFmtBody *f;
	WaveChunkHeader *c;
	u_int type, len;

	if (size < sizeof(WaveHeader))
		return -1;
	if (h->magic != WAV_RIFF || h->type != WAV_WAVE)
		return -1;
	if (size > sizeof(WaveHeader))
		memmove(buffer, buffer + sizeof(WaveHeader), size - sizeof(WaveHeader));
	size -= sizeof(WaveHeader);
	while (1) {
		test_wavefile_read(fd, buffer, &size, sizeof(WaveChunkHeader), __LINE__);
		c = (WaveChunkHeader*)buffer;
		type = c->type;
		len = LE_INT(c->length);
		if (size > sizeof(WaveChunkHeader))
			memmove(buffer, buffer + sizeof(WaveChunkHeader), size - sizeof(WaveChunkHeader));
		size -= sizeof(WaveChunkHeader);
		if (type == WAV_FMT)
			break;
		test_wavefile_read(fd, buffer, &size, len, __LINE__);
		if (size > len)
			memmove(buffer, buffer + len, size - len);
		size -= len;
	}

	if (len < sizeof(WaveFmtBody)) {
		error("unknown length of 'fmt ' chunk (read %u, should be %u at least)", len, (u_int)sizeof(WaveFmtBody));
		exit(EXIT_FAILURE);
	}
	test_wavefile_read(fd, buffer, &size, len, __LINE__);
	f = (WaveFmtBody*) buffer;
	if (LE_SHORT(f->format) != WAV_PCM_CODE) {
		error("can't play not PCM-coded WAVE-files");
		exit(EXIT_FAILURE);
	}
	if (LE_SHORT(f->modus) < 1) {
		error("can't play WAVE-files with %d tracks", LE_SHORT(f->modus));
		exit(EXIT_FAILURE);
	}
	hwparams.channels = LE_SHORT(f->modus);
	switch (LE_SHORT(f->bit_p_spl)) {
	case 8:
		hwparams.format = SND_PCM_FORMAT_U8;
		break;
	case 16:
		hwparams.format = SND_PCM_FORMAT_S16_LE;
		break;
	default:
		error(" can't play WAVE-files with sample %d bits wide", LE_SHORT(f->bit_p_spl));
		exit(EXIT_FAILURE);
	}
	hwparams.rate = LE_INT(f->sample_fq);
	
	if (size > len)
		memmove(buffer, buffer + len, size - len);
	size -= len;
	
	while (1) {
		u_int type, len;

		test_wavefile_read(fd, buffer, &size, sizeof(WaveChunkHeader), __LINE__);
		c = (WaveChunkHeader*)buffer;
		type = c->type;
		len = LE_INT(c->length);
		if (size > sizeof(WaveChunkHeader))
			memmove(buffer, buffer + sizeof(WaveChunkHeader), size - sizeof(WaveChunkHeader));
		size -= sizeof(WaveChunkHeader);
		if (type == WAV_DATA) {
			if (len < count)
				count = len;
			return size;
		}
		test_wavefile_read(fd, buffer, &size, len, __LINE__);
		if (size > len)
			memmove(buffer, buffer + len, size - len);
		size -= len;
	}

	/* shouldn't be reached */
	return -1;
}

/*

 */

static int test_au(int fd, void *buffer)
{
	AuHeader *ap = buffer;

	if (ap->magic != AU_MAGIC)
		return -1;
	if (BE_INT(ap->hdr_size) > 128 || BE_INT(ap->hdr_size) < 24)
		return -1;
	count = BE_INT(ap->data_size);
	switch (BE_INT(ap->encoding)) {
	case AU_FMT_ULAW:
		hwparams.format = SND_PCM_FORMAT_MU_LAW;
		break;
	case AU_FMT_LIN8:
		hwparams.format = SND_PCM_FORMAT_U8;
		break;
	case AU_FMT_LIN16:
		hwparams.format = SND_PCM_FORMAT_U16_LE;
		break;
	default:
		return -1;
	}
	hwparams.rate = BE_INT(ap->sample_rate);
	if (hwparams.rate < 2000 || hwparams.rate > 256000)
		return -1;
	hwparams.channels = BE_INT(ap->channels);
	if (hwparams.channels < 1 || hwparams.channels > 128)
		return -1;
	if (safe_read(fd, buffer + sizeof(AuHeader), BE_INT(ap->hdr_size) - sizeof(AuHeader)) != BE_INT(ap->hdr_size) - sizeof(AuHeader)) {
		error("read error");
		exit(EXIT_FAILURE);
	}
	return 0;
}

static void set_params(void)
{
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *swparams;
	size_t buffer_size;
	int err;
	size_t n;
	size_t xfer_align;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_sw_params_alloca(&swparams);
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		error("Broken configuration for this PCM: no configurations available");
		exit(EXIT_FAILURE);
	}
	if (mmap_flag) {
		snd_pcm_access_mask_t *mask = alloca(snd_pcm_access_mask_sizeof());
		snd_pcm_access_mask_none(mask);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
		err = snd_pcm_hw_params_set_access_mask(handle, params, mask);
	} else if (interleaved)
		err = snd_pcm_hw_params_set_access(handle, params,
						   SND_PCM_ACCESS_RW_INTERLEAVED);
	else
		err = snd_pcm_hw_params_set_access(handle, params,
						   SND_PCM_ACCESS_RW_NONINTERLEAVED);
	if (err < 0) {
		error("Access type not available");
		exit(EXIT_FAILURE);
	}
	err = snd_pcm_hw_params_set_format(handle, params, hwparams.format);
	if (err < 0) {
		error("Sample format non available");
		exit(EXIT_FAILURE);
	}
	err = snd_pcm_hw_params_set_channels(handle, params, hwparams.channels);
	if (err < 0) {
		error("Channels count non available");
		exit(EXIT_FAILURE);
	}

#if 0
	err = snd_pcm_hw_params_set_periods_min(handle, params, 2);
	assert(err >= 0);
#endif
	err = snd_pcm_hw_params_set_rate_near(handle, params, hwparams.rate, 0);
	assert(err >= 0);
	if (buffer_time < 0)
		buffer_time = 500000;
	buffer_time = snd_pcm_hw_params_set_buffer_time_near(handle, params,
							     buffer_time, 0);
	assert(buffer_time >= 0);
	if (period_time < 0)
		period_time = buffer_time / 4;
	period_time = snd_pcm_hw_params_set_period_time_near(handle, params,
							     period_time, 0);
	assert(period_time >= 0);
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		fprintf(stderr, "Unable to install hw params:\n");
		snd_pcm_hw_params_dump(params, log);
		exit(EXIT_FAILURE);
	}
	chunk_size = snd_pcm_hw_params_get_period_size(params, 0);
	buffer_size = snd_pcm_hw_params_get_buffer_size(params);
	snd_pcm_sw_params_current(handle, swparams);
	xfer_align = snd_pcm_sw_params_get_xfer_align(swparams);
	if (sleep_min)
		xfer_align = 1;
	err = snd_pcm_sw_params_set_sleep_min(handle, swparams,
					      sleep_min);
	assert(err >= 0);
	if (avail_min < 0)
		n = chunk_size;
	else
		n = snd_pcm_hw_params_get_rate(params, 0) * (double) avail_min / 1000000;
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);
				   
	err = snd_pcm_sw_params_set_xfer_align(handle, swparams, xfer_align);
	assert(err >= 0);
	if (snd_pcm_sw_params(handle, swparams) < 0) {
		error("unable to install sw params:");
		snd_pcm_sw_params_dump(swparams, log);
		exit(EXIT_FAILURE);
	}

	if (verbose)
		snd_pcm_dump(handle, log);

	bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
	bits_per_frame = bits_per_sample * hwparams.channels;
	chunk_bytes = chunk_size * bits_per_frame / 8;
	audiobuf = realloc(audiobuf, chunk_bytes);
	if (audiobuf == NULL) {
		error("not enough memory");
		exit(EXIT_FAILURE);
	}
	// fprintf(stderr, "real chunk_size = %i, frags = %i, total = %i\n", chunk_size, setup.buf.block.frags, setup.buf.block.frags * chunk_size);
}

/* playback write error hander */

void xrun(void)
{
	snd_pcm_status_t *status;
	int res;
	
	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(handle, status))<0) {
		error("status error: %s", snd_strerror(res));
		exit(EXIT_FAILURE);
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		struct timeval now, diff, tstamp;
		gettimeofday(&now, 0);
		snd_pcm_status_get_trigger_tstamp(status, &tstamp);
		timersub(&now, &tstamp, &diff);
		fprintf(stderr, "xrun!!! (at least %.3f ms long)\n", diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
		if (verbose) {
			fprintf(stderr, "Status:\n");
			snd_pcm_status_dump(status, log);
		}
		if ((res = snd_pcm_prepare(handle))<0) {
			error("xrun: prepare error: %s", snd_strerror(res));
			exit(EXIT_FAILURE);
		}
		return;		/* ok, data should be accepted again */
	}
	error("read/write error");
	exit(EXIT_FAILURE);
}

/*
 *  write function
 */

static ssize_t pcm_write(u_char *data, size_t count)
{
	ssize_t r;
	ssize_t result = 0;

	if (sleep_min == 0 &&
	    count < chunk_size) {
		snd_pcm_format_set_silence(hwparams.format, data + count * bits_per_frame / 8, (chunk_size - count) * hwparams.channels);
		count = chunk_size;
	}
	while (count > 0) {
		r = writei_func(handle, data, count);
		if (r == -EAGAIN || (r >= 0 && r < count)) {
			snd_pcm_wait(handle, 1000);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r < 0) {
			error("write error: %s", snd_strerror(r));
			exit(EXIT_FAILURE);
		}
		if (r > 0) {
			result += r;
			count -= r;
			data += r * bits_per_frame / 8;
		}
	}
	return result;
}

static ssize_t pcm_writev(u_char **data, unsigned int channels, size_t count)
{
	ssize_t r;
	size_t result = 0;

	if (sleep_min == 0 &&
	    count != chunk_size) {
		unsigned int channel;
		size_t offset = count;
		size_t remaining = chunk_size - count;
		for (channel = 0; channel < channels; channel++)
			snd_pcm_format_set_silence(hwparams.format, data[channel] + offset * bits_per_sample / 8, remaining);
		count = chunk_size;
	}
	while (count > 0) {
		unsigned int channel;
		void *bufs[channels];
		size_t offset = result;
		for (channel = 0; channel < channels; channel++)
			bufs[channel] = data[channel] + offset * bits_per_sample / 8;
		r = writen_func(handle, bufs, count);
		if (r == -EAGAIN || (r >= 0 && r < count)) {
			snd_pcm_wait(handle, 1000);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r < 0) {
			error("writev error: %s", snd_strerror(r));
			exit(EXIT_FAILURE);
		}
		if (r > 0) {
			result += r;
			count -= r;
		}
	}
	return result;
}

/*
 *  read function
 */

static ssize_t pcm_read(u_char *data, size_t rcount)
{
	ssize_t r;
	size_t result = 0;
	size_t count = rcount;

	if (sleep_min == 0 &&
	    count != chunk_size) {
		count = chunk_size;
	}

	while (count > 0) {
		r = readi_func(handle, data, count);
		if (r == -EAGAIN || (r >= 0 && r < count)) {
			snd_pcm_wait(handle, 1000);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r < 0) {
			error("read error: %s", snd_strerror(r));
			exit(EXIT_FAILURE);
		}
		if (r > 0) {
			result += r;
			count -= r;
			data += r * bits_per_frame / 8;
		}
	}
	return rcount;
}

static ssize_t pcm_readv(u_char **data, unsigned int channels, size_t rcount)
{
	ssize_t r;
	size_t result = 0;
	size_t count = rcount;

	if (sleep_min == 0 &&
	    count != chunk_size) {
		count = chunk_size;
	}

	while (count > 0) {
		unsigned int channel;
		void *bufs[channels];
		size_t offset = result;
		for (channel = 0; channel < channels; channel++)
			bufs[channel] = data[channel] + offset * bits_per_sample / 8;
		r = readn_func(handle, bufs, count);
		if (r == -EAGAIN || (r >= 0 && r < count)) {
			snd_pcm_wait(handle, 1000);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r < 0) {
			error("readv error: %s", snd_strerror(r));
			exit(EXIT_FAILURE);
		}
		if (r > 0) {
			result += r;
			count -= r;
		}
	}
	return rcount;
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
		if (size > chunk_bytes - buffer_pos)
			size = chunk_bytes - buffer_pos;
		memcpy(audiobuf + buffer_pos, data, size);
		data += size;
		count -= size;
		buffer_pos += size;
		if (buffer_pos == chunk_bytes) {
			if ((r = pcm_write(audiobuf, chunk_size)) != chunk_size)
				return r;
			buffer_pos = 0;
		}
	}
	return result;
}

static void voc_write_silence(unsigned x)
{
	unsigned l;
	char *buf;

	buf = (char *) malloc(chunk_bytes);
	if (buf == NULL) {
		error("can't allocate buffer for silence");
		return;		/* not fatal error */
	}
	snd_pcm_format_set_silence(hwparams.format, buf, chunk_size * hwparams.channels);
	while (x > 0) {
		l = x;
		if (l > chunk_size)
			l = chunk_size;
		if (voc_pcm_write(buf, l) != l) {
			error("write error");
			exit(EXIT_FAILURE);
		}
		x -= l;
	}
}

static void voc_pcm_flush(void)
{
	if (buffer_pos > 0) {
		size_t b;
		if (sleep_min == 0) {
			if (snd_pcm_format_set_silence(hwparams.format, audiobuf + buffer_pos, chunk_bytes - buffer_pos * 8 / bits_per_sample) < 0)
				fprintf(stderr, "voc_pcm_flush - silence error");
			b = chunk_size;
		} else {
			b = buffer_pos * 8 / bits_per_frame;
		}
		if (pcm_write(audiobuf, b) != b)
			error("voc_pcm_flush error");
	}
	snd_pcm_drain(handle);
}

static void voc_play(int fd, int ofs, char *name)
{
	int l;
	VocBlockType *bp;
	VocVoiceData *vd;
	VocExtBlock *eb;
	size_t nextblock, in_buffer;
	u_char *data, *buf;
	char was_extended = 0, output = 0;
	u_short *sp, repeat = 0;
	size_t silence;
	int filepos = 0;

#define COUNT(x)	nextblock -= x; in_buffer -= x; data += x
#define COUNT1(x)	in_buffer -= x; data += x

	data = buf = (u_char *)malloc(64 * 1024);
	buffer_pos = 0;
	if (data == NULL) {
		error("malloc error");
		exit(EXIT_FAILURE);
	}
	if (!quiet_mode) {
		fprintf(stderr, "Playing Creative Labs Channel file '%s'...\n", name);
	}
	/* first we waste the rest of header, ugly but we don't need seek */
	while (ofs > chunk_bytes) {
		if (safe_read(fd, buf, chunk_bytes) != chunk_bytes) {
			error("read error");
			exit(EXIT_FAILURE);
		}
		ofs -= chunk_bytes;
	}
	if (ofs) {
		if (safe_read(fd, buf, ofs) != ofs) {
			error("read error");
			exit(EXIT_FAILURE);
		}
	}
	hwparams.format = SND_PCM_FORMAT_U8;
	hwparams.channels = 1;
	hwparams.rate = DEFAULT_SPEED;
	set_params();

	in_buffer = nextblock = 0;
	while (1) {
	      Fill_the_buffer:	/* need this for repeat */
		if (in_buffer < 32) {
			/* move the rest of buffer to pos 0 and fill the buf up */
			if (in_buffer)
				memcpy(buf, data, in_buffer);
			data = buf;
			if ((l = safe_read(fd, buf + in_buffer, chunk_bytes - in_buffer)) > 0)
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
					hwparams.rate = (int) (vd->tc);
					hwparams.rate = 1000000 / (256 - hwparams.rate);
#if 0
					d_printf("Channel data %d Hz\n", dsp_speed);
#endif
					if (vd->pack) {		/* /dev/dsp can't it */
						error("can't play packed .voc files");
						return;
					}
					if (hwparams.channels == 2)		/* if we are in Stereo-Mode, switch back */
						hwparams.channels = 1;
				} else {	/* there was extended block */
					hwparams.channels = 2;
					was_extended = 0;
				}
				set_params();
				break;
			case 2:	/* nothing to do, pure data */
#if 0
				d_printf("Channel continuation\n");
#endif
				break;
			case 3:	/* a silence block, no data, only a count */
				sp = (u_short *) data;
				COUNT1(sizeof(u_short));
				hwparams.rate = (int) (*data);
				COUNT1(1);
				hwparams.rate = 1000000 / (256 - hwparams.rate);
				set_params();
				silence = (((size_t) * sp) * 1000) / hwparams.rate;
#if 0
				d_printf("Silence for %d ms\n", (int) silence);
#endif
				voc_write_silence(*sp);
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
						error("can't play loops; %s isn't seekable\n", name);
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
				hwparams.rate = (int) (eb->tc);
				hwparams.rate = 256000000L / (65536 - hwparams.rate);
				hwparams.channels = eb->mode == VOC_MODE_STEREO ? 2 : 1;
				if (hwparams.channels == 2)
					hwparams.rate = hwparams.rate >> 1;
				if (eb->pack) {		/* /dev/dsp can't it */
					error("can't play packed .voc files");
					return;
				}
#if 0
				d_printf("Extended block %s %d Hz\n",
					 (eb->mode ? "Stereo" : "Mono"), dsp_speed);
#endif
				break;
			default:
				error("unknown blocktype %d. terminate.", bp->type);
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
					error("write error");
					exit(EXIT_FAILURE);
				}
			} else {
				if (voc_pcm_write(data, l) != l) {
					error("write error");
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
	hwparams = rhwparams;
}

/* calculate the data count to read from/to dsp */
static size_t calc_count(void)
{
	size_t count;

	if (!timelimit) {
		count = 0x7fffffff;
	} else {
		count = snd_pcm_format_size(hwparams.format,
					    timelimit * hwparams.rate *
					    hwparams.channels);
	}
	return count;
}

/* write a .VOC-header */
static void begin_voc(int fd, size_t cnt)
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
		error("write error");
		exit(EXIT_FAILURE);
	}
	if (hwparams.channels > 1) {
		/* write a extended block */
		bt.type = 8;
		bt.datalen = 4;
		bt.datalen_m = bt.datalen_h = 0;
		if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
			error("write error");
			exit(EXIT_FAILURE);
		}
		eb.tc = (u_short) (65536 - 256000000L / (hwparams.rate << 1));
		eb.pack = 0;
		eb.mode = 1;
		if (write(fd, &eb, sizeof(VocExtBlock)) != sizeof(VocExtBlock)) {
			error("write error");
			exit(EXIT_FAILURE);
		}
	}
	bt.type = 1;
	cnt += sizeof(VocVoiceData);	/* Channel_data block follows */
	bt.datalen = (u_char) (cnt & 0xFF);
	bt.datalen_m = (u_char) ((cnt & 0xFF00) >> 8);
	bt.datalen_h = (u_char) ((cnt & 0xFF0000) >> 16);
	if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
		error("write error");
		exit(EXIT_FAILURE);
	}
	vd.tc = (u_char) (256 - (1000000 / hwparams.rate));
	vd.pack = 0;
	if (write(fd, &vd, sizeof(VocVoiceData)) != sizeof(VocVoiceData)) {
		error("write error");
		exit(EXIT_FAILURE);
	}
}

/* write a WAVE-header */
static void begin_wave(int fd, size_t cnt)
{
	WaveHeader h;
	WaveFmtBody f;
	WaveChunkHeader cf, cd;
	int bits;
	u_int tmp;
	u_short tmp2;

	bits = 8;
	switch ((unsigned long) hwparams.format) {
	case SND_PCM_FORMAT_U8:
		bits = 8;
		break;
	case SND_PCM_FORMAT_S16_LE:
		bits = 16;
		break;
	default:
		error("Wave doesn't support %s format...", snd_pcm_format_name(hwparams.format));
		exit(EXIT_FAILURE);
	}
	h.magic = WAV_RIFF;
	tmp = cnt + sizeof(WaveHeader) + sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + sizeof(WaveChunkHeader) - 8;
	h.length = LE_INT(tmp);
	h.type = WAV_WAVE;

	cf.type = WAV_FMT;
	cf.length = LE_INT(16);

	f.format = LE_INT(WAV_PCM_CODE);
	f.modus = LE_SHORT(hwparams.channels);
	f.sample_fq = LE_INT(hwparams.rate);
#if 0
	tmp2 = (samplesize == 8) ? 1 : 2;
	f.byte_p_spl = LE_SHORT(tmp2);
	tmp2 = dsp_speed * hwparams.channels * tmp2;
	f.byte_p_sec = LE_SHORT(tmp2);
#else
	tmp2 = hwparams.channels * ((bits + 7) / 8);
	f.byte_p_spl = LE_SHORT(tmp2);
	tmp2 = tmp2 * hwparams.rate;
	f.byte_p_sec = LE_SHORT(tmp2);
#endif
	f.bit_p_spl = LE_SHORT(bits);

	cd.type = WAV_DATA;
	cd.length = LE_INT(cnt);

	if (write(fd, &h, sizeof(WaveHeader)) != sizeof(WaveHeader) ||
	    write(fd, &cf, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader) ||
	    write(fd, &f, sizeof(WaveFmtBody)) != sizeof(WaveFmtBody) ||
	    write(fd, &cd, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader)) {
		error("write error");
		exit(EXIT_FAILURE);
	}
}

/* write a Au-header */
static void begin_au(int fd, size_t cnt)
{
	AuHeader ah;

	ah.magic = AU_MAGIC;
	ah.hdr_size = BE_INT(24);
	ah.data_size = BE_INT(cnt);
	switch ((unsigned long) hwparams.format) {
	case SND_PCM_FORMAT_MU_LAW:
		ah.encoding = BE_INT(AU_FMT_ULAW);
		break;
	case SND_PCM_FORMAT_U8:
		ah.encoding = BE_INT(AU_FMT_LIN8);
		break;
	case SND_PCM_FORMAT_S16_LE:
		ah.encoding = BE_INT(AU_FMT_LIN16);
		break;
	default:
		error("Sparc Audio doesn't support %s format...", snd_pcm_format_name(hwparams.format));
		exit(EXIT_FAILURE);
	}
	ah.sample_rate = BE_INT(hwparams.rate);
	ah.channels = BE_INT(hwparams.channels);
	if (write(fd, &ah, sizeof(AuHeader)) != sizeof(AuHeader)) {
		error("write error");
		exit(EXIT_FAILURE);
	}
}

/* closing .VOC */
static void end_voc(int fd)
{
	char dummy = 0;		/* Write a Terminator */
	if (write(fd, &dummy, 1) != 1) {
		error("write error");
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
			(stream == SND_PCM_STREAM_PLAYBACK) ? "Playing" : "Recording",
			fmt_rec_table[rtype].what,
			name);
		fprintf(stderr, "%s, ", snd_pcm_format_description(hwparams.format));
		fprintf(stderr, "Rate %d Hz, ", hwparams.rate);
		if (hwparams.channels == 1)
			fprintf(stderr, "Mono");
		else if (hwparams.channels == 2)
			fprintf(stderr, "Stereo");
		else
			fprintf(stderr, "Channels %i", hwparams.channels);
		fprintf(stderr, "\n");
	}
}

/* playing raw data */

void playback_go(int fd, size_t loaded, size_t count, int rtype, char *name)
{
	int l, r;
	size_t written = 0;
	size_t c;

	header(rtype, name);
	set_params();

	while (loaded > chunk_bytes && written < count) {
		if (pcm_write(audiobuf + written, chunk_size) <= 0)
			return;
		written += chunk_bytes;
		loaded -= chunk_bytes;
	}
	if (written > 0 && loaded > 0)
		memmove(audiobuf, audiobuf + written, loaded);

	l = loaded;
	while (written < count) {
		do {
			c = count - written;
			if (c > chunk_bytes)
				c = chunk_bytes;
			c -= l;

			if (c == 0)
				break;
			r = safe_read(fd, audiobuf + l, c);
			if (r < 0) {
				perror(name);
				exit(EXIT_FAILURE);
			}
			if (r == 0)
				break;
			l += r;
		} while (sleep_min == 0 && l < chunk_bytes);
		l = l * 8 / bits_per_frame;
		r = pcm_write(audiobuf, l);
		if (r != l)
			break;
		r = r * bits_per_frame / 8;
		written += r;
		l = 0;
	}
	snd_pcm_drain(handle);
}

/* captureing raw data, this proc handels WAVE files and .VOCs (as one block) */

void capture_go(int fd, size_t count, int rtype, char *name)
{
	size_t c;
	ssize_t r;

	header(rtype, name);
	set_params();

	while (count > 0) {
		c = count;
		if (c > chunk_bytes)
			c = chunk_bytes;
		c = c * 8 / bits_per_frame;
		if ((r = pcm_read(audiobuf, c)) != c)
			break;
		r = r * bits_per_frame / 8;
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
	size_t dta;
	ssize_t dtawave;

	count = calc_count();
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
	dta = sizeof(AuHeader);
	if (safe_read(fd, audiobuf, dta) != dta) {
		error("read error");
		exit(EXIT_FAILURE);
	}
	if (test_au(fd, audiobuf) >= 0) {
		rhwparams.format = SND_PCM_FORMAT_MU_LAW;
		playback_go(fd, 0, count, FORMAT_AU, name);
		goto __end;
	}
	dta = sizeof(VocHeader);
	if (safe_read(fd, audiobuf + sizeof(AuHeader),
		 dta - sizeof(AuHeader)) != dta - sizeof(AuHeader)) {
		error("read error");
		exit(EXIT_FAILURE);
	}
	if ((ofs = test_vocfile(audiobuf)) >= 0) {
		voc_play(fd, ofs, name);
		goto __end;
	}
	/* read bytes for WAVE-header */
	if ((dtawave = test_wavefile(fd, audiobuf, dta)) >= 0) {
		playback_go(fd, dtawave, count, FORMAT_WAVE, name);
	} else {
		/* should be raw data */
		init_raw_data();
		playback_go(fd, dta, count, FORMAT_RAW, name);
	}
      __end:
	if (fd != 0)
		close(fd);
}

static void capture(char *name)
{
	int fd;

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
	count = calc_count();
	count += count % 2;
	/* WAVE-file should be even (I'm not sure), but wasting one byte
	   isn't a problem (this can only be in 8 bit mono) */
	if (fmt_rec_table[file_type].start)
		fmt_rec_table[file_type].start(fd, count);
	capture_go(fd, count, file_type, name);
	fmt_rec_table[file_type].end(fd);
}

void playbackv_go(int* fds, unsigned int channels, size_t loaded, size_t count, int rtype, char **names)
{
	int r;
	size_t vsize;
	unsigned int channel;
	u_char *bufs[channels];

	header(rtype, names[0]);
	set_params();

	vsize = chunk_bytes / channels;

	// Not yet implemented
	assert(loaded == 0);

	for (channel = 0; channel < channels; ++channel)
		bufs[channel] = audiobuf + vsize * channel;

	while (count > 0) {
		size_t c = 0;
		size_t expected = count / channels;
		if (expected > vsize)
			expected = vsize;
		do {
			r = safe_read(fds[0], bufs[0], expected);
			if (r < 0) {
				perror(names[channel]);
				exit(EXIT_FAILURE);
			}
			for (channel = 1; channel < channels; ++channel) {
				if (safe_read(fds[channel], bufs[channel], r) != r) {
					perror(names[channel]);
					exit(EXIT_FAILURE);
				}
			}
			if (r == 0)
				break;
			c += r;
		} while (sleep_min == 0 && c < expected);
		c = c * 8 / bits_per_sample;
		r = pcm_writev(bufs, channels, c);
		if (r != c)
			break;
		r = r * bits_per_frame / 8;
		count -= r;
	}
	snd_pcm_drain(handle);
}

void capturev_go(int* fds, unsigned int channels, size_t count, int rtype, char **names)
{
	size_t c;
	ssize_t r;
	unsigned int channel;
	size_t vsize;
	u_char *bufs[channels];

	header(rtype, names[0]);
	set_params();

	vsize = chunk_bytes / channels;

	for (channel = 0; channel < channels; ++channel)
		bufs[channel] = audiobuf + vsize * channel;

	while (count > 0) {
		size_t rv;
		c = count;
		if (c > chunk_bytes)
			c = chunk_bytes;
		c = c * 8 / bits_per_frame;
		if ((r = pcm_readv(bufs, channels, c)) != c)
			break;
		rv = r * bits_per_sample / 8;
		for (channel = 0; channel < channels; ++channel) {
			if (write(fds[channel], bufs[channel], rv) != rv) {
				perror(names[channel]);
				exit(EXIT_FAILURE);
			}
		}
		r = r * bits_per_frame / 8;
		count -= r;
	}
}

static void playbackv(char **names, unsigned int count)
{
	int ret = 0;
	unsigned int channel;
	unsigned int channels = rhwparams.channels;
	int alloced = 0;
	int fds[channels];
	for (channel = 0; channel < channels; ++channel)
		fds[channel] = -1;

	if (count == 1 && channels > 1) {
		size_t len = strlen(names[0]);
		char format[1024];
		memcpy(format, names[0], len);
		strcpy(format + len, ".%d");
		len += 4;
		names = malloc(sizeof(*names) * channels);
		for (channel = 0; channel < channels; ++channel) {
			names[channel] = malloc(len);
			sprintf(names[channel], format, channel);
		}
		alloced = 1;
	} else if (count != channels) {
		error("You need to specify %d files", channels);
		exit(EXIT_FAILURE);
	}

	for (channel = 0; channel < channels; ++channel) {
		fds[channel] = open(names[channel], O_RDONLY, 0);
		if (fds[channel] < 0) {
			perror(names[channel]);
			ret = EXIT_FAILURE;
			goto __end;
		}
	}
	/* should be raw data */
	init_raw_data();
	count = calc_count();
	playbackv_go(fds, channels, 0, count, FORMAT_RAW, names);

      __end:
	for (channel = 0; channel < channels; ++channel) {
		if (fds[channel] >= 0)
			close(fds[channel]);
		if (alloced)
			free(names[channel]);
	}
	if (alloced)
		free(names);
	if (ret)
		exit(ret);
}

static void capturev(char **names, unsigned int count)
{
	int ret = 0;
	unsigned int channel;
	unsigned int channels = rhwparams.channels;
	int alloced = 0;
	int fds[channels];
	for (channel = 0; channel < channels; ++channel)
		fds[channel] = -1;

	if (count == 1) {
		size_t len = strlen(names[0]);
		char format[1024];
		memcpy(format, names[0], len);
		strcpy(format + len, ".%d");
		len += 4;
		names = malloc(sizeof(*names) * channels);
		for (channel = 0; channel < channels; ++channel) {
			names[channel] = malloc(len);
			sprintf(names[channel], format, channel);
		}
		alloced = 1;
	} else if (count != channels) {
		error("You need to specify %d files", channels);
		exit(EXIT_FAILURE);
	}

	for (channel = 0; channel < channels; ++channel) {
		fds[channel] = open(names[channel], O_WRONLY + O_CREAT, 0644);
		if (fds[channel] < 0) {
			perror(names[channel]);
			ret = EXIT_FAILURE;
			goto __end;
		}
	}
	/* should be raw data */
	init_raw_data();
	count = calc_count();
	capturev_go(fds, channels, count, FORMAT_RAW, names);

      __end:
	for (channel = 0; channel < channels; ++channel) {
		if (fds[channel] >= 0)
			close(fds[channel]);
		if (alloced)
			free(names[channel]);
	}
	if (alloced)
		free(names);
	if (ret)
		exit(ret);
}

