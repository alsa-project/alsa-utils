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
	$(MAKE) -C alsactl
	$(MAKE) -C aplay
	$(MAKE) -C amixer
	$(MAKE) -C alsamixer
	@echo
	@echo "ALSA Utilities were sucessfully compiled."
	@echo

install: all
	$(INSTALL) -m 755 -o root -g root -d ${sbindir}
	$(INSTALL) -s -m 755 -o root -g root alsactl/alsactl ${sbindir}
	$(INSTALL) -m 755 -o root -g root -d ${bindir}
	$(INSTALL) -s -m 755 -o root -g root aplay/aplay ${bindir}
	ln -sf aplay ${bindir}/arecord
	$(INSTALL) -s -m 755 -o root -g root amixer/amixer ${bindir}
	$(INSTALL) -d -m 755 -o root -g root ${mandir}/man1
	$(INSTALL) -m 644 -o root -g root amixer/amixer.1 ${mandir}/man1
	$(INSTALL) -s -m 755 -o root -g root alsamixer/alsamixer ${bindir}

clean:
	$(MAKE) -C include clean
	$(MAKE) -C alsactl clean
	$(MAKE) -C aplay clean
	$(MAKE) -C amixer clean
	$(MAKE) -C alsamixer clean
	$(MAKE) -C utils clean
	rm -f core .depend *.o *.orig *~
	rm -f `find . -name "out.txt"`

mrproper: clean
	rm -f config.cache config.log config.status Makefile.conf \
              include/aconfig.h utils/alsa-utils.spec

cvsclean: mrproper
	rm -f configure

pack: mrproper
	chown -R root.root ../alsa-utils
	mv ../alsa-utils ../alsa-utils-$(SND_UTIL_VERSION)
	tar cvz -C .. -f ../alsa-utils-$(SND_UTIL_VERSION).tar.gz alsa-utils-$(SND_UTIL_VERSION)
	mv ../alsa-utils-$(SND_UTIL_VERSION) ../alsa-utils
