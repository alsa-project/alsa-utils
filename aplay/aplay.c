/*
 *  aplay.c - plays and records 
 *
 * 	CREATIVE LABS VOICE-files
 *	Microsoft WAVE-files
 *      SPARC AUDIO .AU-files
 *      Raw Data
 *
 *  Copyright (c) by Jaroslav Kysela <perex@jcu.cz>
 *  Based on vplay program by Michael Beck
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

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/soundlib.h>
#include "config.h"
#include "formats.h"

#define DEFAULT_SPEED 		8000

#define FORMAT_DEFAULT		-1
#define FORMAT_RAW		0
#define FORMAT_VOC		1
#define FORMAT_WAVE		2
#define FORMAT_AU		3

/* global data */

char *command;
void *pcm_handle;
struct snd_pcm_playback_info pinfo;
struct snd_pcm_record_info rinfo;
snd_pcm_format_t rformat, format;
int timelimit    = 0;
int quiet_mode   = 0;
int verbose_mode = 0;
int active_format = FORMAT_DEFAULT;
int direction = SND_PCM_OPEN_PLAYBACK;
char *audiobuf = NULL;
int buffer_size = -1;

int count;
int vocmajor, vocminor;

/* needed prototypes */

static void playback( char *filename );
static void record( char *filename );

static void begin_voc( int fd, u_long count );
static void end_voc( int fd );
static void begin_wave( int fd, u_long count );
static void end_wave( int fd );
static void begin_au( int fd, u_long count );

struct fmt_record {
  void (*start)(int fd, u_long count);
  void (*end) (int fd);
  char *what;
} fmt_rec_table[] = {
  { NULL,       end_wave,     "raw data" },
  { begin_voc,  end_voc,      "VOC" },
  { begin_wave, end_wave,     "WAVE" },
  { begin_au,	end_wave,     "Sparc Audio" }
}; 

static char *get_format( int format )
{
  static char *formats[] = {
    "Mu-Law",
    "A-Law",
    "Ima-ADPCM",
    "Unsigned 8-bit",
    "Signed 16-bit Little Endian",
    "Signed 16-bit Big Endian",
    "Signed 8-bit",
    "Unsigned 16-bit Little Endian",
    "Unsigned 16-bit Big Endian",
    "MPEG",
    "GSM"
  };
  if ( format < 0 || format > SND_PCM_SFMT_GSM )
    return "Unknown";
  return formats[ format ];
}

static void check_new_format( snd_pcm_format_t *format )
{
  if ( direction == SND_PCM_OPEN_PLAYBACK ) {
    if ( pinfo.min_rate > format -> rate || pinfo.max_rate < format -> rate ) {
      fprintf( stderr, "%s: unsupported rate %iHz for playback (valid range is %iHz-%iHz)\n", command, format -> rate, pinfo.min_rate, pinfo.max_rate );
      exit( 1 );
    }
    if ( format -> format != SND_PCM_SFMT_MU_LAW )
      if ( !(pinfo.formats & (1 << format -> format)) ) {
        fprintf( stderr, "%s: requested format %s isn't supported with hardware\n", command, get_format( format -> format ) );
        exit( 1 );
      }
  } else {
    if ( rinfo.min_rate > format -> rate || rinfo.max_rate < format -> rate ) {
      fprintf( stderr, "%s: unsupported rate %iHz for record (valid range is %iHz-%iHz)\n", command, format -> rate, rinfo.min_rate, rinfo.max_rate );
      exit( 1 );
    }
    if ( format -> format != SND_PCM_SFMT_MU_LAW )
      if ( !(rinfo.formats & (1 << format -> format)) ) {
        fprintf( stderr, "%s: requested format %s isn't supported with hardware\n", command, get_format( rformat.format ) );
        exit( 1 );
      }
  }
}

static void usage( char *command )
{
  fprintf (stderr,
"Usage: %s [switches] [filename] <filename> ...\n"
"Available switches:\n"
"\n"
"  -h,--help     help\n"
"  -V,--version  print current version\n"
"  -l            list all soundcards and digital audio devices\n"
"  -c <card>     select card # or card id (1-%i), defaults to 1\n"
"  -d <device>   select device #, defaults to 0\n"
"  -q            quiet mode\n"
"  -v            file format Voc\n"
"  -u            file format Sparc Audio (.au)\n"
"  -w            file format Wave\n"
"  -r            file format Raw\n"
"  -S            stereo\n"
"  -t <secs>     timelimit (seconds)\n"
"  -s <Hz>       speed (Hz)\n"
"  -b <bits>     sample size (8,16 bits)\n"
"  -m            set CD-ROM quality (44100Hz,stereo,16-bit linear)\n"
"  -M            set DAT quality (48000Hz,stereo,16-bit linear)\n"
"  -p <type>     compression type (alaw, ulaw, adpcm)\n"
, command, snd_cards() );
}

static void device_list( void )
{
  void *handle;
  int card, err, dev, idx;
  unsigned int mask;
  struct snd_ctl_hw_info info;
  snd_pcm_info_t pcminfo;
  snd_pcm_playback_info_t playinfo;
  snd_pcm_record_info_t recinfo;
  
  mask = snd_cards_mask();
  if ( !mask ) {
    printf( "%s: no soundcards found...\n", command );
    return;
  }
  for ( card = 0; card < SND_CARDS; card++ ) {
    if ( !(mask & (1 << card)) ) continue;
    if ( (err = snd_ctl_open( &handle, card )) < 0 ) {
      printf( "Error: control open (%i): %s\n", card, snd_strerror( err ) );
      continue;
    }
    if ( (err = snd_ctl_hw_info( handle, &info )) < 0 ) {
      printf( "Error: control hardware info (%i): %s\n", card, snd_strerror( err ) );
      snd_ctl_close( handle );
      continue;
    }
    for ( dev = 0; dev < info.pcmdevs; dev++ ) {
      if ( (err = snd_ctl_pcm_info( handle, dev, &pcminfo )) < 0 ) {
        printf( "Error: control digital audio info (%i): %s\n", card, snd_strerror( err ) );
        continue;
      }
      printf( "%s: %i [%s] / #%i: %s\n",
      		info.name,
      		card + 1,
      		info.id,
      		dev,
      		pcminfo.name );
      printf( "  Directions: %s%s%s\n",
      		pcminfo.flags & SND_PCM_INFO_PLAYBACK ? "playback " : "",
      		pcminfo.flags & SND_PCM_INFO_RECORD ? "record " : "",
      		pcminfo.flags & SND_PCM_INFO_DUPLEX ? "duplex " : "" );
      if ( (err = snd_ctl_pcm_playback_info( handle, dev, &playinfo )) < 0 ) {
        printf( "Error: control digital audio playback info (%i): %s\n", card, snd_strerror( err ) );
        continue;
      }
      if ( pcminfo.flags & SND_PCM_INFO_PLAYBACK ) {
        printf( "  Playback:\n" );
        printf( "    Speed range: %iHz-%iHz\n", playinfo.min_rate, playinfo.max_rate );
        printf( "    Voices range: %i-%i\n", playinfo.min_channels, playinfo.max_channels );
        printf( "    Formats:\n" );
        for ( idx = 0; idx < SND_PCM_SFMT_GSM; idx++ ) {
          if ( playinfo.formats & (1 << idx) )
            printf( "      %s\n", get_format( idx ) ); 
        }
        if ( (err = snd_ctl_pcm_record_info( handle, dev, &recinfo )) < 0 ) {
          printf( "Error: control digital audio record info (%i): %s\n", card, snd_strerror( err ) );
          continue;
        }
      }
      if ( pcminfo.flags & SND_PCM_INFO_RECORD ) {
        printf( "  Record:\n" );
        printf( "    Speed range: %iHz-%iHz\n", recinfo.min_rate, recinfo.max_rate );
        printf( "    Voices range: %i-%i\n", recinfo.min_channels, recinfo.max_channels );
        printf( "    Formats:\n" );
        for ( idx = 0; idx < SND_PCM_SFMT_GSM; idx++ ) {
          if ( recinfo.formats & (1 << idx) )
            printf( "      %s\n", get_format( idx ) ); 
        }
      }
    }
    snd_ctl_close( handle );
  }
}

static void version( void )
{
  printf( "%s: version " SND_UTIL_VERSION " by Jaroslav Kysela <perex@jcu.cz>\n", command );
}

int main( int argc, char *argv[] )
{
  int card, dev, tmp, err, c;

  card = 0;
  dev = 0;
  command = argv[0];
  active_format = FORMAT_DEFAULT;
  if ( strstr( argv[0], "arecord" ) ) {
    direction = SND_PCM_OPEN_RECORD;
    active_format = FORMAT_WAVE;
    command = "Arecord";
  } else if ( strstr( argv[0], "aplay" ) ) {
    direction = SND_PCM_OPEN_PLAYBACK;
    command = "Aplay";
  } else {
    fprintf( stderr, "Error: command should be named either arecord or aplay\n");
    return 1;
  }

  buffer_size = -1;
  memset( &rformat, 0, sizeof( rformat ) ); 
  rformat.format = SND_PCM_SFMT_U8;
  rformat.rate = DEFAULT_SPEED;
  rformat.channels = 1;

  if ( argc > 1 && !strcmp( argv[1], "--help" ) ) {
    usage( command );
    return 0;
  }
  if ( argc > 1 && !strcmp( argv[1], "--version" ) ) {
    version();
    return 0;
  }
  while ( (c = getopt( argc, argv, "hlc:d:qs:So:t:b:vrwuxB:c:p:mM" )) != EOF )
    switch ( c ) {
      case 'h':
        usage( command );
        return 0;
      case 'l':
        device_list();
        return 0;
      case 'c':
        card = snd_card_name( optarg );
        if ( card < 0 ) {
          fprintf( stderr, "Error: soundcard '%s' not found\n", optarg );
          return 1;
        }
        break;
      case 'd':
        dev = atoi( optarg );
        if ( dev < 0 || dev > 32 ) {
          fprintf( stderr, "Error: device %i is invalid\n", dev );
          return 1;
        }
        break;
      case 'S':
	rformat.channels = 2;
	break;
      case 'o':
        tmp = atoi( optarg );
        if ( tmp < 1 || tmp > 32 ) {
          fprintf( stderr, "Error: value %i for channels is invalid\n", tmp );
          return 1;
        }
        break;
      case 'q':
	quiet_mode = 1;
	break;
      case 'r':
        active_format = FORMAT_RAW;
        break;
      case 'v':
        active_format = FORMAT_VOC;
        break;
      case 'w':
        active_format = FORMAT_WAVE;
        break;
      case 'u':
        active_format = FORMAT_AU;
        rformat.format = SND_PCM_SFMT_MU_LAW;
	break;
      case 's':
	tmp = atoi( optarg );
	if ( tmp < 300 )
	  tmp *= 1000;
        rformat.rate = tmp;
        if ( tmp < 2000 || tmp > 128000 ) {
          fprintf( stderr, "Error: bad speed value %i\n", tmp );
          return 1;
        }
	break;
      case 't':
	timelimit = atoi( optarg );
	break;
      case 'b':
	tmp = atoi( optarg );
	switch( tmp ) {
	  case 8:
	    rformat.format = SND_PCM_SFMT_U8;
	    break;
	  case 16:
	    rformat.format = SND_PCM_SFMT_S16_LE;
	    break;
	}
	break;
      case 'x':
        verbose_mode = 1; quiet_mode = 0;
        break;
      case 'p':
	if ( !strcmp( optarg, "alaw" ) ) {
	  rformat.format = SND_PCM_SFMT_A_LAW;
	  active_format = FORMAT_RAW;
	  break;
	} else if ( !strcmp( optarg, "ulaw" ) || !strcmp( optarg, "mulaw" ) ) {
	  rformat.format = SND_PCM_SFMT_MU_LAW;
	  active_format = FORMAT_RAW;
	  break;
	} if ( !strcmp( optarg, "adpcm" ) ) {
	  rformat.format = SND_PCM_SFMT_IMA_ADPCM;
	  active_format = FORMAT_RAW;
	  break;
	}
        fprintf( stderr, "Error: wrong extended format '%s'\n", optarg );
        return 1;
      case 'm':
      case 'M':
        rformat.format = SND_PCM_SFMT_S16_LE;
        rformat.rate = c == 'M' ? 48000 : 44100;
        rformat.channels = 2;
	break;
      case 'V':
        version();
        return 0;
      default:
        usage( command );
	return 1;
    }

  if ( !quiet_mode )
    version();
    
  if ( (err = snd_pcm_open( &pcm_handle, card, dev, direction )) < 0 ) {
    fprintf( stderr, "Error: audio open error: %s\n", snd_strerror( err ) );
    return 1;
  }
  
  if ( direction == SND_PCM_OPEN_PLAYBACK ) {
    if ( (err = snd_pcm_playback_info( pcm_handle, &pinfo )) < 0 ) {
      fprintf( stderr, "Error: playback info error: %s\n", snd_strerror( err ) );
      return 1;
    }
    tmp = pinfo.buffer_size;
    tmp /= 4;			/* 4 fragments are best */
  } else {
    if ( (err = snd_pcm_record_info( pcm_handle, &rinfo )) < 0 ) {
      fprintf( stderr, "Error: record info error: %s\n", snd_strerror( err ) );
      return 1;
    }
    tmp = rinfo.buffer_size;
    tmp /= 8;			/* 8 fragments are best */
  }
  
  buffer_size = tmp;
  if (buffer_size < 512 || buffer_size > 16L * 1024L * 1024L ) {
    fprintf( stderr, "Error: Invalid audio buffer size %d\n", buffer_size );
    return 1;
  }

  check_new_format( &rformat );
  format = rformat;

  if ( (audiobuf = malloc( buffer_size )) == NULL ) {
    fprintf( stderr, "%s: unable to allocate input/output buffer\n", command );
    return 1;
  }

  if ( optind > argc - 1 ) {
    if ( direction == SND_PCM_OPEN_PLAYBACK )
      playback( NULL );
     else
      record( NULL );
  } else {
    while ( optind <= argc - 1 ) {
      if ( direction == SND_PCM_OPEN_PLAYBACK )
        playback( argv[optind++] );
       else
        record( argv[optind++] );
    }
  }
  snd_pcm_close( pcm_handle );
  return 0;
}

/*
 * Test, if it is a .VOC file and return >=0 if ok (this is the length of rest)
 *                                       < 0 if not 
 */
static int test_vocfile(void *buffer)
{
  VocHeader *vp = buffer;

  if (strstr(vp->magic, VOC_MAGIC_STRING) ) {
    vocminor = vp->version & 0xFF;
    vocmajor = vp->version / 256;
    if (vp->version != (0x1233 - vp->coded_ver) )
      return -2;				/* coded version mismatch */
    return vp->headerlen - sizeof(VocHeader);	/* 0 mostly */
  }
  return -1;					/* magic string fail */
}

/*
 * test, if it's a .WAV file, 0 if ok (and set the speed, stereo etc.)
 *                            < 0 if not
 */
static int test_wavefile( void *buffer )
{
  WaveHeader *wp = buffer;

  if (wp->main_chunk == WAV_RIFF && wp->chunk_type == WAV_WAVE &&
      wp->sub_chunk == WAV_FMT && wp->data_chunk == WAV_DATA) {
    if (wp->format != WAV_PCM_CODE) {
      fprintf( stderr, "%s: can't play not PCM-coded WAVE-files\n", command);
      exit( 1 );
    }
    if (wp -> modus < 1 || wp->modus > 32) {
      fprintf(stderr, "%s: can't play WAVE-files with %d tracks\n",
               command, wp->modus);
      exit( 1 );
    }
    format.channels = wp->modus;
    switch ( wp->bit_p_spl ) {
      case 8:
        format.format = SND_PCM_SFMT_U8;
        break;
      case 16:
        format.format = SND_PCM_SFMT_S16_LE;
        break;
      default:
        fprintf(stderr, "%s: can't play WAVE-files with sample %d bits wide\n",
               command, wp->bit_p_spl);
    }
    format.rate = wp -> sample_fq;
    count = wp -> data_length;
    check_new_format( &format );
    return 0;
  }
  return -1;
}

/*
 *
 */
 
static int test_au( int fd, void *buffer )
{
  AuHeader *ap = buffer;
  
  if ( ntohl( ap -> magic ) != AU_MAGIC )
    return -1;
  if ( ntohl( ap -> hdr_size ) > 128 || ntohl( ap -> hdr_size ) < 24 )
    return -1;
  count = ntohl( ap -> data_size );
  switch ( ntohl( ap -> encoding ) ) {
    case AU_FMT_ULAW:
      format.format = SND_PCM_SFMT_MU_LAW;
      break;
    case AU_FMT_LIN8:
      format.format = SND_PCM_SFMT_U8;
      break;
    case AU_FMT_LIN16:
      format.format = SND_PCM_SFMT_U16_LE;
      break;
    default:
      return -1;
  }
  format.rate = ntohl( ap -> sample_rate );
  if ( format.rate < 2000 || format.rate > 256000 ) return -1;
  format.channels = ntohl( ap -> channels );
  if ( format.channels < 1 || format.channels > 128 ) return -1;
  if ( read( fd, buffer + sizeof( AuHeader ), ntohl( ap -> hdr_size ) - sizeof( AuHeader ) ) < 0 ) {
    fprintf( stderr, "%s: read error\n", command );
    exit( 1 );
  }
  check_new_format( &format );
  return 0;
}
 
/*
 *  writing zeros from the zerobuf to simulate silence,
 *  perhaps it's enough to use a long var instead of zerobuf ?
 */
static void write_zeros( unsigned x )
{
  unsigned l;
  char *buf;
 
  buf = (char *)malloc( buffer_size );
  if ( !buf ) {
    fprintf( stderr, "%s: can allocate buffer for zeros\n", command );
    return;	/* not fatal error */
  }
  memset( buf, 128, buffer_size );
  while ( x > 0 ) {
    l = x;
    if ( l > buffer_size ) l = buffer_size;
    if ( snd_pcm_write( pcm_handle, buf, l ) != l ) {
      fprintf( stderr, "%s: write error\n", command );
      exit( 1 );
    }
    x -= l;
  }
} 

static void set_format(void)
{
  unsigned int bps;	/* bytes per second */
  unsigned int size;	/* fragment size */
  struct snd_pcm_playback_params pparams;
  struct snd_pcm_record_params rparams;

  bps = format.rate * format.channels;
  switch ( format.format ) {
    case SND_PCM_SFMT_U16_LE:
    case SND_PCM_SFMT_U16_BE:
      bps <<= 1;
      break;
    case SND_PCM_SFMT_IMA_ADPCM:
      bps >>= 2;
      break;
  }
  bps >>= 2;		/* ok.. this buffer should be 0.25 sec */
  if ( bps < 16 ) bps = 16;
  size = buffer_size;
  while ( size > bps ) size >>= 1;
  if ( size < 16 ) size = 16;

  if ( direction == SND_PCM_OPEN_PLAYBACK ) {
    if ( snd_pcm_playback_format( pcm_handle, &format ) < 0 ) {
      fprintf( stderr, "%s: unable to set playback format %s, %iHz, %i voices\n", command, get_format( format.format ), format.rate, format.channels );
      exit( 1 );
    }
    memset( &pparams, 0, sizeof( pparams ) );
    pparams.fragment_size = size;
    pparams.fragments_max = -1;		/* little trick */
    pparams.fragments_room = 1;
    if ( snd_pcm_playback_params( pcm_handle, &pparams ) < 0 ) {
      fprintf( stderr, "%s: unable to set playback params\n", command );
      exit( 1 );
    }
  } else {
    if ( snd_pcm_record_format( pcm_handle, &format ) < 0 ) {
      fprintf( stderr, "%s: unable to set record format %s, %iHz, %i voices\n", command, get_format( format.format ), format.rate, format.channels );
      exit( 1 );
    }
    memset( &rparams, 0, sizeof( rparams ) );
    rparams.fragment_size = size;
    rparams.fragments_min = 1;
    if ( snd_pcm_record_params( pcm_handle, &rparams ) < 0 ) {
      fprintf( stderr, "%s: unable to set record params\n", command );
      exit( 1 );
    }    
  }
}

/*
 *  ok, let's play a .voc file
 */ 

static void voc_play( int fd, int ofs, char *name )
{
  int l;
  VocBlockType *bp;
  VocVoiceData *vd;
  VocExtBlock *eb;
  u_long nextblock, in_buffer;
  u_char *data = audiobuf;
  char was_extended = 0, output = 0;
  u_short *sp, repeat = 0;
  u_long silence;
  int filepos = 0;

#define COUNT(x)	nextblock -= x; in_buffer -=x ;data += x
#define COUNT1(x)	in_buffer -=x ;data += x

  if ( !quiet_mode ) {
    fprintf( stderr, "Playing Creative Labs Voice file '%s'...\n", name );
  }

  /* first we waste the rest of header, ugly but we don't need seek */
  while ( ofs > buffer_size ) {	
    if ( read( fd, audiobuf, buffer_size ) != buffer_size ) {
      fprintf( stderr, "%s: read error\n", command );
      exit( 1 );
    }
    ofs -= buffer_size;
  }
  if (ofs)
    if ( read( fd, audiobuf, ofs ) != ofs ) {
      fprintf( stderr, "%s: read error\n", command );
      exit( 1 );
    }

  format.format = SND_PCM_SFMT_U8;
  format.channels = 1;
  format.rate = DEFAULT_SPEED;
  set_format();

  in_buffer = nextblock = 0;
  while (1) {
    Fill_the_buffer:		/* need this for repeat */
    if ( in_buffer < 32 ) {
      /* move the rest of buffer to pos 0 and fill the audiobuf up */
      if (in_buffer)
        memcpy (audiobuf, data, in_buffer);
      data = audiobuf;
      if ((l = read (fd, audiobuf + in_buffer, buffer_size - in_buffer) ) > 0) 
        in_buffer += l;
      else if (! in_buffer) {
	/* the file is truncated, so simulate 'Terminator' 
           and reduce the datablock for save landing */
        nextblock = audiobuf[0] = 0;
        if (l == -1) {
          perror (name);
          exit (-1);
        }
      }
    }
    while (! nextblock) { 	/* this is a new block */
      if (in_buffer<sizeof(VocBlockType)) return;
      bp = (VocBlockType *)data; COUNT1(sizeof (VocBlockType));
      nextblock = VOC_DATALEN(bp);
      if (output && !quiet_mode)
        fprintf (stderr, "\n");	/* write /n after ASCII-out */
      output = 0;
      switch (bp->type) {
        case 0:
#if 0
          d_printf ("Terminator\n");
#endif
          return;		/* VOC-file stop */
        case 1:
          vd = (VocVoiceData *)data; COUNT1(sizeof(VocVoiceData));
          /* we need a SYNC, before we can set new SPEED, STEREO ... */

          if (! was_extended) {
            format.rate = (int)(vd->tc);
            format.rate = 1000000 / (256 - format.rate); 
#if 0
            d_printf ("Voice data %d Hz\n", dsp_speed);
#endif
            if (vd->pack) {	/* /dev/dsp can't it */
              fprintf (stderr, "%s: can't play packed .voc files\n", command);
              return;
            }
            if (format.channels == 2) {	/* if we are in Stereo-Mode, switch back */
              format.channels = 1;
              set_format();
            }
          }
          else {		/* there was extended block */
            format.channels = 2;
            was_extended = 0;
          }
          set_format();
          break;
        case 2:			/* nothing to do, pure data */
#if 0
          d_printf ("Voice continuation\n");
#endif
          break;
        case 3:			/* a silence block, no data, only a count */
          sp = (u_short *)data; COUNT1(sizeof(u_short));
          format.rate = (int)(*data); COUNT1(1);
          format.rate = 1000000 / (256 - format.rate);
          set_format();
          silence = ( ((u_long)*sp) * 1000) / format.rate; 
#if 0
          d_printf ("Silence for %d ms\n", (int)silence);
#endif
          write_zeros (*sp);
          snd_pcm_flush_playback( pcm_handle );
          break;
        case 4:			/* a marker for syncronisation, no effect */
          sp = (u_short *)data; COUNT1(sizeof(u_short));
#if 0
          d_printf ("Marker %d\n", *sp); 
#endif
          break;
        case 5:			/* ASCII text, we copy to stderr */
          output = 1;
#if 0
          d_printf ("ASCII - text :\n");
#endif
          break; 
        case 6:			/* repeat marker, says repeatcount */
          /* my specs don't say it: maybe this can be recursive, but
             I don't think somebody use it */
          repeat = *(u_short *)data; COUNT1(sizeof(u_short));
#if 0
          d_printf ("Repeat loop %d times\n", repeat);
#endif
          if (filepos >= 0)	/* if < 0, one seek fails, why test another */
            if ( (filepos = lseek (fd, 0, 1)) < 0 ) {
              fprintf(stderr, "%s: can't play loops; %s isn't seekable\n", 
                      command, name);
              repeat = 0;
            }
            else
              filepos -= in_buffer;	/* set filepos after repeat */
          else
            repeat = 0;
          break;
        case 7:			/* ok, lets repeat that be rewinding tape */
          if (repeat) {
            if (repeat != 0xFFFF) {
#if 0
              d_printf ("Repeat loop %d\n", repeat);
#endif
              --repeat;
            }
#if 0
            else
              d_printf ("Neverending loop\n");
#endif
            lseek (fd, filepos, 0);
            in_buffer = 0;		/* clear the buffer */
            goto Fill_the_buffer;
          }
#if 0
          else
            d_printf ("End repeat loop\n");
#endif
          break;
        case 8:			/* the extension to play Stereo, I have SB 1.0 :-( */
          was_extended = 1;
          eb = (VocExtBlock *)data; COUNT1(sizeof(VocExtBlock));
          format.rate = (int)(eb->tc);
          format.rate = 256000000L / (65536 - format.rate);
          format.channels = eb->mode == VOC_MODE_STEREO ? 2 : 1;
          if (format.channels == 2) 
            format.rate = format.rate >> 1;
          if (eb->pack) {     /* /dev/dsp can't it */
            fprintf (stderr, "%s: can't play packed .voc files\n", command);
            return;
          }
#if 0
          d_printf ("Extended block %s %d Hz\n", 
                    (eb->mode ? "Stereo" : "Mono"), dsp_speed);
#endif
          break;
        default:
          fprintf (stderr, "%s: unknown blocktype %d. terminate.\n", 
                   command, bp->type);
          return;
      } 		/* switch (bp->type) */
    }			/* while (! nextblock)  */
    /* put nextblock data bytes to dsp */
    l = in_buffer;
    if ( nextblock < l ) l = nextblock;
    if (l) {  
      if (output && !quiet_mode) {
        if ( write( 2, data, l) != l ) {	/* to stderr */
          fprintf( stderr, "%s: write error\n", command );
          exit(1);
        }
      } else {
        if (snd_pcm_write(pcm_handle, data, l) != l) {
          fprintf( stderr, "%s: write error\n", command );
          exit(1);
        }
      }
      COUNT(l);
    }
  }			/* while(1) */
}
/* that was a big one, perhaps somebody split it :-) */

/* setting the globals for playing raw data */
static void init_raw_data(void)
{
  format = rformat;
}

/* calculate the data count to read from/to dsp */
static u_long calc_count(void)
{
  u_long count;

  if (!timelimit)
    count = 0x7fffffff;
  else {
    count = timelimit * format.rate * format.channels;
    switch ( format.format ) {
      case SND_PCM_SFMT_S16_LE:
      case SND_PCM_SFMT_S16_BE:
      case SND_PCM_SFMT_U16_LE:
      case SND_PCM_SFMT_U16_BE:
        count *= 2;
        break;
      case SND_PCM_SFMT_IMA_ADPCM:
        count /= 4;
        break;
    }
  }
  return count;
}

/* write a .VOC-header */ 
static void begin_voc( int fd, u_long cnt )
{
  VocHeader vh;
  VocBlockType bt;
  VocVoiceData vd;
  VocExtBlock eb;
 
  strncpy( vh.magic, VOC_MAGIC_STRING, 20 );
  vh.magic[19] = 0x1A;
  vh.headerlen = sizeof( VocHeader );
  vh.version = VOC_ACTUAL_VERSION;
  vh.coded_ver = 0x1233 - VOC_ACTUAL_VERSION;

  if ( write( fd, &vh, sizeof(VocHeader) ) != sizeof(VocHeader) ) {
    fprintf( stderr, "%s: write error\n", command );
    exit(1);
  }

  if (format.channels > 1) {
    /* write a extended block */
    bt.type = 8;
    bt.datalen = 4;
    bt.datalen_m = bt.datalen_h = 0;
    if ( write (fd, &bt, sizeof(VocBlockType)) != sizeof( VocBlockType ) ) {
      fprintf( stderr, "%s: write error\n", command );
      exit(1);
    }
    eb.tc = (u_short)(65536 - 256000000L / (format.rate << 1));
    eb.pack = 0;
    eb.mode = 1;
    if ( write(fd, &eb, sizeof(VocExtBlock)) != sizeof(VocExtBlock) ) {
      fprintf( stderr, "%s: write error\n", command );
      exit(1);
    }
  }
  bt.type = 1;
  cnt += sizeof(VocVoiceData);	/* Voice_data block follows */
  bt.datalen   = (u_char)  (cnt & 0xFF);
  bt.datalen_m = (u_char)( (cnt & 0xFF00) >> 8 );
  bt.datalen_h = (u_char)( (cnt & 0xFF0000) >> 16 );
  if ( write (fd, &bt, sizeof(VocBlockType)) != sizeof( VocBlockType ) ) {
    fprintf( stderr, "%s: write error\n", command );
    exit(1);
  }
  vd.tc = (u_char)(256 - (1000000 / format.rate) );
  vd.pack = 0;
  if ( write (fd, &vd, sizeof(VocVoiceData) ) != sizeof( VocVoiceData ) ) {
    fprintf( stderr, "%s: write error\n", command );
    exit(1);
  }
} 

/* write a WAVE-header */
static void begin_wave(int fd, u_long cnt)
{
  WaveHeader wh;
  int bits;

  switch ( format.format ) {
    case SND_PCM_SFMT_U8:
      bits = 8;
      break;
    case SND_PCM_SFMT_S16_LE:
      bits = 16;
      break;
    default:
      fprintf( stderr, "%s: Wave doesn't support %s format...\n", command, get_format( format.format ) );
      exit( 1 );
  }
  wh.main_chunk = WAV_RIFF;
  wh.length     = cnt + sizeof(WaveHeader) - 8; 
  wh.chunk_type = WAV_WAVE;
  wh.sub_chunk  = WAV_FMT;
  wh.sc_len     = 16;
  wh.format     = WAV_PCM_CODE;
  wh.modus      = format.channels;
  wh.sample_fq  = format.rate;
#if 0
  wh.byte_p_spl = (samplesize == 8) ? 1 : 2;
  wh.byte_p_sec = dsp_speed * wh.modus * wh.byte_p_spl;
#else
  wh.byte_p_spl = wh.modus * ((bits + 7) / 8);
  wh.byte_p_sec = wh.byte_p_spl * format.rate;
#endif
  wh.bit_p_spl  = bits;
  wh.data_chunk = WAV_DATA;
  wh.data_length= cnt;
  if ( write (fd, &wh, sizeof(WaveHeader)) != sizeof( WaveHeader ) ) {
    fprintf( stderr, "%s: write error\n", command );
    exit(1);
  }
}

/* write a Au-header */
static void begin_au(int fd, u_long cnt)
{
  AuHeader ah;

  ah.magic = htonl( AU_MAGIC );
  ah.hdr_size = htonl( 24 );
  ah.data_size = htonl( cnt );
  switch ( format.format ) {
    case SND_PCM_SFMT_MU_LAW:
      ah.encoding = htonl( AU_FMT_ULAW );
      break;
    case SND_PCM_SFMT_U8:
      ah.encoding = htonl( AU_FMT_LIN8 );
      break;
    case SND_PCM_SFMT_U16_LE:
      ah.encoding = htonl( AU_FMT_LIN16 );
      break;
    default:
      fprintf( stderr, "%s: Sparc Audio doesn't support %s format...\n", command, get_format( format.format ) );
      exit( 1 );
  }
  ah.sample_rate = htonl( format.rate );
  ah.channels = htonl( format.channels );
  if ( write (fd, &ah, sizeof(AuHeader)) != sizeof( AuHeader ) ) {
    fprintf( stderr, "%s: write error\n", command );
    exit(1);
  }
}

/* closing .VOC */
static void end_voc(int fd)
{
  char dummy = 0;		/* Write a Terminator */
  if ( write (fd, &dummy, 1) != 1 ) {
    fprintf( stderr, "%s: write error", command );
    exit( 1 );
  }
  if (fd != 1)
    close (fd);
}

static void end_wave(int fd)
{				/* only close output */
  if (fd != 1)
    close (fd);
}

static void header( int rtype, char *name )
{
  if (!quiet_mode) {
    fprintf (stderr, "%s %s '%s' : ", 
             (direction == SND_PCM_OPEN_PLAYBACK) ? "Playing" : "Recording",
             fmt_rec_table[rtype].what, 
             name );
    fprintf (stderr, "%s, ", get_format( format.format ) );
    fprintf (stderr, "Speed %d Hz, ", format.rate);
    if ( format.channels == 1 )
      fprintf (stderr, "Mono" ); 
    else if ( format.channels == 2 ) 
      fprintf (stderr, "Stereo" );
    else fprintf (stderr, "Voices %i", format.channels);
    fprintf (stderr, "\n" );
  }
}

/* playing raw data */

void playback_go (int fd, int loaded, u_long count, int rtype, char *name)
{
  int l;
  u_long c;

  header( rtype, name );
  set_format();

  while (count) {
    c = count;

    if (c > buffer_size)
      c = buffer_size;

    if ((l = read (fd, audiobuf + loaded, c - loaded)) > 0) {
      l += loaded; loaded = 0;	/* correct the count; ugly but ... */
#if 0
      sleep( 1 );
#endif
      if (snd_pcm_write(pcm_handle, audiobuf, l) != l) {
	fprintf( stderr, "write error\n" );
	exit (1);
      }
      count -= l;
    } else {
      if (l == -1) { 
	perror (name);
        exit (-1);
      }
      count = 0;	/* Stop */
    }
  }			/* while (count) */
}

/* recording raw data, this proc handels WAVE files and .VOCs (as one block) */ 

void record_go(int fd, int loaded, u_long count, int rtype, char *name)
{
  int l;
  u_long c;

  header( rtype, name );
  set_format();

  while (count) {
    c = count;
    if (c > buffer_size)
      c = buffer_size;

    if ((l = snd_pcm_read(pcm_handle, audiobuf, c)) > 0) {
      if (write (fd, audiobuf, l) != l) {
	perror (name);
	exit (-1);
      }
      count -= l;
    }

    if (l == -1) {
      fprintf( stderr, "read error\n" );
      exit (-1);
    }
  }
}

/*
 *  let's play or record it (record_type says VOC/WAVE/raw)
 */

static void playback(char *name)
{
  int fd, ofs;

  snd_pcm_flush_playback( pcm_handle );
  if (!name) {
    fd = 0;
    name = "stdin";
  } else {
    if ((fd = open (name, O_RDONLY, 0)) == -1) {
      perror (name);
      exit(1);
    }
  }
  /* read the file header */
  if ( read( fd, audiobuf, sizeof(AuHeader) ) != sizeof( AuHeader ) ) {
    fprintf( stderr, "%s: read error", command );
    exit( 1 );
  }
  if ( test_au( fd, audiobuf ) >= 0 ) {
    playback_go( fd, 0, count, FORMAT_AU, name );
    goto __end;
  }
  if ( read( fd, audiobuf + sizeof(AuHeader),
  			sizeof(VocHeader)-sizeof(AuHeader) ) !=
                        sizeof(VocHeader)-sizeof(AuHeader) ) {
    fprintf( stderr, "%s: read error", command );
    exit( 1 );
  }
  if ( (ofs = test_vocfile (audiobuf) ) >= 0) {
    voc_play (fd, ofs, name);
    goto __end;
  }
  /* read bytes for WAVE-header */
  if ( read (fd, audiobuf + sizeof(VocHeader), 
           		sizeof(WaveHeader) - sizeof(VocHeader) ) !=
            		sizeof(WaveHeader) - sizeof(VocHeader) ) {
    fprintf( stderr, "%s: read error", command );
    exit( 1 );
  }
  if (test_wavefile (audiobuf) >= 0) {
    playback_go(fd, 0, count, FORMAT_WAVE, name);
  } else {
    /* should be raw data */
    init_raw_data();
    count = calc_count();
    playback_go(fd, sizeof(WaveHeader), count, FORMAT_RAW, name);
  }
  __end:
  if (fd != 0)
    close(fd);
} 

static void record(char *name)
{
  int fd;

  snd_pcm_flush_record( pcm_handle );
  if (!name) {
    fd = 1;
    name = "stdout";
  } else {
    remove( name );
    if ((fd = open (name, O_WRONLY | O_CREAT, 0644)) == -1) {
      perror (name);
      exit(1);
    }
  }
  count = calc_count() & 0xFFFFFFFE;
  /* WAVE-file should be even (I'm not sure), but wasting one byte
     isn't a problem (this can only be in 8 bit mono) */
  if (fmt_rec_table[active_format].start)
    fmt_rec_table[active_format].start(fd, count);
  record_go( fd, 0, count, active_format, name);
  fmt_rec_table[active_format].end(fd);
} 
