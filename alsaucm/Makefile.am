bin_PROGRAMS = \
        alsaucm

if USE_RST2MAN
man_MANS = alsaucm.1
endif

alsaucm_SOURCES = usecase.c dump.c

noinst_HEADERS = usecase.h

AM_CPPFLAGS = \
         -Wall -I$(top_srcdir)/include

%.1: %.rst
	rst2man $< > $@

EXTRA_DIST = alsaucm.rst

CLEANFILES = alsaucm.1
