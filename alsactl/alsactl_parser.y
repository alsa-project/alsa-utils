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
static void build_control_begin(void);
static void build_control_end(void);
static void set_control_iface(int iface);
static void set_control_device(int dev);
static void set_control_subdevice(int subdev);
static void set_control_name(char *name);
static void set_control_index(int idx);
static void set_control_type(snd_control_type_t type);
static void set_control_boolean(int val);
static void set_control_integer(long val);

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
%token L_IDENT L_IFACE L_NAME L_DEVICE L_SUBDEVICE L_INDEX
%token L_BOOL L_INT L_ENUM L_BYTE

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

soundcard : L_CONTROL '(' L_IDENT '=' 	{ build_control_begin(); }
	'{' ctlids '}' ',' controls ')'	{ build_control_end(); }
	| error			{ yyerror("an unknown keyword in the soundcard{} level"); }
	;

ctlids	: ctlid
	| ctlids ',' ctlid
	;

ctlid	: L_IFACE '=' iface	{ set_control_iface($3); }
	| L_DEVICE '=' integer	{ set_control_device($3); }
	| L_SUBDEVICE '=' integer { set_control_subdevice($3); }
	| L_NAME '=' string	{ set_control_name($3); }
	| L_INDEX '=' integer	{ set_control_index($3); }
	| error			{ yyerror("an unknown keyword in the control ID level"); }
	;

controls : control
	;

control : L_BOOL '=' { set_control_type(SND_CONTROL_TYPE_BOOLEAN); } '{' datas '}' 
	| L_INT '=' { set_control_type(SND_CONTROL_TYPE_INTEGER); } '{' datas '}'
	| L_ENUM '=' { set_control_type(SND_CONTROL_TYPE_ENUMERATED); } '{' datas '}'
	| L_BYTE '=' { set_control_type(SND_CONTROL_TYPE_BYTES); } '{' datas '}'
	| error			{ yyerror( "an unknown keyword in the control() data parameter" ); }
	;

datas	: data
	| datas ',' data
	;

data	: boolean		{ set_control_boolean($1); }
	| integer		{ set_control_integer($1); }
	| error			{ yyerror( "an unknown keyword in the control() data argument" ); }
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

static void build_control_begin(void)
{
	struct ctl_control **first;
	struct ctl_control *ctl;

	first = &Xsoundcard->control.controls;
	Xcontrol = (struct ctl_control *)malloc(sizeof(struct ctl_control));
	if (!Xcontrol) {
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
}

static void build_control_end(void)
{
	Xcontrol = NULL;
}

static void set_control_iface(int iface)
{
	Xcontrol->c.id.iface = iface;
}

static void set_control_device(int dev)
{
	Xcontrol->c.id.device = dev;
}

static void set_control_subdevice(int subdev)
{
	Xcontrol->c.id.subdevice = subdev;
}

static void set_control_name(char *name)
{
	if (name == NULL)
		return;
	strncpy(Xcontrol->c.id.name, name, sizeof(Xcontrol->c.id.name));
	free(name);
}

static void set_control_index(int idx)
{
	Xcontrol->c.id.index = idx;
}

static void set_control_type(snd_control_type_t type)
{
	Xcontrol->type = Xtype = type;
}

static void set_control_boolean(int val)
{
	if (Xposition >= 512)
		yyerror("Array overflow.");
	switch (Xtype) {
	case SND_CONTROL_TYPE_BOOLEAN:
		Xcontrol->c.value.integer.value[Xposition++] = val ? 1 : 0;
		break;
	case SND_CONTROL_TYPE_INTEGER:
		Xcontrol->c.value.integer.value[Xposition++] = val ? 1 : 0;
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		Xcontrol->c.value.enumerated.item[Xposition++] = val ? 1 : 0;
		break;
	case SND_CONTROL_TYPE_BYTES:
		Xcontrol->c.value.bytes.data[Xposition++] = val ? 1 : 0;
		break;
	default: break;
	}
}

static void set_control_integer(long val)
{
	if (Xposition >= 512)
		yyerror("Array overflow.");
	switch (Xtype) {
	case SND_CONTROL_TYPE_BOOLEAN:
		Xcontrol->c.value.integer.value[Xposition++] = val ? 1 : 0;
		break;
	case SND_CONTROL_TYPE_INTEGER:
		Xcontrol->c.value.integer.value[Xposition++] = val;
		break;
	case SND_CONTROL_TYPE_ENUMERATED:
		Xcontrol->c.value.enumerated.item[Xposition++] = (unsigned int)val;
		break;
	case SND_CONTROL_TYPE_BYTES:
		Xcontrol->c.value.bytes.data[Xposition++] = (unsigned char)val;
		break;
	default: break;
	}
}
