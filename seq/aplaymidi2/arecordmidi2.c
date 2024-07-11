/*
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <poll.h>
#include <alsa/asoundlib.h>
#include <alsa/ump_msg.h>
#include "aconfig.h"
#include "version.h"

static snd_seq_t *seq;
static int client;
static int port_count;
static snd_seq_addr_t *ports;
static int queue;
static int midi_version = 1;
static int beats = 120;
static int ticks = 384;
static int tempo_base = 10;
static volatile sig_atomic_t stop;
static int ts_num = 4; /* time signature: numerator */
static int ts_div = 4; /* time signature: denominator */
static int last_tick;
static int silent;
static const char *profile_ump_file;

#define MAX_METADATA	16
static int metadata_num;
static unsigned int metadata_types[MAX_METADATA];
static const char *metadata_texts[MAX_METADATA];

/* Parse a decimal number from a command line argument. */
static long arg_parse_decimal_num(const char *str, int *err)
{
	long val;
	char *endptr;

	errno = 0;
	val = strtol(str, &endptr, 0);
	if (errno > 0) {
		*err = -errno;
		return 0;
	}
	if (*endptr != '\0') {
		*err = -EINVAL;
		return 0;
	}

	return val;
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

/* open a sequencer client */
static void init_seq(void)
{
	int err;

	/* open sequencer */
	err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	check_snd("open sequencer", err);

	/* find out our client's id */
	client = snd_seq_client_id(seq);
	check_snd("get client id", client);
}

/* set up UMP virtual client/port */
static void create_ump_client(void)
{
	snd_ump_endpoint_info_t *ep;
	snd_ump_block_info_t *blk;
	snd_seq_port_info_t *pinfo;
	int num_groups;
	int i, err;

	/* in passive mode, create full 16 groups */
	if (port_count)
		num_groups = port_count;
	else
		num_groups = 16;

	/* create a UMP Endpoint */
	snd_ump_endpoint_info_alloca(&ep);
	snd_ump_endpoint_info_set_name(ep, "arecordmidi2");
	if (midi_version == 1) {
		snd_ump_endpoint_info_set_protocol_caps(ep, SND_UMP_EP_INFO_PROTO_MIDI1);
		snd_ump_endpoint_info_set_protocol(ep, SND_UMP_EP_INFO_PROTO_MIDI1);
	} else {
		snd_ump_endpoint_info_set_protocol_caps(ep, SND_UMP_EP_INFO_PROTO_MIDI2);
		snd_ump_endpoint_info_set_protocol(ep, SND_UMP_EP_INFO_PROTO_MIDI2);
	}
	snd_ump_endpoint_info_set_num_blocks(ep, num_groups);

	err = snd_seq_create_ump_endpoint(seq, ep, num_groups);
	check_snd("create UMP endpoint", err);

	/* create UMP Function Blocks */
	snd_ump_block_info_alloca(&blk);
	for (i = 0; i < num_groups; i++) {
		char blkname[32];

		sprintf(blkname, "Group %d", i + 1);
		snd_ump_block_info_set_name(blk, blkname);
		snd_ump_block_info_set_direction(blk, SND_UMP_DIR_INPUT);
		snd_ump_block_info_set_first_group(blk, i);
		snd_ump_block_info_set_num_groups(blk, 1);
		snd_ump_block_info_set_ui_hint(blk, SND_UMP_BLOCK_UI_HINT_RECEIVER);

		err = snd_seq_create_ump_block(seq, i, blk);
		check_snd("create UMP block", err);
	}

	/* toggle timestamping for all input ports */
	snd_seq_port_info_alloca(&pinfo);
	for (i = 0; i <= num_groups; i++) {
		err = snd_seq_get_port_info(seq, i, pinfo);
		check_snd("get port info", err);
		snd_seq_port_info_set_timestamping(pinfo, 1);
		snd_seq_port_info_set_timestamp_queue(pinfo, queue);
		snd_seq_set_port_info(seq, i, pinfo);
		check_snd("set port info", err);
	}
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
		 * spaces because those are valid in client names.
		 */
		s = strchr(port_name, ',');
		if (s)
			*s = '\0';

		++port_count;
		ports = realloc(ports, port_count * sizeof(snd_seq_addr_t));
		check_mem(ports);

		err = snd_seq_parse_address(seq, &ports[port_count - 1], port_name);
		if (err < 0)
			fatal("Invalid port %s - %s", port_name, snd_strerror(err));
	}

	free(buf);
}

/* parses time signature specification */
static void time_signature(const char *arg)
{
	long x = 0;
	char *sep;

	x = strtol(arg, &sep, 10);
	if (x < 1 || x > 64 || *sep != ':')
		fatal("Invalid time signature (%s)", arg);
	ts_num = x;
	x = strtol(++sep, NULL, 10);
	if (x < 1 || x > 64)
		fatal("Invalid time signature (%s)", arg);
	ts_div = x;
}

/* create a queue, set up the default tempo */
static void create_queue(void)
{
	snd_seq_queue_tempo_t *tempo;

	if (!snd_seq_has_queue_tempo_base(seq))
		tempo_base = 1000;

	queue = snd_seq_alloc_named_queue(seq, "arecordmidi2");
	check_snd("create queue", queue);

	snd_seq_queue_tempo_alloca(&tempo);
	if (tempo_base == 1000)
		snd_seq_queue_tempo_set_tempo(tempo, 60000000 / beats);
	else
		snd_seq_queue_tempo_set_tempo(tempo, (unsigned int)(6000000000ULL / beats));
	snd_seq_queue_tempo_set_ppq(tempo, ticks);
	snd_seq_queue_tempo_set_tempo_base(tempo, tempo_base);
	if (snd_seq_set_queue_tempo(seq, queue, tempo) < 0)
		fatal("Cannot set queue tempo (%d)", queue);
}

/* connect to the input ports */
static void connect_ports(void)
{
	int i, err;

	for (i = 0; i < port_count; ++i) {
		err = snd_seq_connect_from(seq, i + 1,
					   ports[i].client, ports[i].port);
		check_snd("port connection", err);
	}
}

/* write the given UMP packet */
static void write_ump(FILE *file, const void *src)
{
	const snd_ump_msg_hdr_t *h = src;
	const uint32_t *p = src;
	uint32_t v;
	int len;

	len = snd_ump_packet_length(h->type);
	while (len-- > 0) {
		v = htobe32(*p++);
		fwrite(&v, 4, 1, file);
	}
}

/* write a DC message */
static void write_dcs(FILE *file, unsigned int t)
{
	snd_ump_msg_dc_t d = {};

	d.type = SND_UMP_MSG_TYPE_UTILITY;
	d.status = SND_UMP_UTILITY_MSG_STATUS_DC;
	d.ticks = t;
	write_ump(file, &d);
}

/* write a DCTPQ message */
static void write_dctpq(FILE *file)
{
	snd_ump_msg_dctpq_t d = {};

	d.type = SND_UMP_MSG_TYPE_UTILITY;
	d.status = SND_UMP_UTILITY_MSG_STATUS_DCTPQ;
	d.ticks = ticks;
	write_ump(file, &d);
}

/* write a Start Clip message */
static void write_start_clip(FILE *file)
{
	snd_ump_msg_stream_gen_t d = {};

	d.type = SND_UMP_MSG_TYPE_STREAM;
	d.status = SND_UMP_STREAM_MSG_STATUS_START_CLIP;
	write_ump(file, &d);
}

/* write an End Clip message */
static void write_end_clip(FILE *file)
{
	snd_ump_msg_stream_gen_t d = {};

	d.type = SND_UMP_MSG_TYPE_STREAM;
	d.status = SND_UMP_STREAM_MSG_STATUS_END_CLIP;
	write_ump(file, &d);
}

/* write a Set Tempo message */
static void write_tempo(FILE *file)
{
	snd_ump_msg_set_tempo_t d = {};

	d.type = SND_UMP_MSG_TYPE_FLEX_DATA;
	d.group = 0;
	d.format = SND_UMP_FLEX_DATA_MSG_FORMAT_SINGLE;
	d.addrs = SND_UMP_FLEX_DATA_MSG_ADDR_GROUP;
	d.status_bank = SND_UMP_FLEX_DATA_MSG_BANK_SETUP;
	d.status = SND_UMP_FLEX_DATA_MSG_STATUS_SET_TEMPO;
	d.tempo = (unsigned int)(6000000000ULL / beats);
	write_ump(file, &d);
}

/* write a Set Time Signature message */
static void write_time_sig(FILE *file)
{
	snd_ump_msg_set_time_sig_t d = {};

	d.type = SND_UMP_MSG_TYPE_FLEX_DATA;
	d.group = 0;
	d.format = SND_UMP_FLEX_DATA_MSG_FORMAT_SINGLE;
	d.addrs = SND_UMP_FLEX_DATA_MSG_ADDR_GROUP;
	d.status_bank = SND_UMP_FLEX_DATA_MSG_BANK_SETUP;
	d.status = SND_UMP_FLEX_DATA_MSG_STATUS_SET_TIME_SIGNATURE;
	d.numerator = ts_num;
	d.denominator = ts_div;
	d.num_notes = 8;
	write_ump(file, &d);
}

/* record the delta time from the last event */
static void delta_time(FILE *file, const snd_seq_ump_event_t *ev)
{
	int diff = ev->time.tick - last_tick;

	if (diff <= 0)
		return;
	write_dcs(file, diff);
	last_tick = ev->time.tick;
}

static void record_event(FILE *file, const snd_seq_ump_event_t *ev)
{
	/* ignore events without proper timestamps */
	if (ev->queue != queue || !snd_seq_ev_is_tick(ev) ||
	    !snd_seq_ev_is_ump(ev))
		return;

	delta_time(file, ev);
	write_ump(file, ev->ump);
}

/* read a UMP raw (big-endian) packet, return the packet length in words */
static int read_ump_raw(FILE *file, uint32_t *buf)
{
	uint32_t v;
	int i, num;

	if (fread(buf, 4, 1, file) != 1)
		return 0;
	v = be32toh(v);
	num = snd_ump_packet_length(snd_ump_msg_hdr_type(v));
	for (i = 1; i < num; i++) {
		if (fread(buf + i, 4, 1, file) != 1)
			return 0;
	}
	return num;
}

/* read the profile UMP data and write to the configuration */
static void write_profiles(FILE *file)
{
	FILE *fp;
	uint32_t ump[4];
	int len;

	if (!profile_ump_file)
		return;

	fp = fopen(profile_ump_file, "rb");
	if (!fp)
		fatal("cannot open the profile '%s'", profile_ump_file);

	while (!feof(fp)) {
		len = read_ump_raw(fp, ump);
		if (!len)
			break;
		fwrite(ump, 4, len, file);
	}

	fclose(fp);
}

/* write Flex Data metadata text given by command lines */
static void write_metadata(FILE *file, unsigned int type, const char *text)
{
	int len = strlen(text), size;
	unsigned int format = SND_UMP_FLEX_DATA_MSG_FORMAT_START;

	while (len > 0) {
		snd_ump_msg_flex_data_t d = {};

		if (len <= 12) {
			if (format == SND_UMP_FLEX_DATA_MSG_FORMAT_CONTINUE)
				format = SND_UMP_FLEX_DATA_MSG_FORMAT_END;
			else
				format = SND_UMP_FLEX_DATA_MSG_FORMAT_SINGLE;
			size = len;
		} else {
			size = 12;
		}

		d.meta.type = SND_UMP_MSG_TYPE_FLEX_DATA;
		d.meta.addrs = SND_UMP_FLEX_DATA_MSG_ADDR_GROUP;
		d.meta.status_bank = SND_UMP_FLEX_DATA_MSG_BANK_METADATA;
		d.meta.status = type;
		d.meta.format = format;

		/* keep the data in big endian */
		d.raw[0] = htobe32(d.raw[0]);
		/* strings are copied as-is in big-endian */
		memcpy(d.meta.data, text, size);

		fwrite(d.raw, 4, 4, file);
		len -= size;
		text += size;
		format = SND_UMP_FLEX_DATA_MSG_FORMAT_CONTINUE;
	}
}

/* write MIDI Clip file header and the configuration packets */
static void write_file_header(FILE *file)
{
	int i;

	/* header id */
	fwrite("SMF2CLIP", 1, 8, file);

	/* clip configuration header */
	write_profiles(file);

	for (i = 0; i < metadata_num; i++)
		write_metadata(file, metadata_types[i], metadata_texts[i]);

	/* first DCS */
	write_dcs(file, 0);
	write_dctpq(file);
}

/* write start bar */
static void start_bar(FILE *file)
{
	int err;

	/* start the queue */
	err = snd_seq_start_queue(seq, queue, NULL);
	check_snd("start queue", err);
	snd_seq_drain_output(seq);

	write_start_clip(file);
	write_tempo(file);
	write_time_sig(file);
}

static void help(const char *argv0)
{
	fprintf(stderr, "Usage: %s [options] outputfile\n"
		"\nAvailable options:\n"
		"  -h,--help                  this help\n"
		"  -V,--version               show version\n"
		"  -p,--port=client:port,...  source port(s)\n"
		"  -b,--bpm=beats             tempo in beats per minute\n"
		"  -t,--ticks=ticks           resolution in ticks per beat or frame\n"
		"  -i,--timesig=nn:dd         time signature\n"
		"  -n,--num-events=events     fixed number of events to record, then exit\n"
		"  -u,--ump=version           UMP MIDI version (1 or 2)\n"
		"  -r,--interactive           Interactive mode\n"
		"  -s,--silent                don't print messages\n"
		"  -P,--profile=file          configuration profile UMP\n"
		"  --project=text             put project name meta data text\n"
		"  --song=text                put song name meta data text\n"
		"  --clip=text                put MIDI clip name meta data text\n"
		"  --copyright=text           put copyright notice meta data text\n"
		"  --composer=text            put composer name meta data text\n"
		"  --lyricist=text            put lyricist name meta data text\n"
		"  --arranger=text            put arranger name meta data text\n"
		"  --publisher=text           put publisher name meta data text\n"
		"  --publisher=text           put publisher name meta data text\n"
		"  --publisher=text           put publisher name meta data text\n"
		"  --performer=text           put performer name meta data text\n"
		"  --accompany=text           put accompany performer name meta data text\n"
		"  --date=text                put recording date meta data text\n"
		"  --location=text            put recording location meta data text\n",
		argv0);
}

static void version(void)
{
	fputs("arecordmidi version " SND_UTIL_VERSION_STR "\n", stderr);
}

static void sighandler(int sig ATTRIBUTE_UNUSED)
{
	stop = 1;
}

#define OPT_META_BIT		0x1000
enum {
	OPT_META_PROJECT	= 0x1001,
	OPT_META_SONG		= 0x1002,
	OPT_META_CLIP		= 0x1003,
	OPT_META_COPYRIGHT	= 0x1004,
	OPT_META_COMPOSER	= 0x1005,
	OPT_META_LYRICIST	= 0x1006,
	OPT_META_ARRANGER	= 0x1007,
	OPT_META_PUBLISHER	= 0x1008,
	OPT_META_PERFORMER	= 0x1009,
	OPT_META_ACCOMPANY	= 0x100a,
	OPT_META_DATE		= 0x100b,
	OPT_META_LOCATION	= 0x100c,
};

int main(int argc, char *argv[])
{
	static const char short_options[] = "hVp:b:t:n:u:rsP:";
	static const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'V'},
		{"port", 1, NULL, 'p'},
		{"bpm", 1, NULL, 'b'},
		{"ticks", 1, NULL, 't'},
		{"timesig", 1, NULL, 'i'},
		{"num-events", 1, NULL, 'n'},
		{"ump", 1, NULL, 'u'},
		{"interactive", 0, NULL, 'r'},
		{"silent", 0, NULL, 's'},
		{"profile", 1, NULL, 'P'},
		/* meta data texts */
		{"project", 1, NULL, OPT_META_PROJECT},
		{"song", 1, NULL, OPT_META_SONG},
		{"clip", 1, NULL, OPT_META_CLIP},
		{"copyright", 1, NULL, OPT_META_COPYRIGHT},
		{"composer", 1, NULL, OPT_META_COMPOSER},
		{"lyricist", 1, NULL, OPT_META_LYRICIST},
		{"arranger", 1, NULL, OPT_META_ARRANGER},
		{"publisher", 1, NULL, OPT_META_PUBLISHER},
		{"performer", 1, NULL, OPT_META_PERFORMER},
		{"accompany", 1, NULL, OPT_META_ACCOMPANY},
		{"date", 1, NULL, OPT_META_DATE},
		{"location", 1, NULL, OPT_META_LOCATION},
		{0}
	};

	char *filename;
	FILE *file;
	struct pollfd *pfds;
	int npfds;
	int c, err;
	/* If |num_events| isn't specified, leave it at 0. */
	long num_events = 0;
	long events_received = 0;
	int start = 0;
	int interactive = 0;

	init_seq();

	while ((c = getopt_long(argc, argv, short_options,
				long_options, NULL)) != -1) {
		switch (c) {
		case 'h':
			help(argv[0]);
			return 0;
		case 'V':
			version();
			return 0;
		case 'p':
			parse_ports(optarg);
			break;
		case 'b':
			beats = atoi(optarg);
			if (beats < 4 || beats > 6000)
				fatal("Invalid tempo");
			break;
		case 't':
			ticks = atoi(optarg);
			if (ticks < 1 || ticks > 0x7fff)
				fatal("Invalid number of ticks");
			break;
		case 'i':
			time_signature(optarg);
			break;
		case 'n':
			err = 0;
			num_events = arg_parse_decimal_num(optarg, &err);
			if (err != 0) {
				fatal("Couldn't parse num_events argument: %s\n",
					strerror(-err));
			}
			if (num_events <= 0)
				fatal("num_events must be greater than 0");
			break;
		case 'u':
			midi_version = atoi(optarg);
			if (midi_version != 1 && midi_version != 2)
				fatal("Invalid MIDI version %d\n", midi_version);
			break;
		case 'r':
			interactive = 1;
			break;
		case 's':
			silent = 1;
			break;
		case 'P':
			profile_ump_file = optarg;
			break;
		default:
			if (c & OPT_META_BIT) {
				if (metadata_num >= MAX_METADATA)
					fatal("Too many metadata given");
				metadata_types[metadata_num] = c & 0x0f;
				metadata_texts[metadata_num] = optarg;
				metadata_num++;
				break;
			}
			help(argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		fputs("Please specify a file to record to.\n", stderr);
		return 1;
	}

	create_queue();
	create_ump_client();
	if (port_count)
		connect_ports();

	filename = argv[optind];

	if (!strcmp(filename, "-")) {
		file = stdout;
		silent = 1; // imply silent mode
	} else {
		file = fopen(filename, "wb");
		if (!file)
			fatal("Cannot open %s - %s", filename, strerror(errno));
	}

	write_file_header(file);
	if (interactive) {
		if (!silent) {
			printf("Press RETURN to start recording:");
			fflush(stdout);
		}
	} else {
		start_bar(file);
		start = 1;
	}

	err = snd_seq_nonblock(seq, 1);
	check_snd("set nonblock mode", err);

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
	pfds = alloca(sizeof(*pfds) * (npfds + 1));
	for (;;) {
		snd_seq_poll_descriptors(seq, pfds, npfds, POLLIN);
		if (interactive) {
			pfds[npfds].fd = STDIN_FILENO;
			pfds[npfds].events = POLLIN | POLLERR | POLLNVAL;
			if (poll(pfds, npfds + 1, -1) < 0)
				break;
			if (pfds[npfds].revents & POLLIN) {
				while (!feof(stdin) && getchar() != '\n')
					;
				if (!start) {
					start_bar(file);
					start = 1;
					if (!silent) {
						printf("Press RETURN to stop recording:");
						fflush(stdout);
					}
					continue;
				} else {
					stop = 1;
				}
			}
		} else {
			if (poll(pfds, npfds, -1) < 0)
				break;
		}

		do {
			snd_seq_ump_event_t *event;

			err = snd_seq_ump_event_input(seq, &event);
			if (err < 0)
				break;
			if (start && event) {
				record_event(file, event);
				events_received++;
			}
		} while (err > 0);
		if (stop)
			break;
		if (num_events && (events_received >= num_events))
			break;
	}

	if (num_events && events_received < num_events) {
		if (!silent)
			fputs("Warning: Received signal before num_events\n", stdout);
	}

	write_end_clip(file);
	if (file != stdout)
		fclose(file);
	snd_seq_close(seq);
	return 0;
}
