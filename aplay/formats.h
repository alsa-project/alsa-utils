#ifndef FORMATS_H
#define FORMATS_H		1

#include <sys/types.h>

/* Definitions for .VOC files */

#define VOC_MAGIC_STRING	"Creative Voice File\0x1A"
#define VOC_ACTUAL_VERSION	0x010A
#define VOC_SAMPLESIZE		8

#define VOC_MODE_MONO		0
#define VOC_MODE_STEREO		1

#define VOC_DATALEN(bp)		((u_long)(bp->datalen) | \
                         	((u_long)(bp->datalen_m) << 8) | \
                         	((u_long)(bp->datalen_h) << 16) )

typedef struct voc_header {
	u_char magic[20];	/* must be MAGIC_STRING */
	u_short headerlen;	/* Headerlength, should be 0x1A */
	u_short version;	/* VOC-file version */
	u_short coded_ver;	/* 0x1233-version */
} VocHeader;

typedef struct voc_blocktype {
	u_char type;
	u_char datalen;		/* low-byte    */
	u_char datalen_m;	/* medium-byte */
	u_char datalen_h;	/* high-byte   */
} VocBlockType;

typedef struct voc_voice_data {
	u_char tc;
	u_char pack;
} VocVoiceData;

typedef struct voc_ext_block {
	u_short tc;
	u_char pack;
	u_char mode;
} VocExtBlock;

/* Definitions for Microsoft WAVE format */

#define WAV_RIFF		0x46464952
#define WAV_WAVE		0x45564157
#define WAV_FMT			0x20746D66
#define WAV_DATA		0x61746164
#define WAV_PCM_CODE		1

/* it's in chunks like .voc and AMIGA iff, but my source say there
   are in only in this combination, so I combined them in one header;
   it works on all WAVE-file I have
 */
typedef struct wav_header {
	u_int main_chunk;	/* 'RIFF' */
	u_int length;		/* filelen */
	u_int chunk_type;	/* 'WAVE' */

	u_int sub_chunk;	/* 'fmt ' */
	u_int sc_len;		/* length of sub_chunk, =16 */
	u_short format;		/* should be 1 for PCM-code */
	u_short modus;		/* 1 Mono, 2 Stereo */
	u_int sample_fq;	/* frequence of sample */
	u_int byte_p_sec;
	u_short byte_p_spl;	/* samplesize; 1 or 2 bytes */
	u_short bit_p_spl;	/* 8, 12 or 16 bit */

	u_int data_chunk;	/* 'data' */
	u_int data_length;	/* samplecount */
} WaveHeader;

/* Definitions for Sparc .au header */

#define AU_MAGIC		0x2e736e64

#define AU_FMT_ULAW		1
#define AU_FMT_LIN8		2
#define AU_FMT_LIN16		3

typedef struct au_header {
	u_int magic;		/* magic '.snd' */
	u_int hdr_size;		/* size of header (min 24) */
	u_int data_size;	/* size of data */
	u_int encoding;		/* see to AU_FMT_XXXX */
	u_int sample_rate;	/* sample rate */
	u_int channels;		/* number of channels (voices) */
} AuHeader;

#endif				/* FORMATS */
