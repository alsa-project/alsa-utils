#ifndef FORMATS_H
#define FORMATS_H		1

#include "bswap.h"

/* Definitions for .VOC files */

#define VOC_MAGIC_STRING	"Creative Voice File\x1A"
#define VOC_ACTUAL_VERSION	0x010A
#define VOC_SAMPLESIZE		8

#define VOC_MODE_MONO		0
#define VOC_MODE_STEREO		1

#define VOC_DATALEN(bp)		((u_long)(bp->datalen) | \
                         	((u_long)(bp->datalen_m) << 8) | \
                         	((u_long)(bp->datalen_h) << 16) )

typedef struct voc_header {
	unsigned char magic[20];	/* must be MAGIC_STRING */
	unsigned short headerlen;	/* Headerlength, should be 0x1A */
	unsigned short version;	/* VOC-file version */
	unsigned short coded_ver;	/* 0x1233-version */
} VocHeader;

typedef struct voc_blocktype {
	unsigned char type;
	unsigned char datalen;		/* low-byte    */
	unsigned char datalen_m;	/* medium-byte */
	unsigned char datalen_h;	/* high-byte   */
} VocBlockType;

typedef struct voc_voice_data {
	unsigned char tc;
	unsigned char pack;
} VocVoiceData;

typedef struct voc_ext_block {
	unsigned short tc;
	unsigned char pack;
	unsigned char mode;
} VocExtBlock;

/* Definitions for Microsoft WAVE format */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define LE_SHORT(v)		(v)
#define LE_INT(v)		(v)
#define BE_SHORT(v)		bswap_16(v)
#define BE_INT(v)		bswap_32(v)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define COMPOSE_ID(a,b,c,d)	((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define LE_SHORT(v)		bswap_16(v)
#define LE_INT(v)		bswap_32(v)
#define BE_SHORT(v)		(v)
#define BE_INT(v)		(v)
#else
#error "Wrong endian"
#endif

/* Note: the following macros evaluate the parameter v twice */
#define TO_CPU_SHORT(v, be) \
	((be) ? BE_SHORT(v) : LE_SHORT(v))
#define TO_CPU_INT(v, be) \
	((be) ? BE_INT(v) : LE_INT(v))

#define WAV_RIFF		COMPOSE_ID('R','I','F','F')
#define WAV_RIFX		COMPOSE_ID('R','I','F','X')
#define WAV_WAVE		COMPOSE_ID('W','A','V','E')
#define WAV_FMT			COMPOSE_ID('f','m','t',' ')
#define WAV_DATA		COMPOSE_ID('d','a','t','a')

/* WAVE fmt block constants from Microsoft mmreg.h header */
#define WAV_FMT_PCM             0x0001
#define WAV_FMT_IEEE_FLOAT      0x0003
#define WAV_FMT_DOLBY_AC3_SPDIF 0x0092
#define WAV_FMT_EXTENSIBLE      0xfffe

/* Used with WAV_FMT_EXTENSIBLE format */
#define WAV_GUID_TAG		"\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71"

/* it's in chunks like .voc and AMIGA iff, but my source say there
   are in only in this combination, so I combined them in one header;
   it works on all WAVE-file I have
 */
typedef struct {
	unsigned int magic;		/* 'RIFF' */
	unsigned int length;		/* filelen */
	unsigned int type;		/* 'WAVE' */
} WaveHeader;

typedef struct {
	unsigned short format;		/* see WAV_FMT_* */
	unsigned short channels;
	unsigned int sample_fq;	/* frequence of sample */
	unsigned int byte_p_sec;
	unsigned short byte_p_spl;	/* samplesize; 1 or 2 bytes */
	unsigned short bit_p_spl;	/* 8, 12 or 16 bit */
} WaveFmtBody;

typedef struct {
	WaveFmtBody format;
	unsigned short ext_size;
	unsigned short bit_p_spl;
	unsigned int channel_mask;
	unsigned short guid_format;	/* WAV_FMT_* */
	unsigned char guid_tag[14];	/* WAV_GUID_TAG */
} WaveFmtExtensibleBody;

typedef struct {
	unsigned int type;		/* 'data' */
	unsigned int length;		/* samplecount */
} WaveChunkHeader;

/* Definitions for Sparc .au header */

#define AU_MAGIC		COMPOSE_ID('.','s','n','d')

#define AU_FMT_ULAW		1
#define AU_FMT_LIN8		2
#define AU_FMT_LIN16		3

typedef struct au_header {
	unsigned int magic;		/* '.snd' */
	unsigned int hdr_size;		/* size of header (min 24) */
	unsigned int data_size;	/* size of data */
	unsigned int encoding;		/* see to AU_FMT_XXXX */
	unsigned int sample_rate;	/* sample rate */
	unsigned int channels;		/* number of channels (voices) */
} AuHeader;

#endif				/* FORMATS */
