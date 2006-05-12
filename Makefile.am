INCLUDES=-I$(top_srcdir)/include
if ALSAMIXER
ALSAMIXER_DIR=alsamixer
else
ALSAMIXER_DIR=
endif
SUBDIRS= include alsactl alsaconf $(ALSAMIXER_DIR) amidi amixer aplay iecset seq speaker-test utils m4 po
EXTRA_DIST= config.rpath mkinstalldirs ChangeLog INSTALL TODO README configure hgcompile depcomp
AUTOMAKE_OPTIONS=foreign
ACLOCAL_AMFLAGS = -I m4

rpm: dist
	$(MAKE) -C $@

dist-hook:
	-chmod -R a+r $(distdir)
	@if ! test -z "$(AMTAR)"; then \
		$(AMTAR) --create --verbose --file=- $(distdir) | bzip2 -c -9 > $(distdir).tar.bz2 ; \
	else \
		$(TAR) --create --verbose --file=- $(distdir) | bzip2 -c -9 > $(distdir).tar.bz2 ; \
	fi
