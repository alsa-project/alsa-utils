#
# Makefile for ALSA Utilities
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@jcu.cz>
#

ifeq (Makefile.conf,$(wildcard Makefile.conf))
include Makefile.conf
else
dummy:
	@echo
	@echo "Please, run configure script as first..."
	@echo
endif


all:
	$(MAKE) -C aplay
	$(MAKE) -C amixer
	$(MAKE) -C alsamixer
	@echo
	@echo "ALSA Utilities were sucessfully compiled."
	@echo

install: all
	$(INSTALL) -s -m 755 -o root -g root aplay/aplay ${bindir}
	ln -sf aplay ${bindir}/arecord
	$(INSTALL) -s -m 755 -o root -g root amixer/amixer ${bindir}
	$(INSTALL) -s -m 755 -o root -g root alsamixer/alsamixer ${bindir}

clean:
	$(MAKE) -C include clean
	$(MAKE) -C aplay clean
	$(MAKE) -C amixer clean
	$(MAKE) -C alsamixer clean
	$(MAKE) -C utils clean
	rm -f core .depend *.o *.orig *~
	rm -f `find . -name "out.txt"`

pack: clean
	rm -f config.cache config.log config.status Makefile.conf
	chown -R root.root ../alsa-utils
	tar cvz -C .. -f ../alsa-utils-$(SND_UTIL_VERSION).tar.gz alsa-utils
