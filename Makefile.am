INCLUDES=-I$(top_srcdir)/include
SUBDIRS1=include alsactl alsamixer amixer aplay utils
if COND_XAMIXER2
SUBDIRS=$(SUBDIRS1) xamixer2
else
SUBDIRS=$(SUBDIRS1)
endif
EXTRA_DIST=ChangeLog INSTALL README configure cvscompile

rpm: dist
	$(MAKE) -C $@

pack: clean-recursive
	rm -f config.log config.cache config.status
	chown -R root.root ../alsa-utils
	mv ../alsa-utils ../alsa-utils-$(VERSION)
	tar --exclude=CVS -cvz -C .. -f ../alsa-utils-$(VERSION).tar.gz alsa-utils-$(VERSION)
	mv ../alsa-utils-$(VERSION) ../alsa-utils
