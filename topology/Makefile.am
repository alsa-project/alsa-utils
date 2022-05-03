SUBDIRS = nhlt

bin_PROGRAMS = \
	alsatplg

if USE_RST2MAN
man_MANS = alsatplg.1
endif

%.1: %.rst
	rst2man $< > $@

alsatplg_SOURCES = topology.c pre-processor.c pre-process-class.c pre-process-object.c \
		    pre-process-dapm.c pre-process-dai.c

noinst_HEADERS = topology.h pre-processor.h pre-process-external.h

AM_CPPFLAGS = \
         -Wall -I$(top_srcdir)/include -DALSA_TOPOLOGY_PLUGIN_DIR=\"@ALSA_TOPOLOGY_PLUGIN_DIR@\"

alsatplg_LDADD = $(ALSA_TOPOLOGY_LIBS)

EXTRA_DIST = alsatplg.rst

CLEANFILES = alsatplg.1
