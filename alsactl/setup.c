/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) 1997 by Perex, APS, University of South Bohemia
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

#include "alsactl.h"

#define SND_INTERFACE_CONTROL	0
#define SND_INTERFACE_MIXER	1
#define SND_INTERFACE_PCM	2
#define SND_INTERFACE_RAWMIDI	3

extern int yyparse(void);
extern int linecount;
extern FILE *yyin;
extern int yydebug;
struct soundcard *soundcards = NULL;
struct soundcard *rsoundcards = NULL;

/*
 *  misc functions
 */

char *control_id(snd_control_id_t *id)
{
	static char str[128];
	
	if (!id)
		return "???"; 
	sprintf(str, "%i,%i,%i,%s,%i", id->iface, id->device, id->subdevice, id->name, id->index);
	return str;
}

/*
 *  free functions
 */

static void soundcard_ctl_control_free(struct ctl_control *first)
{
	struct ctl_control *next;

	while (first) {
		next = first->next;
		free(first);
		first = next;
	}
}

static void soundcard_free1(struct soundcard *soundcard)
{
	if (!soundcard)
		return;
	soundcard_ctl_control_free(soundcard->control.controls);
	free(soundcard);
}

static void soundcard_free(struct soundcard *first)
{
	struct soundcard *next;

	while (first) {
		next = first->next;
		soundcard_free1(first);
		first = next;
	}
}

static int soundcard_remove(int cardno)
{
	struct soundcard *first, *prev = NULL, *next;

	first = soundcards;
	while (first) {
		next = first->next;
		if (first->no == cardno) {
			soundcard_free1(first);
			if (!prev)
				soundcards = next;
			else
				prev->next = next;
			return 0;
		}
		prev = first;
		first = first->next;
	}
	return -1;
}

/*
 *  exported functions
 */

void soundcard_setup_init(void)
{
	soundcards = NULL;
}

void soundcard_setup_done(void)
{
	soundcard_free(soundcards);
	soundcard_free(rsoundcards);
	soundcards = NULL;
}

static int determine_controls(void *handle, struct ctl_control **cctl)
{
	int err, idx;
	snd_control_list_t list;
	snd_control_id_t *item;
	snd_control_t ctl;
	struct ctl_control *prev_control;
	struct ctl_control *new_control;

	*cctl = NULL;
	bzero(&list, sizeof(list));
	if ((err = snd_ctl_clist(handle, &list)) < 0) {
		error("Cannot determine controls: %s", snd_strerror(err));
		return 1;
	}
	if (list.controls <= 0)
		return 0;
	list.controls_request = list.controls + 16;
	list.controls_offset = list.controls_count = 0;
	list.pids = malloc(sizeof(snd_control_id_t) * list.controls_request);
	if (!list.pids) {
		error("No enough memory...");
		return 1;
	}
	if ((err = snd_ctl_clist(handle, &list)) < 0) {
		error("Cannot determine controls (2): %s", snd_strerror(err));
		return 1;
	}
	for (idx = 0, prev_control = NULL; idx < list.controls_count; idx++) {
		item = &list.pids[idx];
		bzero(&ctl, sizeof(ctl));
		ctl.id = *item;
		if ((err = snd_ctl_cread(handle, &ctl)) < 0) {
			error("Cannot read control '%s': %s", control_id(item), snd_strerror(err));
			free(list.pids);
			return 1;
		}
		new_control = malloc(sizeof(*new_control));
		if (!new_control) {
			error("No enough memory...");
			free(list.pids);
			return 1;
		}
		bzero(new_control, sizeof(*new_control));
		memcpy(&new_control->c, &ctl, sizeof(new_control->c));
		new_control->info.id = ctl.id;
		if ((err = snd_ctl_cinfo(handle, &new_control->info)) < 0) {
			error("Cannot read control info '%s': %s", control_id(item), snd_strerror(err));
			free(new_control);
			free(list.pids);
			return 1;
		}
		if (*cctl) {
			prev_control->next = new_control;
			prev_control = new_control;
		} else {
			*cctl = prev_control = new_control;
		}
	}
	free(list.pids);
	return 0;
}

static int soundcard_setup_collect_controls1(int cardno)
{
	snd_ctl_t *handle;
	struct soundcard *card, *first, *prev;
	int err;

	soundcard_remove(cardno);
	if ((err = snd_ctl_open(&handle, cardno)) < 0) {
		error("SND CTL open error: %s", snd_strerror(err));
		return 1;
	}
	/* --- */
	card = (struct soundcard *) malloc(sizeof(struct soundcard));
	if (!card) {
		snd_ctl_close(handle);
		error("malloc error");
		return 1;
	}
	bzero(card, sizeof(struct soundcard));
	card->no = cardno;
	for (first = soundcards, prev = NULL; first; first = first->next) {
		if (first->no > cardno) {
			if (!prev) {
				soundcards = card;
			} else {
				prev->next = card;
			}
			card->next = first;
			break;
		}
		prev = first;
	}
	if (!first) {
		if (!soundcards) {
			soundcards = card;
		} else {
			prev->next = card;
		}
	}
	if ((err = snd_ctl_hw_info(handle, &card->control.hwinfo)) < 0) {
		snd_ctl_close(handle);
		error("SND CTL HW INFO error: %s", snd_strerror(err));
		return 1;
	}
	/* --- */
	if (determine_controls(handle, &card->control.controls)) {
		snd_ctl_close(handle);
		return 1;
	}        
	/* --- */
	snd_ctl_close(handle);
	return 0;
}

int soundcard_setup_collect_controls(int cardno)
{
	int err;
	unsigned int mask;

	if (cardno >= 0) {
		return soundcard_setup_collect_controls1(cardno);
	} else {
		mask = snd_cards_mask();
		for (cardno = 0; cardno < SND_CARDS; cardno++) {
			if (!(mask & (1 << cardno)))
				continue;
			err = soundcard_setup_collect_controls1(cardno);
			if (err)
				return err;
		}
		return 0;
	}
}

int soundcard_setup_load(const char *cfgfile, int skip)
{
	extern int yyparse(void);
	extern int linecount;
	extern FILE *yyin;
#ifdef YYDEBUG
	extern int yydebug;
#endif
	int xtry;

#ifdef YYDEBUG
	yydebug = 1;
#endif
	if (debugflag)
		printf("cfgfile = '%s'\n", cfgfile);
	if (skip && access(cfgfile, R_OK))
		return 0;
	if ((yyin = fopen(cfgfile, "r")) == NULL) {
		error("Cannot open configuration file '%s'...", cfgfile);
		return 1;
	}
	linecount = 0;
	xtry = yyparse();
	fclose(yyin);
	if (debugflag)
		printf("Config ok..\n");
	if (xtry)
		error("Ignored error in configuration file '%s'...", cfgfile);
	return 0;
}

static void soundcard_setup_write_control(FILE * out, const char *space, int card, struct ctl_control *control)
{
	char *s, v[16];
	int err, idx;
	snd_ctl_t *handle;
	snd_control_info_t info;

	memcpy(&info, &control->info, sizeof(info));
	v[0] = '\0';
	switch (info.type) {
	case SND_CONTROL_TYPE_BOOLEAN:	s = "bool";	break;
	case SND_CONTROL_TYPE_INTEGER:	s = "integer";	break;
	case SND_CONTROL_TYPE_ENUMERATED: s = "enumerated"; break;
	case SND_CONTROL_TYPE_BYTES: s = "bytes"; break;
	default: s = "unknown";
	}
	fprintf(out, "\n%s; The type is '%s'. Access:", space, s);
	if (info.access & SND_CONTROL_ACCESS_READ)
		fprintf(out, " read");
	if (info.access & SND_CONTROL_ACCESS_WRITE)
		fprintf(out, " write");
	if (info.access & SND_CONTROL_ACCESS_INACTIVE)
		fprintf(out, " inactive");
	fprintf(out, ". Count is %i.\n", info.values_count);
	switch (info.type) {
	case SND_CONTROL_TYPE_BOOLEAN:
		if (info.value.integer.min != 0 || info.value.integer.max != 1 ||
		    info.value.integer.step != 0)
			error("Wrong control '%s' (boolean)\n", control_id(&info.id));
		break;
	case SND_CONTROL_TYPE_INTEGER:
		fprintf(out, "%s;   The range is %li-%li (step %li)\n", space, info.value.integer.min, info.value.integer.max, info.value.integer.step);
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		if ((err = snd_ctl_open(&handle, card)) >= 0) {
			for (idx = 0; idx < info.value.enumerated.items; idx++) {
				info.value.enumerated.item = idx;
				if (snd_ctl_cinfo(handle, &info) >= 0)
					fprintf(out, "%s;   Item #%i - %s\n", space, idx, info.value.enumerated.name);
			}
			snd_ctl_close(handle);
		}
		break;
	default:
		break;
	}
	switch (info.id.iface) {
	case SND_CONTROL_IFACE_CARD: s = "global"; break;
	case SND_CONTROL_IFACE_HWDEP: s = "hwdep"; break;
	case SND_CONTROL_IFACE_MIXER: s = "mixer"; break;
	case SND_CONTROL_IFACE_PCM: s = "pcm"; break;
	case SND_CONTROL_IFACE_RAWMIDI: s = "rawmidi"; break;
	case SND_CONTROL_IFACE_TIMER: s = "timer"; break;
	case SND_CONTROL_IFACE_SEQUENCER: s = "sequencer"; break;
	default: sprintf(v, "%i", info.id.iface); s = v; break;
	}
	fprintf(out, "%scontrol(%s, %i, %i, \"%s\", %i", space, s, info.id.device, info.id.subdevice, info.id.name, info.id.index);
	if (info.type == SND_CONTROL_TYPE_BYTES)
		fprintf(out, "rawdata(@");
	for (idx = 0; idx < info.values_count; idx++) {
		switch (info.type) {
		case SND_CONTROL_TYPE_BOOLEAN:
			fprintf(out, ", %s", control->c.value.integer.value[idx] ? "true" : "false");
			break;
		case SND_CONTROL_TYPE_INTEGER:
			fprintf(out, ", %li", control->c.value.integer.value[idx]);
			break;
		case SND_CONTROL_TYPE_ENUMERATED:
			fprintf(out, ", %u", control->c.value.enumerated.item[idx]);
			break;
		case SND_CONTROL_TYPE_BYTES:
			if (idx > 0)
				fprintf(out, ":");
			fprintf(out, "%02x", control->c.value.bytes.data[idx]);
			break;
		default:
			break;
		}
	}		
	if (info.type == SND_CONTROL_TYPE_BYTES)
		fprintf(out, ")");
	fprintf(out, ")\n");
}

static void soundcard_setup_write_controls(FILE *out, const char *space, int card, struct ctl_control **controls)
{
	struct ctl_control *ctl;

	if (!(*controls))
		return;
	for (ctl = *controls; ctl; ctl = ctl->next)
		soundcard_setup_write_control(out, space, card, ctl);
}

#define MAX_LINE	(32 * 1024)

int soundcard_setup_write(const char *cfgfile, int cardno)
{
	FILE *out, *out1, *out2, *in;
	char *tmpfile1, *tmpfile2;
	struct soundcard *first, *sel = NULL;
	char *line, cardname[sizeof(first->control.hwinfo.name)+16], *ptr1;
	int mark, size, ok;

	tmpfile1 = (char *)malloc(strlen(cfgfile) + 16);
	tmpfile2 = (char *)malloc(strlen(cfgfile) + 16);
	if (!tmpfile1 || !tmpfile2) {
		error("No enough memory...\n");
		if (tmpfile1)
			free(tmpfile1);
		if (tmpfile2)
			free(tmpfile2);
		return 1;
	}
	strcpy(tmpfile1, cfgfile);
	strcat(tmpfile1, ".new");
	strcpy(tmpfile2, cfgfile);
	strcat(tmpfile2, ".insert");
	
	if (cardno >= 0) {
		line = (char *)malloc(MAX_LINE);
		if (!line) {
			error("No enough memory...\n");
			return 1;
		}
		if ((in = fopen(cfgfile, "r")) == NULL)
			cardno = -1;
	} else {
		line = NULL;
		in = NULL;
	}
	if ((out = out1 = fopen(tmpfile1, "w+")) == NULL) {
		error("Cannot open file '%s' for writing...\n", tmpfile1);
		return 1;
	}
	fprintf(out, "# ALSA driver configuration\n");
	fprintf(out, "# This configuration is generated with the alsactl program.\n");
	fprintf(out, "\n");
	if (cardno >= 0) {
		if ((out = out2 = fopen(tmpfile2, "w+")) == NULL) {
			error("Cannot open file '%s' for writing...\n", tmpfile2);
			return 1;
		}
	} else {
		out2 = NULL;
	}
	for (first = soundcards; first; first = first->next) {
		if (cardno >= 0 && first->no != cardno)
			continue;
		sel = first;
		fprintf(out, "soundcard(\"%s\") {\n", first->control.hwinfo.id);
		if (first->control.controls) {
			soundcard_setup_write_controls(out, "  ", first->no, &first->control.controls);
		}
		fprintf(out, "}\n%s", cardno < 0 && first->next ? "\n" : "");
	}
	/* merge the old and new text */
	if (cardno >= 0) {
		fseek(out2, 0, SEEK_SET);
		mark = ok = 0;
	      __1:
		while (fgets(line, MAX_LINE - 1, in)) {
			line[MAX_LINE - 1] = '\0';
			if (!strncmp(line, "soundcard(", 10))
				break;
		}
		while (!feof(in)) {
			ptr1 = line + 10;
			while (*ptr1 && *ptr1 != '"')
				ptr1++;
			if (*ptr1)
				ptr1++;
			strncpy(cardname, sel->control.hwinfo.id, sizeof(sel->control.hwinfo.id));
			cardname[sizeof(sel->control.hwinfo.id)] = '\0';
			strcat(cardname, "\"");
			if (!strncmp(ptr1, cardname, strlen(cardname))) {
				if (mark)
					fprintf(out1, "\n");
				do {
					size = fread(line, 1, MAX_LINE, out2);
					if (size > 0)
						fwrite(line, 1, size, out1);
				} while (size > 0);
				mark = ok = 1;
				goto __1;
			} else {
				if (mark)
					fprintf(out1, "\n");
				fprintf(out1, line);
				while (fgets(line, MAX_LINE - 1, in)) {
					line[MAX_LINE - 1] = '\0';
					fprintf(out1, line);
					if (line[0] == '}') {
						mark = 1;
						goto __1;
					}
				}
			}
		}
		if (!ok) {
			if (mark)
				fprintf(out1, "\n");
			do {
				size = fread(line, 1, MAX_LINE, out2);
				printf("size = %i\n", size);
				if (size > 0)
					fwrite(line, 1, size, out1);
			} while (size > 0);			
		}
	}
	if (in)
		fclose(in);
	if (out2)
		fclose(out2);
	if (!access(cfgfile, F_OK) && remove(cfgfile))
		error("Cannot remove file '%s'...", cfgfile);
	if (rename(tmpfile1, cfgfile) < 0)
		error("Cannot rename file '%s' to '%s'...", tmpfile1, cfgfile);
	fclose(out1);
	if (line)
		free(line);
	if (tmpfile2) {
		remove(tmpfile2);
		free(tmpfile2);
	}
	if (tmpfile1) {
		remove(tmpfile1);
		free(tmpfile1);
	}
	return 0;
}
