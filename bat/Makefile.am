bin_PROGRAMS = bat

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
