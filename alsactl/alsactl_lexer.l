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
")"[ \t]*"{"		return L_DOUBLE1;
","			return yytext[0];

	/* tokens */

soundcard		return L_SOUNDCARD;
control			return L_CONTROL;
mixer			return L_MIXER;
channel			return L_CHANNEL;
stereo			return L_STEREO;
mono			return L_MONO;
switch			return L_SWITCH;
rawdata			return L_RAWDATA;
pcm			return L_PCM;
rawmidi			return L_RAWMIDI;
playback		return L_PLAYBACK;
record			return L_RECORD;
output			return L_OUTPUT;
input			return L_INPUT;
iec958ocs		return L_IEC958OCS;
3d			return L_3D;
reset			return L_RESET;
user			return L_USER;
valid			return L_VALID;
data			return L_DATA;
protected		return L_PROTECT;
pre2			return L_PRE2;
fslock			return L_FSLOCK;
type			return L_TYPE;
gstatus			return L_GSTATUS;
enable			return L_ENABLE;
disable			return L_DISABLE;
mute			return L_MUTE;
swout			return L_SWOUT;
swin			return L_SWIN;

	/* boolean */

false|off|no		return L_FALSE;
true|on|yes		return L_TRUE;

	/* integers */

[0-9]+			{ yylval.i_value = atoi( yytext ); return L_INTEGER; }
0x[0-9a-f]+		{ char *end;
                          yylval.i_value = strtol( yytext, &end, 0 );
                          return L_INTEGER; }

	/* byte array */

"@"([0-9a-f]{2}:){0,31}([0-9a-f]{2})"@" {
			  char *p = yytext + 1, x[3];
			  unsigned char *d;
			  int val;
			  yylval.a_value = d = (unsigned char *)malloc( 32 );
			  while ( p ) {
                            strncpy( x, p, 2 ); x[2] = '\0';
			    sscanf( x, "%02x", &val );
			    *d++ = val;
			  }
		          return L_BYTEARRAY; }

	/* strings */

\"[^\"]*\"              { yytext[ strlen( yytext ) - 1 ] = 0;
                          yylval.s_value = strdup( &yytext[ 1 ] );
                          return L_STRING; }
\'[^\']*\'              { yytext[ strlen( yytext ) - 1 ] = 0;
                          yylval.s_value = strdup( &yytext[ 1 ] );
                          return L_STRING; }
[a-z0-9/\~@-_\+=:\.]+  { yylval.s_value = strdup( yytext );
                          return L_STRING; }
$[a-z0-9/\~@-_\+=:\.]+ { yylval.s_value = strdup( getenv( &yytext[ 1 ] ) );
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
