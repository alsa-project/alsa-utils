%{

/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) 1998 by Perex, APS, University of South Bohemia
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
#include <stdarg.h>

	/* insgus_lexer.c */

int yylex( void );

extern char cfgfile[];
extern int linecount;
extern FILE *yyin;

	/* structure for byte arrays */ 

struct bytearray {
  unsigned char *data;
  size_t datalen;
};

	/* local functions */

static void yyerror(char *, ...);

static void build_soundcard(char *name);
static void build_control_begin(int iface, unsigned int device, unsigned int subdevice, char *name, unsigned int index);
static void build_control_end(void);
static void set_control_boolean(int val);
static void set_control_integer(long val);
static void set_control_bytearray(struct bytearray val);

	/* local variables */

static struct soundcard *Xsoundcard = NULL;
static struct ctl_control *Xcontrol = NULL;
static int Xposition = 0;
static snd_control_type_t Xtype = SND_CONTROL_TYPE_NONE;

%}

%start lines

%union {
    int b_value;
    long i_value;
    char *s_value;
    struct bytearray a_value;
  };

%token <b_value> L_TRUE L_FALSE
%token <i_value> L_INTEGER
%token <s_value> L_STRING
%token <a_value> L_BYTEARRAY

	/* types */
%token L_INTEGER L_STRING
	/* boolean */
%token L_FALSE L_TRUE
	/* misc */
%token L_DOUBLE1
	/* other keywords */
%token L_SOUNDCARD L_CONTROL L_RAWDATA
%token L_GLOBAL L_HWDEP L_MIXER L_PCM L_RAWMIDI L_TIMER L_SEQUENCER


%type <b_value> boolean
%type <i_value> integer iface
%type <s_value> string
%type <a_value> rawdata

%%

lines	: line
	| lines line
	;

line	: L_SOUNDCARD '(' string { build_soundcard($3); }
	  L_DOUBLE1 soundcards '}' { build_soundcard(NULL); }
	| error			{ yyerror("unknown keyword in top level"); }
	;

soundcards :
	| soundcards soundcard
	;

soundcard : L_CONTROL '(' iface ',' integer ',' integer ',' string ',' integer
				{ build_control_begin($3, $5, $7, $9, $11); }
	',' controls ')'	{ build_control_end(); }
	| error			{ yyerror( "an unknown keyword in the soundcard{} level"); }
	;

controls : control
	| controls ',' control
	;

control : boolean		{ set_control_boolean($1); }
	| integer		{ set_control_integer($1); }
	| rawdata		{ set_control_bytearray($1); }
	| error			{ yyerror( "an unknown keyword in the control() data parameter" ); }
	;

iface	: L_INTEGER		{ $$ = $1; }
	| L_GLOBAL		{ $$ = SND_CONTROL_IFACE_CARD; }
	| L_HWDEP		{ $$ = SND_CONTROL_IFACE_HWDEP; }
	| L_MIXER		{ $$ = SND_CONTROL_IFACE_MIXER; }
	| L_PCM			{ $$ = SND_CONTROL_IFACE_PCM; }
	| L_RAWMIDI		{ $$ = SND_CONTROL_IFACE_RAWMIDI; }
	| L_TIMER		{ $$ = SND_CONTROL_IFACE_TIMER; }
	| L_SEQUENCER		{ $$ = SND_CONTROL_IFACE_SEQUENCER; }
	| error			{ yyerror( "an unknown keyword in the interface field"); }
	;

boolean	: L_TRUE		{ $$ = 1; }
	| L_FALSE		{ $$ = 0; }
	;

integer	: L_INTEGER		{ $$ = $1; }
	;

string	: L_STRING		{ $$ = $1; }
	;

rawdata : L_RAWDATA '(' L_BYTEARRAY ')'	{ $$ = $3; }
	| L_RAWDATA error	{ yyerror( "malformed rawdata value" ); }
	;

%%

static void yyerror(char *string,...)
{
	char errstr[1024];

	va_list vars;
	va_start(vars, string);
	vsprintf(errstr, string, vars);
	va_end(vars);
	error("Error in configuration file '%s' (line %i): %s", cfgfile, linecount + 1, errstr);

	exit(1);
}

static void error_nomem(void)
{
	yyerror("No enough memory...\n");
}

static void build_soundcard(char *name)
{
	struct soundcard *soundcard;

	if (!name) {
		Xsoundcard = NULL;
		return;
	}
	Xsoundcard = (struct soundcard *)malloc(sizeof(struct soundcard));
	if (!Xsoundcard) {
		free(name);
		error_nomem();
		return;
	}
	bzero(Xsoundcard, sizeof(*Xsoundcard));
	for (soundcard = rsoundcards; soundcard && soundcard->next; soundcard = soundcard->next);
	if (soundcard) {
		soundcard->next = Xsoundcard;
	} else {
		rsoundcards = Xsoundcard;
	}
	strncpy(Xsoundcard->control.hwinfo.id, name, sizeof(Xsoundcard->control.hwinfo.id));
	free(name);
}

static void build_control_begin(int iface, unsigned int device, unsigned int subdevice, char *name, unsigned int index)
{
	struct ctl_control **first;
	struct ctl_control *ctl;

	first = &Xsoundcard->control.controls;
	Xcontrol = (struct ctl_control *)malloc(sizeof(struct ctl_control));
	if (!Xcontrol) {
		free(name);
		error_nomem();
		return;
	}
	Xposition = 0;
	Xtype = SND_CONTROL_TYPE_NONE;
	bzero(Xcontrol, sizeof(*Xcontrol));
 	for (ctl = *first; ctl && ctl->next; ctl = ctl->next);
	if (ctl) {
		ctl->next = Xcontrol;
	} else {
		*first = Xcontrol;
	}
	Xcontrol->c.id.iface = iface;
	Xcontrol->c.id.device = device;
	Xcontrol->c.id.subdevice = subdevice;
	strncpy(Xcontrol->c.id.name, name, sizeof(Xcontrol->c.id.name));
	Xcontrol->c.id.index = index;
	free(name);
}

static void build_control_end(void)
{
	Xcontrol = NULL;
}

static void set_control_boolean(int val)
{
	switch (Xtype) {
	case SND_CONTROL_TYPE_NONE:
	case SND_CONTROL_TYPE_BOOLEAN:
		Xtype = Xcontrol->type = SND_CONTROL_TYPE_BOOLEAN;
		break;
	case SND_CONTROL_TYPE_INTEGER:
		break;
	default:
		yyerror("Unexpected previous type (%i).\n", Xtype);
	}
	if (Xposition < 512)
		Xcontrol->c.value.integer.value[Xposition++] = val ? 1 : 0;
	else
		yyerror("Array overflow.");
}

static void set_control_integer(long val)
{
	unsigned int xx;

	switch (Xtype) {
	case SND_CONTROL_TYPE_NONE:
	case SND_CONTROL_TYPE_BOOLEAN:
	case SND_CONTROL_TYPE_INTEGER:
		Xtype = Xcontrol->type = SND_CONTROL_TYPE_INTEGER;
		break;
	default:
		yyerror("Unexpected previous type (%i).\n", Xtype);
	}
	if (Xposition < 512) {
		xx = val;
		Xcontrol->c.value.integer.value[Xposition++] = val;
	}
}

static void set_control_bytearray(struct bytearray val)
{
	if (Xtype != SND_CONTROL_TYPE_NONE && Xtype != SND_CONTROL_TYPE_BYTES)
		yyerror("Unexpected previous type (%i).\n", Xtype);
	Xtype = Xcontrol->type = SND_CONTROL_TYPE_BYTES;

	if (val.datalen + Xposition > 512)
		yyerror("Byte array too large for control.");

	memcpy(&Xcontrol->c.value.bytes.data[Xposition], val.data, val.datalen);
	Xposition += val.datalen;
}
