INCLUDES=-I$(top_srcdir)/include
SUBDIRS=include alsactl alsamixer amixer aplay seq utils
EXTRA_DIST=ChangeLog INSTALL TODO README configure cvscompile

rpm: dist
	$(MAKE) -C $@

dist-hook:
	-chmod -R a+r $(distdir)
	@if ! test -z "$(AMTAR)"; then \
		$(AMTAR) --create --verbose --bzip2 --file=$(distdir).tar.bz2 $(distdir); \
	else \
		$(TAR) --create --verbose --bzip2 --file=$(distdir).tar.bz2 $(distdir); \
	fi
