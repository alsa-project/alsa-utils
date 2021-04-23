bin_PROGRAMS = \
	alsatplg

if USE_RST2MAN
man_MANS = alsatplg.1
endif

%.1: %.rst
	rst2man $< > $@

alsatplg_SOURCES = topology.c pre-processor.c pre-process-class.c

noinst_HEADERS = topology.h pre-processor.h

AM_CPPFLAGS = \
         -Wall -I$(top_srcdir)/include

alsatplg_LDADD = $(ALSA_TOPOLOGY_LIBS)

EXTRA_DIST = alsatplg.rst

CLEANFILES = alsatplg.1
