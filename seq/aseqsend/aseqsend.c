/*
 *  aseqsend.c - send arbitrary MIDI messages to selected ALSA MIDI seqencer port
 *
 *  Copyright (c) 2005 Clemens Ladisch <clemens@ladisch.de>
 *  Copyright (c) 2024 Miroslav Kovac <mixxoo@gmail.com>
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define _GNU_SOURCE
#include "aconfig.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <alsa/ump_msg.h>

typedef unsigned char mbyte_t;

static snd_seq_t *seq;
static char *port_name = NULL;
static char *send_file_name = NULL;
static char *send_hex;
static mbyte_t *send_data;
static snd_seq_addr_t addr;
static int send_data_length;
static int sent_data_c;
static int ump_version;
static int sysex_interval = 1000; //us
static snd_midi_event_t *edev;

static void error(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	putc('\n', stderr);
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

static void usage(void)
{
	printf(
		"\nUsage: aseqsend -p target-port -s file-name|\"hex encoded bytes\"\n\n"
		"  -h,--help         this help\n"
		"  -V,--version      print current version\n"
		"  -v,--verbose      verbose mode\n"
		"  -l,--list         list all sequencer ports\n"
		"  -p,--port=c:p     target port by number or name\n"
		"  -s,--file=name    send binary data from given file name\n"
		"  -i,--interval=v   interval between SysEx messages in miliseconds\n"
		"  -u,--ump=version  MIDI version: 0=legacy (default), 1=MIDI1, 2=MIDI2\n\n");
}

static void version(void)
{
	puts("aseqsend version " SND_UTIL_VERSION_STR);
}

static void *my_malloc(size_t size)
{
	void *p = malloc(size);
	if (!p) {
		fatal("out of memory");
		exit(EXIT_FAILURE);
	}
	return p;
}

static int hex_value(char c)
{
	if ('0' <= c && c <= '9')
		return c - '0';
	if ('A' <= c && c <= 'F')
		return c - 'A' + 10;
	if ('a' <= c && c <= 'f')
		return c - 'a' + 10;
	error("invalid character %c", c);
	return -1;
}

static void parse_data(void)
{
	const char *p;
	int i, value;

	send_data = my_malloc(strlen(send_hex));
	i = 0;
	value = -1; /* value is >= 0 when the first hex digit of a byte has been read */
	for (p = send_hex; *p; ++p) {
		int digit;
		if (isspace((unsigned char)*p)) {
			if (value >= 0) {
				send_data[i++] = value;
				value = -1;
			}
			continue;
		}
		digit = hex_value(*p);
		if (digit < 0) {
			exit(EXIT_FAILURE);
		}
		if (value < 0) {
			value = digit;
		} else {
			send_data[i++] = (value << 4) | digit;
			value = -1;
		}
	}
	if (value >= 0)
		send_data[i++] = value;
	send_data_length = i;
}

static void add_send_hex_data(const char *str)
{
	int length;
	char *s;

	length = (send_hex ? strlen(send_hex) + 1 : 0) + strlen(str) + 1;
	s = my_malloc(length);
	if (send_hex) {
		strcpy(s, send_hex);
		strcat(s, " ");
	} else {
		s[0] = '\0';
	}
	strcat(s, str);
	free(send_hex);
	send_hex = s;
}

static void load_file(void)
{
	int fd;
	off_t length;

	fd = open(send_file_name, O_RDONLY);
	if (fd == -1) {
		error("cannot open %s - %s", send_file_name, strerror(errno));
		return;
	}
	length = lseek(fd, 0, SEEK_END);
	if (length == (off_t)-1) {
		error("cannot determine length of %s: %s", send_file_name, strerror(errno));
		goto _error;
	}
	send_data = my_malloc(length);
	lseek(fd, 0, SEEK_SET);
	if (read(fd, send_data, length) != length) {
		error("cannot read from %s: %s", send_file_name, strerror(errno));
		goto _error;
	}
	if (length >= 4 && !memcmp(send_data, "MThd", 4)) {
		error("%s is a Standard MIDI File; use aplaymidi to send it", send_file_name);
		goto _error;
	}
	send_data_length = length;
	goto _exit;
_error:
	free(send_data);
	send_data = NULL;
_exit:
	close(fd);
}

/* error handling for ALSA functions */
static void check_snd(const char *operation, int err)
{
	if (err < 0)
		fatal("Cannot %s - %s", operation, snd_strerror(err));
}

static void init_seq(void)
{
	int err;

	/* open sequencer */
	err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0);
	check_snd("open sequencer", err);

	/* set our client's name */
	err = snd_seq_set_client_name(seq, "aseqsend");
	check_snd("set client name", err);

	err = snd_seq_set_client_midi_version(seq, ump_version);
	check_snd("set client midi version", err);
}

static void create_port(void)
{
	int err;

	err = snd_seq_create_simple_port(seq, "aseqsend",
					SND_SEQ_PORT_CAP_READ,
					SND_SEQ_PORT_TYPE_MIDI_GENERIC |
					SND_SEQ_PORT_TYPE_APPLICATION);
	check_snd("create port", err);
}

static void init_midi_event_encoder(void)
{
	int err;

	err = snd_midi_event_new(256, &edev);
	check_snd("create midi event encoder", err);
}

static void list_ports(void)
{
	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	puts(" Port    Client name                      Port name");

	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		int client = snd_seq_client_info_get_client(cinfo);

		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0) {

			if ((snd_seq_port_info_get_capability(pinfo)
			     & SND_SEQ_PORT_CAP_WRITE)
			    != SND_SEQ_PORT_CAP_WRITE)
				continue;
			printf("%3d:%-3d  %-32.32s %s\n",
			       snd_seq_port_info_get_client(pinfo),
			       snd_seq_port_info_get_port(pinfo),
			       snd_seq_client_info_get_name(cinfo),
			       snd_seq_port_info_get_name(pinfo));
		}
	}
}

/* compose a UMP event, submit it, return the next data position */
static int send_ump(int pos)
{
	int ump_len = 0, offset = 0;
	unsigned int ump[4];
	snd_seq_ump_event_t ev;

	snd_seq_ump_ev_clear(&ev);
	snd_seq_ev_set_source(&ev, 0);
	snd_seq_ev_set_dest(&ev, addr.client, addr.port);
	snd_seq_ev_set_direct(&ev);

	do {
		const mbyte_t *data = send_data + pos;

		if (pos >= send_data_length)
			return pos;
		ump[offset] = (data[0] << 24) | (data[1] << 16) |
			(data[2] << 8) | data[3];
		if (!offset)
			ump_len = snd_ump_packet_length(snd_ump_msg_type(ump));
		offset++;
		pos += 4;
	} while (offset < ump_len);

	snd_seq_ev_set_ump_data(&ev, ump, ump_len * 4);
	snd_seq_ump_event_output(seq, &ev);
	snd_seq_drain_output(seq);

	sent_data_c += ump_len * 4;
	return pos;
}

/* compose an event, submit it, return the next data position */
static int send_midi_bytes(int pos)
{
	const mbyte_t *data = send_data + pos;
	snd_seq_event_t ev;
	int is_sysex = 0;
	int end;

	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev, 0);
	snd_seq_ev_set_dest(&ev, addr.client, addr.port);
	snd_seq_ev_set_direct(&ev);

	if (send_data[pos] == 0xf0) {
		is_sysex = 1;
		for (end = pos + 1; end < send_data_length; end++) {
			if (send_data[end] == 0xf7)
				break;
		}

		if (end == send_data_length)
			fatal("SysEx is missing terminating byte (0xF7)");
		end++;
		snd_seq_ev_set_sysex(&ev, end - pos, send_data + pos);
	} else {
		end = pos;
		while (!snd_midi_event_encode_byte(edev, *data++, &ev)) {
			if (++end >= send_data_length)
				return end;
		}

		end++;
	}

	snd_seq_event_output(seq, &ev);
	snd_seq_drain_output(seq);
	if (is_sysex)
		usleep(sysex_interval);

	sent_data_c += end - pos;
	return end;
}

int main(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'V'},
		{"verbose", 0, NULL, 'v'},
		{"list", 0, NULL, 'l'},
		{"port", 1, NULL, 'p'},
		{"file", 1, NULL, 's'},
		{"interval", 1, NULL, 'i'},
		{"ump", 1, NULL, 'u'},
		{0}
	};
	char c = 0;
	char do_send_file = 0;
	char do_port_list = 0;
	char verbose = 0;
	int k;

	while ((c = getopt_long(argc, argv, "hi:Vvlp:s:u:", long_options, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage();
			return 0;
		case 'V':
			version();
			return 0;
		case 'v':
			verbose = 1;
			break;
		case 'l':
			do_port_list = 1;
			break;
		case 'p':
			port_name = optarg;
			break;
		case 's':
			send_file_name = optarg;
			do_send_file = 1;
			break;
		case 'i':
			sysex_interval = atoi(optarg) * 1000; //ms--->us
			break;
		case 'u':
			ump_version = atoi(optarg);
			break;
		default:
			error("Try 'aseqsend -h' for more information.");
			exit(EXIT_FAILURE);
		}
	}

	if (argc < 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	if (do_port_list){
		init_seq();
		list_ports();
		exit(EXIT_SUCCESS);
	}

	if (port_name == NULL)
		fatal("Output port must be specified!");

	if (do_send_file) {
		load_file();
	} else {
		/* no file specified ---> send hex bytes from cmd arguments*/
		/* data for send can be specified as multiple arguments */
		for (; argv[optind]; ++optind) {
			add_send_hex_data(argv[optind]);
		}
		if (send_hex)
			parse_data();
	}

	if (!send_data)
		exit(EXIT_SUCCESS);

	if (ump_version && (send_data_length % 4) != 0)
		fatal("UMP data must be aligned to 4 bytes");

	init_seq();
	create_port();
	if (!ump_version)
		init_midi_event_encoder();

	if (snd_seq_parse_address(seq, &addr, port_name) < 0) {
		error("Unable to parse port name!");
		exit(EXIT_FAILURE);
	}

	sent_data_c = 0; //counter of actually sent bytes

	k = 0;
	while (k < send_data_length) {
		if (ump_version)
			k = send_ump(k);
		else
			k = send_midi_bytes(k);
	}

	if (verbose)
		printf("Sent : %u bytes\n", sent_data_c);

	snd_seq_close(seq);
	exit(EXIT_SUCCESS);
}
