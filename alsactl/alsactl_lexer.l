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

%{

#include "alsactl.h"

struct bytearray {
  unsigned char *data;
  size_t datalen;
};

#include "alsactl_parser.h"

#define YY_NO_UNPUT
#undef YY_CDECL
#define YY_CDECL int YY_PROTO(yylex( void ));

int linecount;

%}

%%

	/* special characters */

"{"|"}"			return yytext[0];
"("|")"			return yytext[0];
"["|"]"			return yytext[0];
")"[ \t]*"{"		return L_DOUBLE1;
","			return yytext[0];
"="			return yytext[0];

	/* tokens */

soundcard		return L_SOUNDCARD;
control			return L_CONTROL;
mixer			return L_MIXER;
element			return L_ELEMENT;
switch			return L_SWITCH;
rawdata			return L_RAWDATA;
pcm			return L_PCM;
rawmidi			return L_RAWMIDI;
playback		return L_PLAYBACK;
capture			return L_CAPTURE;
output			return L_OUTPUT;
input			return L_INPUT;
iec958ocs		return L_IEC958OCS;
3d			return L_3D;
reset			return L_RESET;
user			return L_USER;
valid			return L_VALID;
data			return L_DATA;
protect			return L_PROTECT;
pre2			return L_PRE2;
fsunlock		return L_FSUNLOCK;
type			return L_TYPE;
gstatus			return L_GSTATUS;
enable			return L_ENABLE;
disable			return L_DISABLE;
sw			return L_SW;
mono_sw			return L_MONO_SW;
wide			return L_WIDE;
volume			return L_VOLUME;
center			return L_CENTER;
space			return L_SPACE;
depth			return L_DEPTH;
delay			return L_DELAY;
feedback		return L_FEEDBACK;
depth_rear		return L_DEPTH_REAR;
bass			return L_BASS;
treble			return L_TREBLE;

	/* element types */

Switch1			return L_SWITCH1;
Switch2			return L_SWITCH2;
Switch3			return L_SWITCH3;
Volume1			return L_VOLUME1;
Accu3			return L_ACCU3;
Mux1			return L_MUX1;
Mux2			return L_MUX2;
ToneControl1		return L_TONE_CONTROL1;
_3D_Effect1		return L_3D_EFFECT1;

	/* boolean */

false|off|no		return L_FALSE;
true|on|yes		return L_TRUE;

	/* integers */

[0-9]+			{ yylval.i_value = atoi(yytext); return L_INTEGER; }
0x[0-9a-f]+		{ char *end;
                          yylval.i_value = strtol(yytext, &end, 0);
                          return L_INTEGER; }

	/* byte array */

"@"([0-9a-f]{2}:){0,31}([0-9a-f]{2})"@" {
			  char *p = yytext + 1, x[3];
			  unsigned char *d;
			  int val;
			  yylval.a_value.data = d = (unsigned char *)malloc( 32 );
			  yylval.a_value.datalen = 0;
			  while (*p) {
				strncpy(x, p, 2); x[2] = '\0';
				sscanf(x, "%02x", &val);
				*d++ = val; 
				++yylval.a_value.datalen;
				p += 2;
				if (*p == '@')
					break;
				p++;
			  }
		          return L_BYTEARRAY; }

	/* strings */

\"[^\"]*\"              { yytext[strlen(yytext) - 1] = 0;
                          yylval.s_value = strdup(&yytext[1]);
                          return L_STRING; }
\'[^\']*\'              { yytext[strlen(yytext) - 1] = 0;
                          yylval.s_value = strdup(&yytext[1]);
                          return L_STRING; }
[a-z0-9/\~@-Za-z_]+ 	{ yylval.s_value = strdup(yytext);
                          return L_STRING; }
$[a-z0-9/\~@-Za-z_]+	{ yylval.s_value = strdup(getenv(&yytext[1]));
                          return L_STRING; }

	/* comments & whitespaces */

[#\;][^\n]*\n		{ linecount++; }
[ \t]+			;
\n			{ linecount++; }
.			fprintf( stderr, "alsactl: discarding char '%c' - line %i\n", yytext[0], linecount + 1 );

%%

#ifndef yywrap
int yywrap(void)	/* do this avoid to do -lfl */
{
  return 1;
}
#endif
