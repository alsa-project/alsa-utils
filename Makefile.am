INCLUDES=-I$(top_srcdir)/include
SUBDIRS=include alsactl alsamixer amixer aplay utils

rpm: dist
	$(MAKE) -C $@

pack: clean-recursive
	chown -R root.root ../alsa-utils
	mv ../alsa-utils ../alsa-utils-$(VERSION)
	tar --exclude=CVS -cvz -C .. -f ../alsa-utils-$(VERSION).tar.gz alsa-utils-$(VERSION)
	mv ../alsa-utils-$(VERSION) ../alsa-utils
