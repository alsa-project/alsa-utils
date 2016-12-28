bin_PROGRAMS = \
        alsaucm

if USE_RST2MAN
man_MANS = alsaucm.1
endif

alsaucm_SOURCES = usecase.c

AM_CPPFLAGS = \
         -Wall -I$(top_srcdir)/include

alsaucm_LDADD = -lasound

%.1: %.rst
	rst2man $< > $@

EXTRA_DIST = alsaucm.rst
