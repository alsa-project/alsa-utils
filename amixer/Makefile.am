AM_CFLAGS = -D_GNU_SOURCE
AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = -lm
# LDFLAGS = -static
# CFLAGS += -g -Wall

bin_PROGRAMS = amixer
amixer_SOURCES = amixer.c volume_mapping.c
noinst_HEADERS = amixer.h volume_mapping.h
man_MANS = amixer.1
EXTRA_DIST = amixer.1
