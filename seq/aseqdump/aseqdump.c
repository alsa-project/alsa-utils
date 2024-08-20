/*
 * aseqdump.c - show the events received at an ALSA sequencer port
 *
 * Copyright (c) 2005 Clemens Ladisch <clemens@ladisch.de>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "aconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <poll.h>
#include <alsa/asoundlib.h>
#include "version.h"
#include <alsa/ump_msg.h>

enum {
	VIEW_RAW, VIEW_NORMALIZED, VIEW_PERCENT
};

static snd_seq_t *seq;
static int port_count;
static snd_seq_addr_t *ports;
static volatile sig_atomic_t stop = 0;
static int ump_version;
static int view_mode = VIEW_RAW;

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

static void init_seq(void)
{
	int err;

	/* open sequencer */
	err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	check_snd("open sequencer", err);

	/* set our client's name */
	err = snd_seq_set_client_name(seq, "aseqdump");
	check_snd("set client name", err);
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
		ports = realloc(ports, port_count * sizeof(snd_seq_addr_t));
		check_mem(ports);

		err = snd_seq_parse_address(seq, &ports[port_count - 1], port_name);
		if (err < 0)
			fatal("Invalid port %s - %s", port_name, snd_strerror(err));
	}

	free(buf);
}

static void create_port(void)
{
	int err;

	err = snd_seq_create_simple_port(seq, "aseqdump",
					 SND_SEQ_PORT_CAP_WRITE |
					 SND_SEQ_PORT_CAP_SUBS_WRITE,
					 SND_SEQ_PORT_TYPE_MIDI_GENERIC |
					 SND_SEQ_PORT_TYPE_APPLICATION);
	check_snd("create port", err);
}

static void connect_ports(void)
{
	int i, err;

	for (i = 0; i < port_count; ++i) {
		err = snd_seq_connect_from(seq, 0, ports[i].client, ports[i].port);
		if (err < 0)
			fatal("Cannot connect from port %d:%d - %s",
			      ports[i].client, ports[i].port, snd_strerror(err));
	}
}

static int channel_number(unsigned char c)
{
	if (view_mode != VIEW_RAW)
		return c + 1;
	else
		return c;
}

static const char *midi1_data(unsigned int v)
{
	static char tmp[32];

	if (view_mode == VIEW_PERCENT) {
		if (v <= 64)
			snprintf(tmp, sizeof(tmp), "%.2f%%",
				 ((double)v * 50.0) / 64);
		else
			snprintf(tmp, sizeof(tmp), "%.2f%%",
				 ((double)(v - 64) * 50.0) / 63 + 50.0);
		return tmp;
	}

	sprintf(tmp, "%d", v);
	return tmp;
}

static const char *midi1_pitchbend(int v)
{
	static char tmp[32];

	if (view_mode == VIEW_PERCENT) {
		if (v < 0)
			snprintf(tmp, sizeof(tmp), "%.2f%%",
				 ((double)v * 100.0) / 8192);
		else
			snprintf(tmp, sizeof(tmp), "%.2f%%",
				 ((double)v * 100.0) / 8191);
		return tmp;
	}

	sprintf(tmp, "%d", v);
	return tmp;
}

static void dump_event(const snd_seq_event_t *ev)
{
	printf("%3d:%-3d ", ev->source.client, ev->source.port);

	switch (ev->type) {
	case SND_SEQ_EVENT_NOTEON:
		if (ev->data.note.velocity)
			printf("Note on                %2d, note %d, velocity %s\n",
			       channel_number(ev->data.note.channel),
			       ev->data.note.note,
			       midi1_data(ev->data.note.velocity));
		else
			printf("Note off               %2d, note %d\n",
			       channel_number(ev->data.note.channel),
			       ev->data.note.note);
		break;
	case SND_SEQ_EVENT_NOTEOFF:
		printf("Note off               %2d, note %d, velocity %s\n",
		       channel_number(ev->data.note.channel),
		       ev->data.note.note,
		       midi1_data(ev->data.note.velocity));
		break;
	case SND_SEQ_EVENT_KEYPRESS:
		printf("Polyphonic aftertouch  %2d, note %d, value %s\n",
		       channel_number(ev->data.note.channel),
		       ev->data.note.note,
		       midi1_data(ev->data.note.velocity));
		break;
	case SND_SEQ_EVENT_CONTROLLER:
		printf("Control change         %2d, controller %d, value %d\n",
		       channel_number(ev->data.control.channel),
		       ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_PGMCHANGE:
		printf("Program change         %2d, program %d\n",
		       channel_number(ev->data.control.channel),
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_CHANPRESS:
		printf("Channel aftertouch     %2d, value %s\n",
		       channel_number(ev->data.control.channel),
		       midi1_data(ev->data.control.value));
		break;
	case SND_SEQ_EVENT_PITCHBEND:
		printf("Pitch bend             %2d, value %s\n",
		       channel_number(ev->data.control.channel),
		       midi1_pitchbend(ev->data.control.value));
		break;
	case SND_SEQ_EVENT_CONTROL14:
		printf("Control change         %2d, controller %d, value %5d\n",
		       channel_number(ev->data.control.channel),
		       ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_NONREGPARAM:
		printf("Non-reg. parameter     %2d, parameter %d, value %d\n",
		       channel_number(ev->data.control.channel),
		       ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_REGPARAM:
		printf("Reg. parameter         %2d, parameter %d, value %d\n",
		       channel_number(ev->data.control.channel),
		       ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_SONGPOS:
		printf("Song position pointer      value %d\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_SONGSEL:
		printf("Song select                value %d\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_QFRAME:
		printf("MTC quarter frame          %02xh\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_TIMESIGN:
		// XXX how is this encoded?
		printf("SMF time signature         (%#010x)\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_KEYSIGN:
		// XXX how is this encoded?
		printf("SMF key signature          (%#010x)\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_START:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			printf("Queue start                queue %d\n",
			       ev->data.queue.queue);
		else
			printf("Start\n");
		break;
	case SND_SEQ_EVENT_CONTINUE:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			printf("Queue continue             queue %d\n",
			       ev->data.queue.queue);
		else
			printf("Continue\n");
		break;
	case SND_SEQ_EVENT_STOP:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			printf("Queue stop                 queue %d\n",
			       ev->data.queue.queue);
		else
			printf("Stop\n");
		break;
	case SND_SEQ_EVENT_SETPOS_TICK:
		printf("Set tick queue pos.        queue %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_SETPOS_TIME:
		printf("Set rt queue pos.          queue %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_TEMPO:
		printf("Set queue tempo            queue %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_CLOCK:
		printf("Clock\n");
		break;
	case SND_SEQ_EVENT_TICK:
		printf("Tick\n");
		break;
	case SND_SEQ_EVENT_QUEUE_SKEW:
		printf("Queue timer skew           queue %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_TUNE_REQUEST:
		printf("Tune request\n");
		break;
	case SND_SEQ_EVENT_RESET:
		printf("Reset\n");
		break;
	case SND_SEQ_EVENT_SENSING:
		printf("Active Sensing\n");
		break;
	case SND_SEQ_EVENT_CLIENT_START:
		printf("Client start               client %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_CLIENT_EXIT:
		printf("Client exit                client %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_CLIENT_CHANGE:
		printf("Client changed             client %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_PORT_START:
		printf("Port start                 %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_EXIT:
		printf("Port exit                  %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_CHANGE:
		printf("Port changed               %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_SUBSCRIBED:
		printf("Port subscribed            %d:%d -> %d:%d\n",
		       ev->data.connect.sender.client, ev->data.connect.sender.port,
		       ev->data.connect.dest.client, ev->data.connect.dest.port);
		break;
	case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
		printf("Port unsubscribed          %d:%d -> %d:%d\n",
		       ev->data.connect.sender.client, ev->data.connect.sender.port,
		       ev->data.connect.dest.client, ev->data.connect.dest.port);
		break;
	case SND_SEQ_EVENT_SYSEX:
		{
			unsigned int i;
			printf("System exclusive          ");
			for (i = 0; i < ev->data.ext.len; ++i)
				printf(" %02X", ((unsigned char*)ev->data.ext.ptr)[i]);
			printf("\n");
		}
		break;
	default:
		printf("Event type %d\n",  ev->type);
	}
}

static int group_number(unsigned char c)
{
	if (view_mode != VIEW_RAW)
		return c + 1;
	else
		return c;
}

static const char *pitchbend_value(uint8_t msb, uint8_t lsb)
{
	int pb = (msb << 7) | lsb;

	return midi1_pitchbend(pb - 8192);
}

static void dump_ump_midi1_event(const unsigned int *ump)
{
	const snd_ump_msg_midi1_t *m = (const snd_ump_msg_midi1_t *)ump;
	unsigned char group = group_number(m->hdr.group);
	unsigned char status = m->hdr.status;
	unsigned char channel = channel_number(m->hdr.channel);

	printf("Group %2d, ", group);
	switch (status) {
	case SND_UMP_MSG_NOTE_OFF:
		printf("Note off               %2d, note %d, velocity %s",
		       channel, m->note_off.note,
		       midi1_data(m->note_off.velocity));
		break;
	case SND_UMP_MSG_NOTE_ON:
		printf("Note on                %2d, note %d, velocity %s",
		       channel, m->note_off.note,
		       midi1_data(m->note_off.velocity));
		break;
	case SND_UMP_MSG_POLY_PRESSURE:
		printf("Poly pressure          %2d, note %d, value %s",
		       channel, m->poly_pressure.note,
		       midi1_data(m->poly_pressure.data));
		break;
	case SND_UMP_MSG_CONTROL_CHANGE:
		printf("Control change         %2d, controller %d, value %d",
		       channel, m->control_change.index, m->control_change.data);
		break;
	case SND_UMP_MSG_PROGRAM_CHANGE:
		printf("Program change         %2d, program %d",
		       channel, m->program_change.program);
		break;
	case SND_UMP_MSG_CHANNEL_PRESSURE:
		printf("Channel pressure       %2d, value %s",
		       channel, midi1_data(m->channel_pressure.data));
		break;
	case SND_UMP_MSG_PITCHBEND:
		printf("Pitchbend              %2d, value %s",
		       channel, pitchbend_value(m->pitchbend.data_msb,
						m->pitchbend.data_lsb));
		break;
	default:
		printf("UMP MIDI1 event: status = %d, channel = %d, 0x%08x",
		       status, channel, *ump);
		break;
	}
	printf("\n");
}

static const char *midi2_velocity(unsigned int v)
{
	static char tmp[32];

	if (view_mode == VIEW_NORMALIZED) {
		if (v <= 0x8000)
			snprintf(tmp, sizeof(tmp), "%.2f",
				 ((double)v * 64.0) / 0x8000);
		else
			snprintf(tmp, sizeof(tmp), "%.2f",
				 ((double)(v - 0x8000) * 63.0) / 0x7fff + 64.0);
		return tmp;
	} else if (view_mode == VIEW_PERCENT) {
		snprintf(tmp, sizeof(tmp), "%.2f%%", ((double)v * 100.0) / 0xffff);
		return tmp;
	}

	sprintf(tmp, "0x%x", v);
	return tmp;
}

static const char *midi2_data(unsigned int v)
{
	static char tmp[32];

	if (view_mode == VIEW_NORMALIZED) {
		if (!v)
			return "0";
		else if (v == 0xffffffffU)
			return "127";
		if (v <= 0x80000000)
			snprintf(tmp, sizeof(tmp), "%.2f",
				 ((double)v * 64.0) / 0x80000000U);
		else
			snprintf(tmp, sizeof(tmp), "%.2f",
				 ((double)(v - 0x80000000U) * 63.0) / 0x7fffffffU + 64.0);
		return tmp;
	} else if (view_mode == VIEW_PERCENT) {
		snprintf(tmp, sizeof(tmp), "%.2f%%", ((double)v * 100.0) / 0xffffffffU);
		return tmp;
	}

	sprintf(tmp, "0x%x", v);
	return tmp;
}

static const char *midi2_pitchbend(unsigned int v)
{
	static char tmp[32];

	if (view_mode == VIEW_NORMALIZED) {
		if (!v)
			return "-8192";
		else if (v == 0xffffffffU)
			return "8191";
		if (v <= 0x80000000)
			snprintf(tmp, sizeof(tmp), "%.2f",
				 ((int)(v ^ 0x80000000U) * 8192.0) / 0x80000000U);
		else
			snprintf(tmp, sizeof(tmp), "%.2f",
				 ((double)(v - 0x80000000U) * 8191.0) / 0x7fffffffU + 8192.0);
		return tmp;
	} else if (view_mode == VIEW_PERCENT) {
		snprintf(tmp, sizeof(tmp), "%.2f%%", ((int)(v ^ 0x80000000U) * 100.0) / 0xffffffffU);
		return tmp;
	}

	sprintf(tmp, "0x%x", v);
	return tmp;
}

static void dump_ump_midi2_event(const unsigned int *ump)
{
	const snd_ump_msg_midi2_t *m = (const snd_ump_msg_midi2_t *)ump;
	unsigned char group = group_number(m->hdr.group);
	unsigned char status = m->hdr.status;
	unsigned char channel = channel_number(m->hdr.channel);

	printf("Group %2d, ", group);
	switch (status) {
	case SND_UMP_MSG_PER_NOTE_RCC:
		printf("Per-note RCC           %2d, note %u, index %u, value 0x%x",
		       channel, m->per_note_rcc.note,
		       m->per_note_rcc.index, m->per_note_rcc.data);
		break;
	case SND_UMP_MSG_PER_NOTE_ACC:
		printf("Per-note ACC           %2d, note %u, index %u, value 0x%x",
		       channel, m->per_note_acc.note,
		       m->per_note_acc.index, m->per_note_acc.data);
		break;
	case SND_UMP_MSG_RPN:
		printf("RPN                    %2d, bank %u:%u, value 0x%x",
		       channel, m->rpn.bank, m->rpn.index, m->rpn.data);
		break;
	case SND_UMP_MSG_NRPN:
		printf("NRPN                   %2d, bank %u:%u, value 0x%x",
		       channel, m->rpn.bank, m->rpn.index, m->rpn.data);
		break;
	case SND_UMP_MSG_RELATIVE_RPN:
		printf("relative RPN           %2d, bank %u:%u, value 0x%x",
		       channel, m->rpn.bank, m->rpn.index, m->rpn.data);
		break;
	case SND_UMP_MSG_RELATIVE_NRPN:
		printf("relative NRP           %2d, bank %u:%u, value 0x%x",
		       channel, m->rpn.bank, m->rpn.index, m->rpn.data);
		break;
	case SND_UMP_MSG_PER_NOTE_PITCHBEND:
		printf("Per-note pitchbend     %2d, note %d, value %s",
		       channel, m->per_note_pitchbend.note,
		       midi2_pitchbend(m->per_note_pitchbend.data));
		break;
	case SND_UMP_MSG_NOTE_OFF:
		printf("Note off               %2d, note %d, velocity %s, attr type = %d, data = 0x%x",
		       channel, m->note_off.note,
		       midi2_velocity(m->note_off.velocity),
		       m->note_off.attr_type, m->note_off.attr_data);
		break;
	case SND_UMP_MSG_NOTE_ON:
		printf("Note on                %2d, note %d, velocity %s, attr type = %d, data = 0x%x",
		       channel, m->note_off.note,
		       midi2_velocity(m->note_off.velocity),
		       m->note_off.attr_type, m->note_off.attr_data);
		break;
	case SND_UMP_MSG_POLY_PRESSURE:
		printf("Poly pressure          %2d, note %d, value %s",
		       channel, m->poly_pressure.note,
		       midi2_data(m->poly_pressure.data));
		break;
	case SND_UMP_MSG_CONTROL_CHANGE:
		printf("Control change         %2d, controller %d, value 0x%x",
		       channel, m->control_change.index, m->control_change.data);
		break;
	case SND_UMP_MSG_PROGRAM_CHANGE:
		printf("Program change         %2d, program %d",
		       channel, m->program_change.program);
		if (m->program_change.bank_valid)
			printf(", Bank select %d:%d",
			       m->program_change.bank_msb,
			       m->program_change.bank_lsb);
		break;
	case SND_UMP_MSG_CHANNEL_PRESSURE:
		printf("Channel pressure       %2d, value %s",
		       channel,
		       midi2_data(m->channel_pressure.data));
		break;
	case SND_UMP_MSG_PITCHBEND:
		printf("Channel pressure       %2d, value %s",
		       channel,
		       midi2_pitchbend(m->channel_pressure.data));
		break;
	case SND_UMP_MSG_PER_NOTE_MGMT:
		printf("Per-note management    %2d, value 0x%x",
		       channel, m->per_note_mgmt.flags);
		break;
	default:
		printf("UMP MIDI2 event: status = %d, channel = %d, 0x%08x",
		       status, channel, *ump);
		break;
	}
	printf("\n");
}

static void dump_ump_utility_event(const unsigned int *ump)
{
	unsigned char status = snd_ump_msg_status(ump);
	unsigned int val = *ump & 0xfffff;

	printf("          ");
	switch (status) {
	case SND_UMP_UTILITY_MSG_STATUS_NOOP:
		printf("Noop\n");
		break;
	case SND_UMP_UTILITY_MSG_STATUS_JR_CLOCK:
		printf("JR Clock               value %d\n", val);
		break;
	case SND_UMP_UTILITY_MSG_STATUS_JR_TSTAMP:
		printf("JR Timestamp           value %d\n", val);
		break;
	case SND_UMP_UTILITY_MSG_STATUS_DCTPQ:
		printf("DCTPQ                  value %d\n", val);
		break;
	case SND_UMP_UTILITY_MSG_STATUS_DC:
		printf("DC Ticks               value %d\n", val);
		break;
	default:
		printf("UMP Utility event: status = %d, 0x%08x\n",
		       status, *ump);
		break;
	}
}

static void dump_ump_system_event(const unsigned int *ump)
{
	const snd_ump_msg_system_t *m = (const snd_ump_msg_system_t *)ump;

	printf("Group %2d, ", group_number(m->group));
	switch (m->status) {
	case SND_UMP_MSG_MIDI_TIME_CODE:
		printf("MIDI Time Code         value %d\n", m->parm1);
		break;
	case SND_UMP_MSG_SONG_POSITION:
		printf("Song position pointer  value %d\n",
		       ((unsigned int)m->parm2 << 7) | m->parm1);
		break;
	case SND_UMP_MSG_SONG_SELECT:
		printf("Song select            value %d\n", m->parm1);
		break;
	case SND_UMP_MSG_TUNE_REQUEST:
		printf("Tune request\n");
		break;
	case SND_UMP_MSG_TIMING_CLOCK:
		printf("Timing clock\n");
		break;
	case SND_UMP_MSG_START:
		printf("Start\n");
		break;
	case SND_UMP_MSG_CONTINUE:
		printf("Continue\n");
		break;
	case SND_UMP_MSG_STOP:
		printf("Stop\n");
		break;
	case SND_UMP_MSG_ACTIVE_SENSING:
		printf("Active sensing\n");
		break;
	case SND_UMP_MSG_RESET:
		printf("Reset\n");
		break;
	default:
		printf("UMP System event: status = %d, 0x%08x\n",
		       m->status, *ump);
		break;
	}
}

static unsigned char ump_sysex7_data(const unsigned int *ump,
				     unsigned int offset)
{
	return snd_ump_get_byte(ump, offset + 2);
}

static void dump_ump_sysex_status(const char *prefix, unsigned int status)
{
	printf("%s ", prefix);
	switch (status) {
	case SND_UMP_SYSEX_STATUS_SINGLE:
		printf("Single  ");
		break;
	case SND_UMP_SYSEX_STATUS_START:
		printf("Start   ");
		break;
	case SND_UMP_SYSEX_STATUS_CONTINUE:
		printf("Continue");
		break;
	case SND_UMP_SYSEX_STATUS_END:
		printf("End     ");
		break;
	default:
		printf("(0x%04x)", status);
		break;
	}
}

static void dump_ump_sysex_event(const unsigned int *ump)
{
	int i, length;

	printf("Group %2d, ", group_number(snd_ump_msg_group(ump)));
	dump_ump_sysex_status("SysEx", snd_ump_sysex_msg_status(ump));
	length = snd_ump_sysex_msg_length(ump);
	printf(" length %d ", length);
	if (length > 6)
		length = 6;
	for (i = 0; i < length; i++)
		printf("%s%02x", i ? ":" : "", ump_sysex7_data(ump, i));
	printf("\n");
}

static unsigned char ump_sysex8_data(const unsigned int *ump,
				     unsigned int offset)
{
	return snd_ump_get_byte(ump, offset + 3);
}

static void dump_ump_sysex8_event(const unsigned int *ump)
{
	int i, length;

	printf("Group %2d, ", group_number(snd_ump_msg_group(ump)));
	dump_ump_sysex_status("SysEx8", snd_ump_sysex_msg_status(ump));
	length = snd_ump_sysex_msg_length(ump);
	printf(" length %d ", length);
	printf(" stream %d ", (ump[0] >> 8) & 0xff);
	if (length > 13)
		length = 13;
	for (i = 0; i < length; i++)
		printf("%s%02x", i ? ":" : "", ump_sysex8_data(ump, i));
	printf("\n");
}

static void dump_ump_mixed_data_event(const unsigned int *ump)
{
	const snd_ump_msg_mixed_data_t *m =
		(const snd_ump_msg_mixed_data_t *)ump;
	int i;

	printf("Group %2d, ", group_number(snd_ump_msg_group(ump)));
	switch (snd_ump_sysex_msg_status(ump)) {
	case SND_UMP_MIXED_DATA_SET_STATUS_HEADER:
		printf("MDS Header id=0x%x, bytes=%d, chunk=%d/%d, manufacturer=0x%04x, device=0x%04x, sub_id=0x%04x 0x%04x\n",
		       m->header.mds_id, m->header.bytes,
		       m->header.chunk, m->header.chunks,
		       m->header.manufacturer, m->header.device,
		       m->header.sub_id_1, m->header.sub_id_2);
		break;
	case SND_UMP_MIXED_DATA_SET_STATUS_PAYLOAD:
		printf("MDS Payload id=0x%x, ", m->payload.mds_id);
		for (i = 0; i < 14; i++)
			printf("%s%02x", i ? ":" : "",
			       snd_ump_get_byte(ump, i + 2));
		printf("\n");
		break;
	default:
		printf("Extended Data (status 0x%x)\n",
		       snd_ump_sysex_msg_status(ump));
		break;
	}
}

static void dump_ump_extended_data_event(const unsigned int *ump)
{
	unsigned char status = snd_ump_sysex_msg_status(ump);

	if (status < 4)
		dump_ump_sysex8_event(ump);
	else
		dump_ump_mixed_data_event(ump);
}

static void print_ump_string(const unsigned int *ump, unsigned int fmt,
			     unsigned int offset, int maxlen)
{
	static const char *fmtstr[4] = { "Single", "Start", "Cont", "End" };
	unsigned char buf[32];
	int i = 0;

	do {
		buf[i] = snd_ump_get_byte(ump, offset);
		if (!buf[i])
			break;
		if (buf[i] < 0x20)
			buf[i] = '.';
		offset++;
	} while (++i < maxlen);
	buf[i] = 0;

	printf("%6s: %s", fmtstr[fmt], buf);
}

static void dump_ump_stream_event(const unsigned int *ump)
{
	const snd_ump_msg_stream_t *s = (const snd_ump_msg_stream_t *)ump;

	printf("          "); /* stream message is groupless */
	switch (s->gen.status) {
	case SND_UMP_STREAM_MSG_STATUS_EP_DISCOVERY:
		printf("EP Discovery    ver=%d/%d, filter=0x%x\n",
		       (ump[0] >> 8) & 0xff, ump[0] & 0xff, ump[1] & 0xff);
		break;
	case SND_UMP_STREAM_MSG_STATUS_EP_INFO:
		printf("EP Info         ver=%d/%d, static=%d, fb#=%d, midi2=%d, midi1=%d, rxjr=%d, txjr=%d\n",
		       (ump[0] >> 8) & 0xff, ump[0] & 0xff, (ump[1] >> 31),
		       (ump[1] >> 24) & 0x7f,
		       (ump[1] >> 9) & 1, (ump[1] >> 8) & 1,
		       (ump[1] >> 1) & 1, ump[1] & 1);
		break;
	case SND_UMP_STREAM_MSG_STATUS_DEVICE_INFO:
		printf("Device Info     sysid=%02x:%02x:%02x, family=%02x:%02x, model=%02x:%02x, rev=%02x:%02x:%02x:%02x\n",
		       (ump[1] >> 16) & 0x7f, (ump[1] >> 8) & 0x7f, ump[1] & 0x7f,
		       (ump[2] >> 16) & 0x7f, (ump[2] >> 24) & 0x7f,
		       ump[2] & 0x7f, (ump[2] >> 8) & 0x7f,
		       (ump[3] >> 24) & 0x7f, (ump[3] >> 16) & 0x7f,
		       (ump[3] >> 8) & 0x7f, ump[3] & 0x7f);
		break;
	case SND_UMP_STREAM_MSG_STATUS_EP_NAME:
		printf("EP Name        ");
		print_ump_string(ump, (ump[0] >> 26) & 3, 2, 14);
		printf("\n");
		break;
	case SND_UMP_STREAM_MSG_STATUS_PRODUCT_ID:
		printf("Product Id     ");
		print_ump_string(ump, (ump[0] >> 26) & 3, 2, 14);
		printf("\n");
		break;
	case SND_UMP_STREAM_MSG_STATUS_STREAM_CFG_REQUEST:
		printf("Stream Cfg Req protocl=%d, rxjr=%d, txjr=%d\n",
		       (ump[0] >> 8) & 0xff, (ump[0] >> 1) & 1, ump[0] & 1);
		break;
	case SND_UMP_STREAM_MSG_STATUS_STREAM_CFG:
		printf("Stream Cfg     protocl=%d, rxjr=%d, txjr=%d\n",
		       (ump[0] >> 8) & 0xff, (ump[0] >> 1) & 1, ump[0] & 1);
		break;
	case SND_UMP_STREAM_MSG_STATUS_FB_DISCOVERY:
		printf("FB Discovery   fb#=%d, filter=0x%x\n",
		       (ump[0] >> 8) & 0xff, ump[0] & 0xff);
		break;
	case SND_UMP_STREAM_MSG_STATUS_FB_INFO:
		printf("FB Info        fb#=%d, active=%d, ui=%d, MIDI1=%d, dir=%d, group=%d-%d, MIDI-CI=%d, SysEx8=%d\n",
		       (ump[0] >> 8) & 0x7f, (ump[0] >> 15) & 1,
		       (ump[0] >> 4) & 3, (ump[0] >> 2) & 3, ump[0] & 3,
		       (ump[1] >> 24) & 0xff, (ump[1] >> 16) & 0xff,
		       (ump[1] >> 8) * 0xff, ump[1] & 0xff);
		break;
	case SND_UMP_STREAM_MSG_STATUS_FB_NAME:
		printf("Product Id     ");
		printf("FB Name #%02d    ", (ump[0] >> 8) & 0xff);
		print_ump_string(ump, (ump[0] >> 26) & 3, 3, 13);
		printf("\n");
		break;
	case SND_UMP_STREAM_MSG_STATUS_START_CLIP:
		printf("Start Clip\n");
		break;
	case SND_UMP_STREAM_MSG_STATUS_END_CLIP:
		printf("End Clip\n");
		break;
	default:
		printf("UMP Stream event: status = %d, 0x%08x:0x%08x:0x%08x:0x%08x\n",
		       s->gen.status, ump[0], ump[1], ump[2], ump[3]);
		break;
	}
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

static const char *ump_meta_prefix(const snd_ump_msg_flex_data_t *fh)
{
	static char buf[32];
	int i;

	for (i = 0; text_prefix[i].status_bank; i++) {
		if (text_prefix[i].status_bank == fh->meta.status_bank &&
		    text_prefix[i].status == fh->meta.status)
			return text_prefix[i].prefix;
	}

	sprintf(buf, "(%d:%d)", fh->meta.status_bank, fh->meta.status);
	return buf;
}

static void dump_ump_flex_data_event(const unsigned int *ump)
{
	const snd_ump_msg_flex_data_t *fh =
		(const snd_ump_msg_flex_data_t *)ump;

	printf("Group %2d, ", group_number(snd_ump_msg_group(ump)));

	if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_SETUP &&
	    fh->meta.status == SND_UMP_FLEX_DATA_MSG_STATUS_SET_TEMPO) {
		printf("UMP Set Tempo          value %d\n", fh->set_tempo.tempo);
		return;
	}

	if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_SETUP &&
	    fh->meta.status == SND_UMP_FLEX_DATA_MSG_STATUS_SET_TIME_SIGNATURE) {
		printf("UMP Set Time Signature value %d / %d, num_notes %d\n",
		       fh->set_time_sig.numerator, fh->set_time_sig.denominator,
		       fh->set_time_sig.num_notes);
		return;
	}

	if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_SETUP &&
	    fh->meta.status == SND_UMP_FLEX_DATA_MSG_STATUS_SET_METRONOME) {
		printf("UMP Set Metronome      clock %d, bar %d/%d/%d, sub %d/%d\n",
		       fh->set_metronome.clocks_primary,
		       fh->set_metronome.bar_accent_1,
		       fh->set_metronome.bar_accent_2,
		       fh->set_metronome.bar_accent_3,
		       fh->set_metronome.subdivision_1,
		       fh->set_metronome.subdivision_2);
		return;
	}

	if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_SETUP &&
	    fh->meta.status == SND_UMP_FLEX_DATA_MSG_STATUS_SET_KEY_SIGNATURE) {
		printf("UMP Set Key Signature     sharps/flats %d, tonic %d\n",
		       fh->set_key_sig.sharps_flats,
		       fh->set_key_sig.tonic_note);
		return;
	}

	if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_SETUP &&
	    fh->meta.status == SND_UMP_FLEX_DATA_MSG_STATUS_SET_CHORD_NAME) {
		printf("UMP Set Chord Name     tonic %d %d %d, alt1 %d/%d, alt2 %d/%d, alt3 %d/%d, alt4 %d/%d, bass %d %d %d, alt1 %d/%d alt2 %d/%d\n",
		       fh->set_chord_name.tonic_sharp,
		       fh->set_chord_name.chord_tonic,
		       fh->set_chord_name.chord_type,
		       fh->set_chord_name.alter1_type,
		       fh->set_chord_name.alter1_degree,
		       fh->set_chord_name.alter2_type,
		       fh->set_chord_name.alter2_degree,
		       fh->set_chord_name.alter3_type,
		       fh->set_chord_name.alter3_degree,
		       fh->set_chord_name.alter4_type,
		       fh->set_chord_name.alter4_degree,
		       fh->set_chord_name.bass_sharp,
		       fh->set_chord_name.bass_note,
		       fh->set_chord_name.bass_type,
		       fh->set_chord_name.bass_alter1_type,
		       fh->set_chord_name.bass_alter1_type,
		       fh->set_chord_name.bass_alter2_degree,
		       fh->set_chord_name.bass_alter2_degree);
		return;
	}

	if (fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_METADATA ||
	    fh->meta.status_bank == SND_UMP_FLEX_DATA_MSG_BANK_PERF_TEXT) {
		printf("Meta (%s) ", ump_meta_prefix(fh));
		print_ump_string(ump, fh->meta.format, 4, 12);
		printf("\n");
		return;
	}

	printf("Flex Data: channel = %d, format = %d, addrs = %d, status_bank = %d, status = %d\n",
	       fh->meta.channel, fh->meta.format, fh->meta.addrs,
	       fh->meta.status_bank, fh->meta.status);
}

static void dump_ump_event(const snd_seq_ump_event_t *ev)
{
	if (!snd_seq_ev_is_ump(ev)) {
		dump_event((const snd_seq_event_t *)ev);
		return;
	}

	printf("%3d:%-3d ", ev->source.client, ev->source.port);

	switch (snd_ump_msg_type(ev->ump)) {
	case SND_UMP_MSG_TYPE_UTILITY:
		dump_ump_utility_event(ev->ump);
		break;
	case SND_UMP_MSG_TYPE_SYSTEM:
		dump_ump_system_event(ev->ump);
		break;
	case SND_UMP_MSG_TYPE_MIDI1_CHANNEL_VOICE:
		dump_ump_midi1_event(ev->ump);
		break;
	case SND_UMP_MSG_TYPE_MIDI2_CHANNEL_VOICE:
		dump_ump_midi2_event(ev->ump);
		break;
	case SND_UMP_MSG_TYPE_DATA:
		dump_ump_sysex_event(ev->ump);
		break;
	case SND_UMP_MSG_TYPE_EXTENDED_DATA:
		dump_ump_extended_data_event(ev->ump);
		break;
	case SND_UMP_MSG_TYPE_FLEX_DATA:
		dump_ump_flex_data_event(ev->ump);
		break;
	case SND_UMP_MSG_TYPE_STREAM:
		dump_ump_stream_event(ev->ump);
		break;
	default:
		printf("UMP event: type = %d, group = %d, status = %d, 0x%08x\n",
		       snd_ump_msg_type(ev->ump),
		       snd_ump_msg_group(ev->ump),
		       snd_ump_msg_status(ev->ump),
		       *ev->ump);
		break;
	}
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
			/* we need both READ and SUBS_READ */
			if ((snd_seq_port_info_get_capability(pinfo)
			     & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
			    != (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
				continue;
			printf("%3d:%-3d  %-32.32s %s\n",
			       snd_seq_port_info_get_client(pinfo),
			       snd_seq_port_info_get_port(pinfo),
			       snd_seq_client_info_get_name(cinfo),
			       snd_seq_port_info_get_name(pinfo));
		}
	}
}

static void help(const char *argv0)
{
	printf("Usage: %s [options]\n"
		"\nAvailable options:\n"
		"  -h,--help                  this help\n"
		"  -V,--version               show version\n"
		"  -l,--list                  list input ports\n"
		"  -N,--normalized-view       show normalized values\n"
		"  -P,--percent-view          show percent values\n"
		"  -R,--raw-view              show raw values (default)\n"
		"  -u,--ump=version           set client MIDI version (0=legacy, 1= UMP MIDI 1.0, 2=UMP MIDI2.0)\n"
		"  -r,--raw                   do not convert UMP and legacy events\n"
		"  -p,--port=client:port,...  source port(s)\n",
		argv0);
}

static void version(void)
{
	puts("aseqdump version " SND_UTIL_VERSION_STR);
}

static void sighandler(int sig ATTRIBUTE_UNUSED)
{
	stop = 1;
}

int main(int argc, char *argv[])
{
	static const char short_options[] = "hVlp:NPRu:r";
	static const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'V'},
		{"list", 0, NULL, 'l'},
		{"port", 1, NULL, 'p'},
		{"normalized-view", 0, NULL, 'N'},
		{"percent-view", 0, NULL, 'P'},
		{"raw-view", 0, NULL, 'R'},
		{"ump", 1, NULL, 'u'},
		{"raw", 0, NULL, 'r'},
		{0}
	};

	int do_list = 0;
	struct pollfd *pfds;
	int npfds;
	int c, err;

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
		case 'l':
			do_list = 1;
			break;
		case 'p':
			parse_ports(optarg);
			break;
		case 'R':
			view_mode = VIEW_RAW;
			break;
		case 'P':
			view_mode = VIEW_PERCENT;
			break;
		case 'N':
			view_mode = VIEW_NORMALIZED;
			break;
		case 'u':
			ump_version = atoi(optarg);
			if (ump_version < 0 || ump_version > 2)
				fatal("Invalid UMP version %d", ump_version);
			snd_seq_set_client_midi_version(seq, ump_version);
			break;
		case 'r':
			snd_seq_set_client_ump_conversion(seq, 0);
			break;
		default:
			help(argv[0]);
			return 1;
		}
	}
	if (optind < argc) {
		help(argv[0]);
		return 1;
	}

	if (do_list) {
		list_ports();
		return 0;
	}

	create_port();
	connect_ports();

	err = snd_seq_nonblock(seq, 1);
	check_snd("set nonblock mode", err);
	
	if (port_count > 0)
		printf("Waiting for data.");
	else
		printf("Waiting for data at port %d:0.",
		       snd_seq_client_id(seq));
	printf(" Press Ctrl+C to end.\n");
	printf("Source  %sEvent                  Ch  Data\n",
	       ump_version ? "Group    " : "");
	
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
	pfds = alloca(sizeof(*pfds) * npfds);
	for (;;) {
		snd_seq_poll_descriptors(seq, pfds, npfds, POLLIN);
		if (poll(pfds, npfds, -1) < 0)
			break;
		for (;;) {
			snd_seq_event_t *event;
			snd_seq_ump_event_t *ump_ev;
			if (ump_version > 0) {
				err = snd_seq_ump_event_input(seq, &ump_ev);
				if (err < 0)
					break;
				if (ump_ev)
					dump_ump_event(ump_ev);
				continue;
			}

			err = snd_seq_event_input(seq, &event);
			if (err < 0)
				break;
			if (event)
				dump_event(event);
		}
		fflush(stdout);
		if (stop)
			break;
	}

	snd_seq_close(seq);
	return 0;
}
