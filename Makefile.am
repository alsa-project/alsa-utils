INCLUDES=-I$(top_srcdir)/include
SUBDIRS1=include alsactl alsamixer amixer aplay seq utils
if COND_XAMIXER2
SUBDIRS2=$(SUBDIRS1) xamixer2
else
SUBDIRS2=$(SUBDIRS1)
endif
if COND_GAMIX
SUBDIRS=$(SUBDIRS2) gamix
else
SUBDIRS=$(SUBDIRS2)
endif
EXTRA_DIST=ChangeLog INSTALL README configure cvscompile

rpm: dist
	$(MAKE) -C $@

pack: clean-recursive
	rm -f config.log config.cache config.status
	rm -rf `find . -name ".deps"`
	chown -R root.root ../alsa-utils
	mv ../alsa-utils ../alsa-utils-$(VERSION)
	tar --exclude=CVS -cvz -C .. -f ../alsa-utils-$(VERSION).tar.gz alsa-utils-$(VERSION)
	mv ../alsa-utils-$(VERSION) ../alsa-utils
