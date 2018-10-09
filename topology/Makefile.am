bin_PROGRAMS = \
	alsatplg

if USE_RST2MAN
man_MANS = alsatplg.1
endif

%.1: %.rst
	rst2man $< > $@

alsatplg_SOURCES = topology.c

AM_CPPFLAGS = \
         -Wall -I$(top_srcdir)/include

alsatplg_LDADD = -lasound

EXTRA_DIST = alsatplg.rst

CLEANFILES = alsatplg.1