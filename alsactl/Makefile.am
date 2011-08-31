SUBDIRS = init

sbin_PROGRAMS=alsactl
man_MANS=alsactl.1
if USE_XMLTO
man_MANS += alsactl_init.7
endif
EXTRA_DIST=alsactl.1 alsactl_init.xml

alsactl_SOURCES=alsactl.c state.c utils.c init_parse.c
alsactl_CFLAGS=$(AM_CFLAGS) -DSYS_ASOUNDRC=\"$(ASOUND_STATE_DIR)/asound.state\"
noinst_HEADERS=alsactl.h list.h init_sysdeps.c init_utils_string.c init_utils_run.c init_sysfs.c

dist_udevrules_DATA = \
	90-alsa-restore.rules

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
	alsa-store.service \
	alsa-restore.service

install-data-hook:
	$(MKDIR_P) -m 0755 \
		$(DESTDIR)$(systemdsystemunitdir)/basic.target.wants \
		$(DESTDIR)$(systemdsystemunitdir)/shutdown.target.wants
	( cd $(DESTDIR)$(systemdsystemunitdir)/basic.target.wants && \
	        rm -f alsa-restore.service && \
		$(LN_S) ../alsa-restore.service alsa-restore.service )
	( cd $(DESTDIR)$(systemdsystemunitdir)/shutdown.target.wants && \
	        rm -f alsa-store.service && \
		$(LN_S) ../alsa-store.service alsa-store.service )

endif

edit = \
	$(SED) -r 's,@sbindir\@,$(sbindir),g' < $< > $@ || rm $@

alsa-store.service: alsa-store.service.in
	$(edit)

alsa-restore.service: alsa-restore.service.in
	$(edit)

90-alsa-restore.rules: 90-alsa-restore.rules.in
	$(edit)

EXTRA_DIST += \
	alsa-store.service.in \
	alsa-restore.service.in \
	90-alsa-restore.rules.in

CLEANFILES = \
	alsa-store.service \
	alsa-restore.service \
	90-alsa-restore.rules

%.7: %.xml
	xmlto man $?
