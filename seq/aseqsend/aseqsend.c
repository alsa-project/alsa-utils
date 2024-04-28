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

typedef unsigned char mbyte_t;

static snd_seq_t *seq;
static char *port_name = NULL;
static char *send_file_name = NULL;
static char *send_hex;
static mbyte_t *send_data;
static snd_seq_addr_t addr;
static int send_data_length;

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
		"         -h  this help\n"
		"         -V  print current version\n"
		"         -v  verbose\n"
		"         -l  list all sequencer ports\n"
		"         -p  target port by number or name\n"
		"         -s  send binary data from given file name\n"
		"         -i  interval between SysEx messages in miliseconds\n\n");
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

void send_midi_msg(snd_seq_event_type_t type, mbyte_t *data, int len)
{
		snd_seq_event_t ev;

		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_source(&ev, 0);
		snd_seq_ev_set_dest(&ev,addr.client,addr.port);
		snd_seq_ev_set_direct(&ev);

		if (type == SND_SEQ_EVENT_SYSEX) {

			snd_seq_ev_set_sysex(&ev,len,data);

		} else {

			mbyte_t ch = data[0] & 0xF;

			switch (type) {
				case SND_SEQ_EVENT_NOTEON:
					snd_seq_ev_set_noteon(&ev,ch,data[1],data[2]);
					break;
				case SND_SEQ_EVENT_NOTEOFF:
					snd_seq_ev_set_noteoff(&ev,ch,data[1],data[2]);
					break;
				case SND_SEQ_EVENT_KEYPRESS:
					snd_seq_ev_set_keypress(&ev,ch,data[1],data[2]);
					break;
				case SND_SEQ_EVENT_CONTROLLER:
					snd_seq_ev_set_controller(&ev,ch,data[1],data[2]);
					break;
				case SND_SEQ_EVENT_PITCHBEND:
					snd_seq_ev_set_pitchbend(&ev,ch,(data[1]<<7|data[2])-8192);
					break;
				case SND_SEQ_EVENT_PGMCHANGE:
					snd_seq_ev_set_pgmchange(&ev,ch,data[1]);
					break;
				case SND_SEQ_EVENT_CHANPRESS:
					snd_seq_ev_set_chanpress(&ev,ch,data[1]);
					break;
				default:
					ev.type = SND_SEQ_EVENT_NONE;
			}
		}

		snd_seq_event_output(seq, &ev);
		snd_seq_drain_output(seq);

}

static int msg_byte_in_range(mbyte_t *data, mbyte_t len)
{
	for (int i=0;i<len;i++) {
		if (data[i] > 0x7F) {
			error("msg byte value out of range 0-127");
			return 0;
		}
	}
	return 1;
}


int main(int argc, char *argv[])
{
	char c = 0;
	char do_send_file = 0;
	char do_port_list = 0;
	char verbose = 0;
	int sysex_interval = 1000; //us

	while ((c = getopt(argc, argv, "hi:Vvlp:s:")) != -1) {
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
		if (send_hex) parse_data();
	}

	if (send_data) {

		init_seq();
		create_port();

		if (snd_seq_parse_address(seq,&addr,port_name) == 0) {

			int sent_data_c = 0;//counter of actually sent bytes

			int k = 0;

			while (k < send_data_length) {

				if (send_data[k] == 0xF0) {

					int c1 = k;
					while (c1 < send_data_length)
					{
						if (send_data[c1] == 0xF7) break;
						c1++;
					}

					if (c1 == send_data_length)
						fatal("SysEx is missing terminating byte (0xF7)");

					int sl = c1-k+1;
					sent_data_c += sl;

					send_midi_msg(SND_SEQ_EVENT_SYSEX, send_data+k,sl);

					usleep(sysex_interval);

					k = c1+1;

				} else {

					mbyte_t tp = send_data[k] >> 4;

					if (tp == 0x8) {
						if (msg_byte_in_range(send_data + k + 1, 2)) {
							send_midi_msg(SND_SEQ_EVENT_NOTEOFF, send_data+k,3);
							sent_data_c += 3;
						}
						k = k+3;
					} else if (tp == 0x9) {
						if (msg_byte_in_range(send_data + k + 1, 2)) {
							send_midi_msg(SND_SEQ_EVENT_NOTEON, send_data+k,3);
							sent_data_c += 3;
						}
						k = k+3;
					} else if (tp == 0xA) {
						if (msg_byte_in_range(send_data + k + 1, 2)) {
							send_midi_msg(SND_SEQ_EVENT_KEYPRESS, send_data+k,3);
							sent_data_c += 3;
						}
						k = k+3;
					} else if (tp == 0xB) {
						if (msg_byte_in_range(send_data + k + 1, 2)) {
							send_midi_msg(SND_SEQ_EVENT_CONTROLLER, send_data+k,3);
							sent_data_c += 3;
						}
						k = k+3;
					} else if (tp == 0xC) {
						if (msg_byte_in_range(send_data + k + 1, 1)) {
							send_midi_msg(SND_SEQ_EVENT_PGMCHANGE, send_data+k,2);
							sent_data_c += 2;
						}
						k = k+2;
					} else if (tp == 0xD) {
						if (msg_byte_in_range(send_data + k + 1, 1)) {
							send_midi_msg(SND_SEQ_EVENT_CHANPRESS, send_data+k,2);
							sent_data_c += 2;
						}
						k = k+2;
					} else if (tp == 0xE) {
						if (msg_byte_in_range(send_data + k + 1, 2)) {
							send_midi_msg(SND_SEQ_EVENT_PITCHBEND, send_data+k,3);
							sent_data_c += 3;
						}
						k = k+3;
					} else k++;
				}
			}

			if (verbose)
				printf("Sent : %u bytes\n",sent_data_c);

		} else {

			error("Unable to parse port name!");
			exit(EXIT_FAILURE);

		}
		snd_seq_close(seq);
	}

	exit(EXIT_SUCCESS);
}
