INCLUDES=-I$(top_srcdir)/include
SUBDIRS=include alsactl alsamixer amixer aplay seq utils
EXTRA_DIST=ChangeLog INSTALL TODO README configure cvscompile

rpm: dist
	$(MAKE) -C $@

dist-hook:
	-chmod -R a+r $(distdir)
	$(TAR) cvIf $(distdir).tar.bz2 $(distdir)

pack: clean-recursive
	rm -f config.log config.cache config.status
	rm -rf `find . -name ".deps"`
	mv ../alsa-utils ../alsa-utils-$(VERSION)
	tar --exclude=CVS --owner=root --group=root -cvI -C .. -f ../alsa-utils-$(VERSION).tar.bz2 alsa-utils-$(VERSION)
	mv ../alsa-utils-$(VERSION) ../alsa-utils
