INCLUDES=-I$(top_srcdir)/include
SUBDIRS=include alsactl alsamixer amixer aplay seq utils
EXTRA_DIST=ChangeLog INSTALL TODO README configure cvscompile

rpm: dist
	$(MAKE) -C $@

dist-hook:
	-chmod -R a+r $(distdir)
	$(TAR) cvf $(distdir).tar.bz2 --bzip2 $(distdir)
