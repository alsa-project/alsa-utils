INCLUDES=-I$(top_srcdir)/include
SUBDIRS=include alsactl alsamixer amixer aplay seq utils
EXTRA_DIST=ChangeLog INSTALL TODO README configure cvscompile

rpm: dist
	$(MAKE) -C $@

dist-hook:
	-chmod -R a+r $(distdir)
	$(TAR) cvjf $(distdir).tar.bz2 $(distdir)
