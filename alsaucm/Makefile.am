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

udevrules_DATA = \
	89-alsa-ucm.rules

edit = \
	$(SED) -r -e 's,@bindir\@,$(bindir),g' \
		  -e 's,@mydatadir\@,$(mydatadir),g' \
							< $< > $@ || rm $@

89-alsa-ucm.rules: 89-alsa-ucm.rules.in
	$(edit)

EXTRA_DIST = alsaucm.rst 89-alsa-ucm.rules.in

CLEANFILES = alsaucm.1 89-alsa-ucm.rules
