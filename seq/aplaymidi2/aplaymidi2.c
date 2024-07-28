// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aplaymidi2.c - simple player of a MIDI Clip File over ALSA sequencer
 */

#include "aconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <alsa/ump_msg.h>
#include "version.h"

static snd_seq_t *seq;
static int client;
static int port_count;
static snd_seq_addr_t ports[16];
static int queue;
static int end_delay = 2;
static int silent;
static int passall;

static unsigned int _current_tempo  = 50000000; /* default 120 bpm */
static unsigned int tempo_base = 10;
static unsigned int current_tick;

/* prints an error message to stderr */
static void errormsg(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	fputc('\n', stderr);
}

/* prints an error message to stderr, and dies */
static void fatal(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

/* memory allocation error handling */
static void check_mem(void *p)
{
	if (!p)
		fatal("Out of memory");
}

/* error handling for ALSA functions */
static void check_snd(const char *operation, int err)
{
	if (err < 0)
		fatal("Cannot %s - %s", operation, snd_strerror(err));
}

/* open and initialize the sequencer client */
static void init_seq(void)
{
	int err;

	err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	check_snd("open sequencer", err);

	err = snd_seq_set_client_name(seq, "aplaymidi2");
	check_snd("set client name", err);

	client = snd_seq_client_id(seq);
	check_snd("get client id", client);

	err = snd_seq_set_client_midi_version(seq, SND_SEQ_CLIENT_UMP_MIDI_2_0);
	check_snd("set midi version", err);
}

/* parses one or more port addresses from the string */
static void parse_ports(const char *arg)
{
	char *buf, *s, *port_name;
	int err;

	/* make a copy of the string because we're going to modify it */
	buf = strdup(arg);
	check_mem(buf);

	for (port_name = s = buf; s; port_name = s + 1) {
		/* Assume that ports are separated by commas.  We don't use
		 * spaces because those are valid in client names. */
		s = strchr(port_name, ',');
		if (s)
			*s = '\0';

		++port_count;
		if (port_count > 16)
			fatal("Too many ports specified");

		err = snd_seq_parse_address(seq, &ports[port_count - 1], port_name);
		if (err < 0)
			fatal("Invalid port %s - %s", port_name, snd_strerror(err));
	}

	free(buf);
}

/* create a source port to send from */
static void create_source_port(void)
{
	snd_seq_port_info_t *pinfo;
	int err;

	snd_seq_port_info_alloca(&pinfo);

	/* the first created port is 0 anyway, but let's make sure ... */
	snd_seq_port_info_set_port(pinfo, 0);
	snd_seq_port_info_set_port_specified(pinfo, 1);

	snd_seq_port_info_set_name(pinfo, "aplaymidi2");

	snd_seq_port_info_set_capability(pinfo, 0); /* sic */
	snd_seq_port_info_set_type(pinfo,
				   SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				   SND_SEQ_PORT_TYPE_APPLICATION);

	err = snd_seq_create_port(seq, pinfo);
	check_snd("create port", err);
}

/* create a queue */
static void create_queue(void)
{
	if (!snd_seq_has_queue_tempo_base(seq))
		tempo_base = 1000;

	queue = snd_seq_alloc_named_queue(seq, "aplaymidi2");
	check_snd("create queue", queue);
}

/* connect to destination ports */
static void connect_ports(void)
{
	int i, err;

	for (i = 0; i < port_count; ++i) {
		err = snd_seq_connect_to(seq, 0, ports[i].client, ports[i].port);
		if (err < 0)
			fatal("Cannot connect to port %d:%d - %s",
			      ports[i].client, ports[i].port, snd_strerror(err));
	}
}

/* read 32bit word and convert to native endian:
 * return 0 on success, -1 on error
 */
static int read_word(FILE *file, uint32_t *dest)
{
	uint32_t v;

	if (fread(&v, 4, 1, file) != 1)
		return -1;
	*dest = be32toh(v);
	return 0;
}

/* read a UMP packet: return the number of packets, -1 on error */
static int read_ump_packet(FILE *file, uint32_t *buf)
{
	snd_ump_msg_hdr_t *h = (snd_ump_msg_hdr_t *)buf;

	int i, num;

	if (read_word(file, buf) < 0)
		return -1;
	num = snd_ump_packet_length(h->type);
	for (i = 1; i < num; i++) {
		if (read_word(file, buf + i) < 0)
			return -1;
	}
	return num;
}

/* read the file header and verify it's MIDI Clip File: return 0 on success */
static int verify_file_header(FILE *file)
{
	unsigned char buf[8];

	if (fread(buf, 1, 8, file) != 8)
		return -1;
	if (memcmp(buf, "SMF2CLIP", 8))
		return -1;
	return 0;
}

/* return the current tempo, corrected to be sent to host */
static int current_tempo(void)
{
	if (tempo_base != 10)
		return _current_tempo / 100; /* down to us */
	return _current_tempo;
}

/* send a timer event */
static void send_timer_event(unsigned int type, unsigned int val)
{
	snd_seq_ump_event_t ev = {
		.type = type,
		.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_EVENT_LENGTH_FIXED,
	};

	ev.queue = queue;
	ev.source.port = 0;
	ev.time.tick = current_tick;

	ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
	ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
	ev.data.queue.queue = queue;
	ev.data.queue.param.value = val;

	snd_seq_ump_event_output(seq, &ev);
}

/* set DCTPQ */
static void set_dctpq(unsigned int ppq)
{
	snd_seq_queue_tempo_t *queue_tempo;

	snd_seq_queue_tempo_alloca(&queue_tempo);
	snd_seq_queue_tempo_set_tempo(queue_tempo, current_tempo());
	snd_seq_queue_tempo_set_ppq(queue_tempo, ppq);
	snd_seq_queue_tempo_set_tempo_base(queue_tempo, tempo_base);

	if (snd_seq_set_queue_tempo(seq, queue, queue_tempo) < 0)
		errormsg("Cannot set queue tempo (%d)", queue);
}

/* set DC */
static void set_dc(unsigned int ticks)
{
	current_tick += ticks;
}

/* set tempo event */
static void set_tempo(unsigned int tempo)
{
	_current_tempo = tempo;
	send_timer_event(SND_SEQ_EVENT_TEMPO, current_tempo());
}

/* start clip */
static void start_clip(void)
{
	if (snd_seq_start_queue(seq, queue, NULL) < 0)
		errormsg("Cannot start queue (%d)", queue);
}

/* end clip */
static void end_clip(void)
{
	send_timer_event(SND_SEQ_EVENT_STOP, 0);
}

/* send a UMP packet */
static void send_ump(const uint32_t *ump, int len)
{
	snd_seq_ump_event_t ev = {
		.flags = SND_SEQ_TIME_STAMP_TICK | SND_SEQ_EVENT_LENGTH_FIXED |
		SND_SEQ_EVENT_UMP,
	};
	int group;

	memcpy(ev.ump, ump, len * 4);

	ev.queue = queue;
	ev.source.port = 0;
	ev.time.tick = current_tick;
	group = snd_ump_msg_group(ump);
	if (group >= port_count)
		ev.dest = ports[0];
	else
		ev.dest = ports[group];

	snd_seq_ump_event_output(seq, &ev);
}

struct flexdata_text_prefix {
	unsigned char status_bank;
	unsigned char status;
	const char *prefix;
};

static struct flexdata_text_prefix text_prefix[] = {
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_PROJECT_NAME,
	  .prefix = "Project" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_SONG_NAME,
	  .prefix = "Song" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_MIDI_CLIP_NAME,
	  .prefix = "MIDI Clip" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_COPYRIGHT_NOTICE,
	  .prefix = "Copyright" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_COMPOSER_NAME,
	  .prefix = "Composer" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_LYRICIST_NAME,
	  .prefix = "Lyricist" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_ARRANGER_NAME,
	  .prefix = "Arranger" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_PUBLISHER_NAME,
	  .prefix = "Publisher" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_PRIMARY_PERFORMER,
	  .prefix = "Performer" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_ACCOMPANY_PERFORMAER,
	  .prefix = "Accompany Performer" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_RECORDING_DATE,
	  .prefix = "Recording Date" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_RECORDING_LOCATION,
	  .prefix = "Recording Location" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_PERF_TEXT,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_LYRICS,
	  .prefix = "Lyrics" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_PERF_TEXT,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_LYRICS_LANGUAGE,
	  .prefix = "Lyrics Language" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_PERF_TEXT,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_RUBY,
	  .prefix = "Ruby" },
	{ .status_bank = SND_UMP_FLEX_DATA_MSG_BANK_PERF_TEXT,
	  .status = SND_UMP_FLEX_DATA_MSG_STATUS_RUBY_LANGUAGE,
	  .prefix = "Ruby Language" },
	{}
};

static void show_text(const uint32_t *ump)
{
	static unsigned char textbuf[256];
	static int len;
	const snd_ump_msg_flex_data_t *fh =
		(const snd_ump_msg_flex_data_t *)ump;
	const char *prefix;
	int i;

	if (fh->meta.format == SND_UMP_FLEX_DATA_MSG_FORMAT_SINGLE ||
	    fh->meta.format == SND_UMP_FLEX_DATA_MSG_FORMAT_START)
		len = 0;

	for (i = 0; i < 12 && len < (int)sizeof(textbuf); i++) {
		textbuf[len] = snd_ump_get_byte(ump, 4 + i);
		if (!textbuf[len])
			break;
		switch (textbuf[len]) {
		case 0x0a: /* end of paragraph */
		case 0x0d: /* end of line */
			textbuf[len] = '\n';
			break;
		}
		len++;
	}

	if (fh->meta.format != SND_UMP_FLEX_DATA_MSG_FORMAT_SINGLE &&
	    fh->meta.format != SND_UMP_FLEX_DATA_MSG_FORMAT_END)
		return;

	if (len >= (int)sizeof(textbuf))
		len = sizeof(textbuf) - 1;
	textbuf[len] = 0;

	prefix = NULL;
	for (i = 0; text_prefix[i].status_bank; i++) {
		if (text_prefix[i].status_bank == fh->meta.status_bank &&
		    text_prefix[i].status == fh->meta.status) {
			prefix = text_prefix[i].prefix;
			break;
		}
	}

	if (prefix) {
		printf("%s: %s\n", prefix, textbuf);
	} else {
		printf("(%d:%d): %s\n", fh->meta.status_bank, fh->meta.status,
		       textbuf);
	}

	len = 0;
}

/* play the given MIDI Clip File content */
static void play_midi(FILE *file)
{
	uint32_t ump[4];
	int len;

	current_tick = 0;

	while ((len = read_ump_packet(file, ump)) > 0) {
		const snd_ump_msg_hdr_t *h = (snd_ump_msg_hdr_t *)ump;

		if (passall)
			send_ump(ump, len);

		if (h->type == SND_UMP_MSG_TYPE_UTILITY) {
			const snd_ump_msg_utility_t *uh =
				(const snd_ump_msg_utility_t *)ump;
			switch (h->status) {
			case SND_UMP_UTILITY_MSG_STATUS_DCTPQ:
				set_dctpq(uh->dctpq.ticks);
				continue;
			case SND_UMP_UTILITY_MSG_STATUS_DC:
				set_dc(uh->dctpq.ticks);
				continue;
			}
		} else if (h->type == SND_UMP_MSG_TYPE_FLEX_DATA) {
			const snd_ump_msg_flex_data_t *fh =
				(const snd_ump_msg_flex_data_t *)ump;
			if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_SETUP &&
			    fh->meta.status == SND_UMP_FLEX_DATA_MSG_STATUS_SET_TEMPO) {
				set_tempo(fh->set_tempo.tempo);
				continue;
			}

			if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_METADATA ||
			    fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_PERF_TEXT) {
				if (!silent)
					show_text(ump);
				continue;
			}
		} else if (h->type == SND_UMP_MSG_TYPE_STREAM) {
			const snd_ump_msg_stream_t *sh =
				(const snd_ump_msg_stream_t *)ump;
			switch (sh->gen.status) {
			case SND_UMP_STREAM_MSG_STATUS_START_CLIP:
				start_clip();
				continue;
			case SND_UMP_STREAM_MSG_STATUS_END_CLIP:
				end_clip();
				continue;
			}
		} else if (!passall &&
			   (h->type == SND_UMP_MSG_TYPE_MIDI1_CHANNEL_VOICE ||
			    h->type == SND_UMP_MSG_TYPE_DATA ||
			    h->type == SND_UMP_MSG_TYPE_MIDI2_CHANNEL_VOICE)) {
			send_ump(ump, len);
		}
	}

	snd_seq_drain_output(seq);
	snd_seq_sync_output_queue(seq);

	/* give the last notes time to die away */
	if (end_delay > 0)
		sleep(end_delay);
}

static void play_file(const char *file_name)
{
	FILE *file;

	if (!strcmp(file_name, "-"))
		file = stdin;
	else
		file = fopen(file_name, "rb");
	if (!file) {
		errormsg("Cannot open %s - %s", file_name, strerror(errno));
		return;
	}

	if (verify_file_header(file) < 0) {
		errormsg("%s is not a MIDI Clip File", file_name);
		goto error;
	}

	play_midi(file);

 error:
	if (file != stdin)
		fclose(file);
}

static void usage(const char *argv0)
{
	printf(
		"Usage: %s -p client:port[,...] [-d delay] midifile ...\n"
		"-h, --help                  this help\n"
		"-V, --version               print current version\n"
		"-p, --port=client:port,...  set port(s) to play to\n"
		"-d, --delay=seconds         delay after song ends\n"
		"-s, --silent                don't show texts\n"
		"-a, --passall               pass all UMP packets as-is\n",
		argv0);
}

static void version(void)
{
	puts("aplaymidi2 version " SND_UTIL_VERSION_STR);
}

int main(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'V'},
		{"port", 1, NULL, 'p'},
		{"delay", 1, NULL, 'd'},
		{"silent", 0, NULL, 's'},
		{"passall", 0, NULL, 'a'},
		{0}
	};
	int c;

	init_seq();

	while ((c = getopt_long(argc, argv, "hVp:d:sa",
				long_options, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'V':
			version();
			return 0;
		case 'p':
			parse_ports(optarg);
			break;
		case 'd':
			end_delay = atoi(optarg);
			break;
		case 's':
			silent = 1;
			break;
		case 'a':
			passall = 1;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}


	if (port_count < 1) {
		/* use env var for compatibility with pmidi */
		const char *ports_str = getenv("ALSA_OUTPUT_PORTS");
		if (ports_str)
			parse_ports(ports_str);
		if (port_count < 1) {
			errormsg("Please specify at least one port with --port.");
			return 1;
		}
	}
	if (optind >= argc) {
		errormsg("Please specify a file to play.");
		return 1;
	}

	create_source_port();
	create_queue();
	connect_ports();

	for (; optind < argc; optind++)
		play_file(argv[optind]);

	snd_seq_close(seq);
	return 0;
}
