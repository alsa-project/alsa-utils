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

AM_CPPFLAGS = \
	      -Wall -I$(top_srcdir)/include

bat_LDADD = -lasound
