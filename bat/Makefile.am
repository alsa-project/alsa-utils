bin_PROGRAMS = bat
man_MANS = bat.1

EXTRA_DIST = bat.1

bat_SOURCES = \
	bat.c \
	common.c \
	analyze.c \
	signal.c \
	convert.c \
	alsa.c

noinst_HEADERS = \
	common.h \
	signal.h \
	alsa.h \
	convert.h \
	analyze.h

AM_CPPFLAGS = \
	      -Wall -I$(top_srcdir)/include

bat_LDADD = @FFTW_LIB@
