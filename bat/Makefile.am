bin_PROGRAMS = alsabat
man_MANS = alsabat.1

EXTRA_DIST = alsabat.1

alsabat_SOURCES = \
	bat.c \
	common.c \
	analyze.c \
	signal.c \
	convert.c \
	alsa.c

noinst_HEADERS = \
	common.h \
	bat-signal.h \
	alsa.h \
	convert.h \
	analyze.h

AM_CPPFLAGS = \
	      -Wall -I$(top_srcdir)/include

alsabat_LDADD = @FFTW_LIB@
