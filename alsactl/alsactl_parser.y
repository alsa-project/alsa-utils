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
static void build_mixer(char *name);
static void build_pcm(char *name);
static void build_rawmidi(char *name);

static void build_mixer_element(char *name, int index, int etype);

static void build_control_switch(char *name);
static void build_mixer_switch(char *name);
static void build_pcm_playback_switch(char *name);
static void build_pcm_record_switch(char *name);
static void build_rawmidi_output_switch(char *name);
static void build_rawmidi_input_switch(char *name);

static void mixer_switch1(int end);
static void mixer_switch1_value(int val);
static void mixer_switch2(int end);
static void mixer_switch2_value(int val);
static void mixer_switch3(int end);
static void mixer_switch3_value(int val);
static void mixer_volume1(int end);
static void mixer_volume1_value(int val);
static void mixer_3d_effect1(int end);
static void mixer_3d_effect1_value(unsigned int effect, int val);
static void mixer_accu3(int end);
static void mixer_accu3_value(int val);
static void mixer_mux1(int end);
static void mixer_mux1_value(char *str, int index, int type);
static void mixer_mux2(int end);
static void mixer_mux2_value(char *str, int index, int type);
static void mixer_tone_control1(int end);
static void mixer_tone_control1_value(unsigned int effect, int val);

static void set_switch_boolean(int val);
static void set_switch_integer(int val);
static void set_switch_bytearray(struct bytearray val);
static void set_switch_iec958ocs_begin(int end);
static void set_switch_iec958ocs(int idx, unsigned short val, unsigned short mask);

	/* local variables */

static struct soundcard *Xsoundcard = NULL;
static struct mixer *Xmixer = NULL;
static struct pcm *Xpcm = NULL;
static struct rawmidi *Xrawmidi = NULL;
static struct mixer_element *Xelement  = NULL;
static struct ctl_switch *Xswitch = NULL;
static unsigned int Xswitchiec958ocs = 0;
static unsigned short Xswitchiec958ocs1[16];

%}

%start lines

%union {
    int b_value;
    int i_value;
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
%token L_SOUNDCARD L_MIXER L_ELEMENT L_SWITCH L_RAWDATA
%token L_CONTROL L_PCM L_RAWMIDI L_PLAYBACK L_RECORD L_INPUT L_OUTPUT
%token L_SWITCH1 L_SWITCH2 L_SWITCH3 L_VOLUME1 L_3D_EFFECT1 L_ACCU3
%token L_MUX1 L_MUX2 L_TONE_CONTROL1
%token L_IEC958OCS L_3D L_RESET L_USER L_VALID L_DATA L_PROTECT L_PRE2
%token L_FSUNLOCK L_TYPE L_GSTATUS L_ENABLE L_DISABLE
%token L_SW L_MONO_SW L_WIDE L_VOLUME L_CENTER L_SPACE L_DEPTH L_DELAY
%token L_DEPTH_REAR
%token L_FEEDBACK L_BASS L_TREBLE


%type <b_value> boolean
%type <i_value> integer
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

soundcards : soundcard
	| soundcards soundcard
	;

soundcard : L_CONTROL '{' controls '}'
	| L_MIXER '(' string	{ build_mixer($3); }
	  L_DOUBLE1 mixers '}'	{ build_mixer(NULL); }
	| L_PCM '(' string	{ build_pcm($3); }
	  L_DOUBLE1 pcms '}'	{ build_pcm(NULL); }
	| L_RAWMIDI '(' string	{ build_rawmidi($3); }
	  L_DOUBLE1 rawmidis '}' { build_rawmidi(NULL); }
	| error			{ yyerror( "an unknown keyword in the soundcard{} level"); }
	;

controls : control
	| controls control
	;

control : L_SWITCH '(' string	{ build_control_switch($3); }
	  ',' switches ')'	{ build_control_switch(NULL); }
	| error			{ yyerror("an unknown keyword in the control{} level"); }
	;


mixers	: mixer
	| mixers mixer
	;

mixer	: L_ELEMENT '(' string
	  ',' integer ',' integer { build_mixer_element($3, $5, $7); } 
	  ',' etype ')' 	{ build_mixer_element(NULL, -1, -1); }
	| L_SWITCH '(' string	{ build_mixer_switch($3); }
	  ',' switches ')'	{ build_mixer_switch(NULL); }
	| error			{ yyerror("an unknown keyword in the mixer level"); }
	;


etype	: L_SWITCH1 '('		{ mixer_switch1(0); } 
	  m_switch1 ')'		{ mixer_switch1(1); }
	| L_SWITCH2 '('		{ mixer_switch2(0); }
	  m_switch2 ')'		{ mixer_switch2(1); }
	| L_SWITCH3 '('		{ mixer_switch3(0); }
          m_switch3 ')'		{ mixer_switch3(1); }
	| L_VOLUME1 '('		{ mixer_volume1(0); }
	  m_volume1 ')'		{ mixer_volume1(1); }
	| L_3D_EFFECT1 '('	{ mixer_3d_effect1(0); }
	  m_3d_effect1 ')'	{ mixer_3d_effect1(1); }
	| L_ACCU3 '('		{ mixer_accu3(0); }
	  m_accu3 ')'		{ mixer_accu3(1); }
	| L_MUX1 '('		{ mixer_mux1(0); }
	  m_mux1 ')'		{ mixer_mux1(1); }
	| L_MUX2 '('		{ mixer_mux2(0); }
	  L_ELEMENT '('
	  string ','
	  integer ','
	  integer ')'		{ mixer_mux2_value($6, $8, $10); }
	  ')'			{ mixer_mux2(1); }
	| L_TONE_CONTROL1 '('	{ mixer_tone_control1(0); }
	  m_tone_control1 ')'	{ mixer_tone_control1(1); }
	| error			{ yyerror("an unknown keyword in the mixer element level"); }
	;

m_switch1 : m_switch1_0
	| m_switch1 ',' m_switch1_0
	;

m_switch1_0 : boolean		{ mixer_switch1_value($1); }
	| error			{ yyerror("an unknown keyword in the Switch1 element level"); }
	;

m_switch2 : m_switch2_0
	| m_switch2 ',' m_switch2_0
	;

m_switch2_0 : boolean		{ mixer_switch2_value($1); }
	| error			{ yyerror("an unknown keyword in the Switch2 element level"); }
	;

m_switch3 : m_switch3_0
	| m_switch3 ',' m_switch3_0
	;

m_switch3_0 : boolean		{ mixer_switch3_value($1); }
	| error			{ yyerror("an unknown keyword in the Switch3 element level"); }
	;

m_volume1 : m_volume1_0
	| m_volume1 ',' m_volume1_0
	;

m_volume1_0 : integer		{ mixer_volume1_value($1); }
	| error			{ yyerror("an unknown keyword in the Volume1 element level"); }
	;

m_3d_effect1 : m_3d_effect1_0
	| m_3d_effect1 ',' m_3d_effect1_0
	;

m_3d_effect1_0 : L_SW '=' boolean { mixer_3d_effect1_value(SND_MIXER_EFF1_SW, $3); }
	| L_MONO_SW '=' boolean	{ mixer_3d_effect1_value(SND_MIXER_EFF1_MONO_SW, $3); }
	| L_WIDE '=' integer	{ mixer_3d_effect1_value(SND_MIXER_EFF1_WIDE, $3); }
	| L_VOLUME '=' integer	{ mixer_3d_effect1_value(SND_MIXER_EFF1_VOLUME, $3); }
	| L_CENTER '=' integer	{ mixer_3d_effect1_value(SND_MIXER_EFF1_CENTER, $3); }
	| L_SPACE '=' integer	{ mixer_3d_effect1_value(SND_MIXER_EFF1_SPACE, $3); }
	| L_DEPTH '=' integer	{ mixer_3d_effect1_value(SND_MIXER_EFF1_DEPTH, $3); }
	| L_DELAY '=' integer	{ mixer_3d_effect1_value(SND_MIXER_EFF1_DELAY, $3); }
	| L_FEEDBACK '=' integer { mixer_3d_effect1_value(SND_MIXER_EFF1_FEEDBACK, $3); }
	| L_DEPTH_REAR '=' integer { mixer_3d_effect1_value(SND_MIXER_EFF1_DEPTH_REAR, $3); }
	| error			{ yyerror("an unknown keyword in the 3D Effect1 element level"); }
	;

m_accu3 : m_accu3_0
	| m_accu3 ',' m_accu3_0
	;

m_accu3_0 : integer		{ mixer_accu3_value($1); }
	| error			{ yyerror("an unknown keyword in the Accu3 element level"); }
	;

m_mux1	: m_mux1_0
	| m_mux1 ',' m_mux1_0
	;

m_mux1_0 : L_ELEMENT '(' string
	   ',' integer ','
	   integer ')'		{ mixer_mux1_value($3, $5, $7); }
	| error			{ yyerror("an unknown keyword in the Mux1 element level"); }
	;

m_tone_control1 : m_tone_control1_0
	| m_tone_control1 ',' m_tone_control1_0
	;

m_tone_control1_0 : L_SW '=' boolean { mixer_tone_control1_value(SND_MIXER_TC1_SW, $3); }
	| L_BASS '=' integer	{ mixer_tone_control1_value(SND_MIXER_TC1_BASS, $3); }
	| L_TREBLE '=' integer	{ mixer_tone_control1_value(SND_MIXER_TC1_TREBLE, $3); }
	| error			{ yyerror("an unknown keyword in the ToneControl1 element level"); }
	;


pcms	: pcm
	| pcms pcm
	;

pcm	: L_PLAYBACK '{' playbacks '}'
	| L_RECORD '{' records '}'
	| error			{ yyerror("an unknown keyword in the pcm{} section"); }
	;

playbacks : playback
	| playbacks playback
	;

playback : L_SWITCH '(' string	{ build_pcm_playback_switch($3); }
	   ',' switches ')'	{ build_pcm_playback_switch(NULL); }
	| error			{ yyerror("an unknown keyword in the playback{} section"); }
	;

records : record
	| records record
	;

record	: L_SWITCH '(' string	{ build_pcm_record_switch($3); }
	  ',' switches ')'	{ build_pcm_record_switch(NULL); }
	| error			{ yyerror("an unknown keyword in the record{} section"); }
	;

rawmidis : rawmidi
	| rawmidis rawmidi
	;

rawmidi	: L_INPUT '{' inputs '}'
	| L_OUTPUT '{' outputs '}'
	;

inputs	: input
	| inputs input
	;

input	: L_SWITCH '(' string	{ build_rawmidi_input_switch($3); }
	  ',' switches ')'	{ build_rawmidi_input_switch(NULL); }
	| error			{ yyerror( "an unknown keyword in the input{} section" ); }
	;

outputs	: output
	| outputs output
	;

output	: L_SWITCH '(' string	{ build_rawmidi_output_switch($3); }
	  ',' switches ')'	{ build_rawmidi_output_switch(NULL); }
	| error			{ yyerror( "an unknown keyword in the output{} section" ); }
	;

switches : switch
	| switches switch
	;

switch	: boolean		{ set_switch_boolean($1); }
	| integer		{ set_switch_integer($1); }
	| L_IEC958OCS '('	{ set_switch_iec958ocs_begin(0); }
	  iec958ocs ')'		{ set_switch_iec958ocs_begin(1); }
	| rawdata		{ set_switch_bytearray($1); }
	| error			{ yyerror( "an unknown keyword in the switch() data parameter" ); }
	;

iec958ocs : iec958ocs1
	| iec958ocs ',' iec958ocs1
	;

iec958ocs1 : L_ENABLE		{ set_switch_iec958ocs( 0, 1, 0 ); }
	| L_DISABLE		{ set_switch_iec958ocs( 0, 0, 0 ); }
	| L_3D			{ set_switch_iec958ocs( 4, 0x2000, ~0x2000 ); }
	| L_RESET		{ set_switch_iec958ocs( 4, 0x0040, ~0x0040 ); }
	| L_USER		{ set_switch_iec958ocs( 4, 0x0020, ~0x0020 ); }
	| L_VALID		{ set_switch_iec958ocs( 4, 0x0010, ~0x0010 ); }
	| L_DATA		{ set_switch_iec958ocs( 5, 0x0002, ~0x0002 ); }
	| L_PROTECT		{ set_switch_iec958ocs( 5, 0, ~0x0004 ); }
	| L_PRE2		{ set_switch_iec958ocs( 5, 0x0008, ~0x0018 ); }
	| L_FSUNLOCK		{ set_switch_iec958ocs( 5, 0x0020, ~0x0020 ); }
	| L_TYPE '(' integer ')' { set_switch_iec958ocs( 5, ($3 & 0x7f) << 6, ~(0x7f<<6) ); }
	| L_GSTATUS		{ set_switch_iec958ocs( 5, 0x2000, ~0x2000 ); }
	| error			{ yyerror( "an unknown keyword in the iec958ocs1() arguments" ); }
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

static void build_mixer(char *name)
{
	struct mixer *mixer;

	if (!name) {
		Xmixer = NULL;
		return;
	}
	Xmixer = (struct mixer *)malloc(sizeof(struct pcm));
	if (!Xmixer) {
		free(name);
		error_nomem();
		return;
	}
	bzero(Xmixer, sizeof(*Xmixer));
	for (mixer = Xsoundcard->mixers; mixer && mixer->next; mixer = mixer->next);
	if (mixer) {
		mixer->next = Xmixer;
	} else {
		Xsoundcard->mixers = Xmixer;
	}
	strncpy(Xmixer->info.name, name, sizeof(Xmixer->info.name));
	free(name);
}

static void build_pcm(char *name)
{
	struct pcm *pcm;

	if (!name) {
		Xpcm = NULL;
		return;
	}
	Xpcm = (struct pcm *)malloc(sizeof(struct pcm));
	if (!Xpcm) {
		free(name);
		error_nomem();
		return;
	}
	bzero(Xpcm, sizeof(*Xpcm));
	for (pcm = Xsoundcard->pcms; pcm && pcm->next; pcm = pcm->next);
	if (pcm) {
		pcm->next = Xpcm;
	} else {
		Xsoundcard->pcms = Xpcm;
	}
	strncpy(Xpcm->info.name, name, sizeof(Xpcm->info.name));
	free(name);
}

static void build_rawmidi(char *name)
{
	struct rawmidi *rawmidi;

	if (!name) {
		Xrawmidi = NULL;
		return;
	}
	Xrawmidi = (struct rawmidi *)malloc(sizeof(struct rawmidi));
	if (!Xrawmidi) {
		free(name);
		error_nomem();
		return;
	}
	bzero(Xrawmidi, sizeof(*Xrawmidi));
	for (rawmidi = Xsoundcard->rawmidis; rawmidi && rawmidi->next; rawmidi = rawmidi->next);
	if (rawmidi) {
		rawmidi->next = Xrawmidi;
	} else {
		Xsoundcard->rawmidis = Xrawmidi;
	}
	strncpy(Xrawmidi->info.name, name, sizeof(Xrawmidi->info.name));
	free(name);
}

static void build_mixer_element(char *name, int index, int etype)
{
	struct mixer_element *element;

	if (!name) {
		Xelement = NULL;
		return;
	}
	Xelement = (struct mixer_element *)malloc(sizeof(struct mixer_element));
	if (!Xelement) {
		free(name);
		error_nomem();
		return;
	}
	bzero(Xelement, sizeof(*Xelement));	
	for (element = Xmixer->elements; element && element->next; element = element->next);
	if (element) {
		element->next = Xelement;
	} else {
		Xmixer->elements = Xelement;
	}
	strncpy(Xelement->element.eid.name, name, sizeof(Xelement->element.eid.name));
	Xelement->element.eid.index = index;
	Xelement->element.eid.type = etype;
	Xelement->info.eid = Xelement->element.eid;
	free(name);
}

static void mixer_type_check(int type)
{
	if (Xelement->element.eid.type != type)
		yyerror("The element has got the unexpected data type.");
}

static void mixer_switch1(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_SWITCH1);
}

static void mixer_switch1_value(int val)
{
	unsigned int *ptr;

	if (Xelement->element.data.switch1.sw_size <= Xelement->element.data.switch1.sw) {
		Xelement->element.data.switch1.sw_size += 32;
		ptr = (unsigned int *)realloc(Xelement->element.data.switch1.psw, ((Xelement->element.data.switch1.sw_size + 31) / 32) * sizeof(unsigned int));
		if (ptr == NULL) {
			error_nomem();
			return;
		}
		Xelement->element.data.switch1.psw = ptr;
	}
	snd_mixer_set_bit(Xelement->element.data.switch1.psw, Xelement->element.data.switch1.sw++, val ? 1 : 0);
}

static void mixer_switch2(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_SWITCH2);
}

static void mixer_switch2_value(int val)
{
	Xelement->element.data.switch2.sw = val ? 1 : 0;
}

static void mixer_switch3(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_SWITCH3);
}

static void mixer_switch3_value(int val)
{
	unsigned int *ptr;

	if (Xelement->element.data.switch3.rsw_size <= Xelement->element.data.switch3.rsw) {
		Xelement->element.data.switch3.rsw_size += 32;
		ptr = (unsigned int *)realloc(Xelement->element.data.switch1.psw, ((Xelement->element.data.switch3.rsw_size + 31) / 32) * sizeof(unsigned int));
		if (ptr == NULL) {
			error_nomem();
			return;
		}
		Xelement->element.data.switch3.prsw = ptr;
	}
	snd_mixer_set_bit(Xelement->element.data.switch3.prsw, Xelement->element.data.switch3.rsw++, val ? 1 : 0);
}

static void mixer_volume1(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_VOLUME1);
}

static void mixer_volume1_value(int val)
{
	int *ptr;

	if (Xelement->element.data.volume1.voices_size <= Xelement->element.data.volume1.voices) {
		Xelement->element.data.volume1.voices_size += 4;
		ptr = (int *)realloc(Xelement->element.data.volume1.pvoices, Xelement->element.data.volume1.voices_size * sizeof(int));
		if (ptr == NULL) {
			error_nomem();
			return;
		}
		Xelement->element.data.volume1.pvoices = ptr;
	}
	Xelement->element.data.volume1.pvoices[Xelement->element.data.volume1.voices++] = val;
} 

static void mixer_3d_effect1(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_3D_EFFECT1);
}

static void mixer_3d_effect1_value(unsigned int effect, int val)
{
	switch (effect) {
	case SND_MIXER_EFF1_SW:
		Xelement->element.data.teffect1.sw = val ? 1 : 0;
		break;
	case SND_MIXER_EFF1_MONO_SW:
		Xelement->element.data.teffect1.mono_sw = val ? 1 : 0;
		break;
	case SND_MIXER_EFF1_WIDE:
		Xelement->element.data.teffect1.wide = val;
		break;
	case SND_MIXER_EFF1_VOLUME:
		Xelement->element.data.teffect1.volume = val;
		break;
	case SND_MIXER_EFF1_CENTER:
		Xelement->element.data.teffect1.center = val;
		break;
	case SND_MIXER_EFF1_SPACE:
		Xelement->element.data.teffect1.space = val;
		break;
	case SND_MIXER_EFF1_DEPTH:
		Xelement->element.data.teffect1.depth = val;
		break;
	case SND_MIXER_EFF1_DELAY:
		Xelement->element.data.teffect1.delay = val;
		break;
	case SND_MIXER_EFF1_FEEDBACK:
		Xelement->element.data.teffect1.feedback = val;
		break;
	case SND_MIXER_EFF1_DEPTH_REAR:
		Xelement->element.data.teffect1.depth_rear = val;
		break;
	default:
		yyerror("Unknown effect 0x%x\n", effect);
	}
} 

static void mixer_accu3(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_ACCU3);
}

static void mixer_accu3_value(int val)
{
	int *ptr;

	if (Xelement->element.data.accu3.voices_size <= Xelement->element.data.accu3.voices) {
		Xelement->element.data.accu3.voices_size += 4;
		ptr = (int *)realloc(Xelement->element.data.accu3.pvoices, Xelement->element.data.accu3.voices_size * sizeof(int));
		if (ptr == NULL) {
			error_nomem();
			return;
		}
		Xelement->element.data.accu3.pvoices = ptr;
	}
	Xelement->element.data.accu3.pvoices[Xelement->element.data.accu3.voices++] = val;
} 

static void mixer_mux1(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_MUX1);
}

static void mixer_mux1_value(char *name, int index, int type)
{
	snd_mixer_eid_t *ptr;
	snd_mixer_eid_t *eid;

	if (Xelement->element.data.mux1.output_size <= Xelement->element.data.mux1.output) {
		Xelement->element.data.mux1.output_size += 4;
		ptr = (snd_mixer_eid_t *)realloc(Xelement->element.data.mux1.poutput, Xelement->element.data.mux1.output_size * sizeof(snd_mixer_eid_t));
		if (ptr == NULL) {
			error_nomem();
			free(name);
			return;
		}
		Xelement->element.data.mux1.poutput = ptr;
	}
	eid = &Xelement->element.data.mux1.poutput[Xelement->element.data.mux1.output++];
	strncpy(eid->name, name, sizeof(eid->name));
	eid->index = index;
	eid->type = type;
	free(name);
} 

static void mixer_mux2(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_MUX2);
}

static void mixer_mux2_value(char *name, int index, int type)
{
	snd_mixer_eid_t *eid;

	eid = &Xelement->element.data.mux2.output;
	strncpy(eid->name, name, sizeof(eid->name));
	eid->index = index;
	eid->type = type;
	free(name);
} 

static void mixer_tone_control1(int end)
{
	mixer_type_check(SND_MIXER_ETYPE_TONE_CONTROL1);
}

static void mixer_tone_control1_value(unsigned int effect, int val)
{
	Xelement->element.data.tc1.tc |= effect;
	switch (effect) {
	case SND_MIXER_TC1_SW:
		Xelement->element.data.tc1.sw = val ? 1 : 0;
		break;
	case SND_MIXER_TC1_BASS:
		Xelement->element.data.tc1.bass = val;
		break;
	case SND_MIXER_TC1_TREBLE:
		Xelement->element.data.tc1.treble = val;
		break;
	default:
		yyerror("Unknown effect 0x%x\n", effect);
	}
} 

static void build_switch(struct ctl_switch **first, char *name)
{
	struct ctl_switch *sw;

	if (!name) {
		Xswitch = NULL;
		return;
	}
	Xswitch = (struct ctl_switch *)malloc(sizeof(struct ctl_switch));
	if (!Xswitch) {
		free(name);
		error_nomem();
		return;
	}
	bzero(Xswitch, sizeof(*Xswitch));
	for (sw = *first; sw && sw->next; sw = sw->next);
	if (sw) {
		sw->next = Xswitch;
	} else {
		*first = Xswitch;
	}
	strncpy(Xswitch->s.name, name, sizeof(Xswitch->s.name));
	free(name);
}

static void build_control_switch(char *name)
{
	build_switch(&Xsoundcard->control.switches, name);
}

static void build_mixer_switch(char *name)
{
	build_switch(&Xmixer->switches, name);
}

static void build_pcm_playback_switch(char *name)
{
	build_switch(&Xpcm->pswitches, name);
}

static void build_pcm_record_switch(char *name)
{ 
	build_switch(&Xpcm->rswitches, name);
}

static void build_rawmidi_output_switch(char *name)
{
	build_switch(&Xrawmidi->oswitches, name);
}

static void build_rawmidi_input_switch(char *name)
{
	build_switch(&Xrawmidi->iswitches, name);
}

static void set_switch_boolean(int val)
{
	Xswitch->s.type = SND_SW_TYPE_BOOLEAN;
	Xswitch->s.value.enable = val ? 1 : 0;
}

static void set_switch_integer(int val)
{
	unsigned int xx;

	Xswitch->s.type = SND_SW_TYPE_DWORD;
	xx = val;
	memcpy(&Xswitch->s.value, &xx, sizeof(xx));
}

static void set_switch_bytearray(struct bytearray val)
{
	Xswitch->s.type = SND_SW_TYPE_LAST + 1;

	if (val.datalen > 32)
		yyerror("Byte array too large for switch.");

	memcpy(Xswitch->s.value.data8, val.data, val.datalen);
}

static void set_switch_iec958ocs_begin(int end)
{
	if (end) {
		Xswitch->s.value.enable = Xswitchiec958ocs;
		Xswitch->s.value.data16[4] = Xswitchiec958ocs1[4];
		Xswitch->s.value.data16[5] = Xswitchiec958ocs1[5];
#if 0
		printf("IEC958: enable = %i, ocs1[4] = 0x%x, ocs1[5] = 0x%x\n",
		       sw->value.enable,
		       sw->value.data16[4],
		       sw->value.data16[5]);
#endif
		return;
	}
	Xswitch->s.type = SND_SW_TYPE_BOOLEAN;
	Xswitch->s.value.data32[1] = ('C' << 8) | 'S';
	Xswitchiec958ocs = 0;
	Xswitchiec958ocs1[4] = 0x0000;
	Xswitchiec958ocs1[5] = 0x0004;	/* copy permitted */
}

static void set_switch_iec958ocs(int idx, unsigned short val, unsigned short mask)
{
	if (idx == 0) {
		Xswitchiec958ocs = val ? 1 : 0;
		return;
	}
	Xswitchiec958ocs1[idx] &= mask;
	Xswitchiec958ocs1[idx] |= val;
}
