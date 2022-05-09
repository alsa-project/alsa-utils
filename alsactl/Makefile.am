SUBDIRS = init

sbin_PROGRAMS=alsactl
man_MANS=alsactl.1
if USE_XMLTO
man_MANS += alsactl_init.7
endif
EXTRA_DIST=alsactl.1 alsactl_init.xml

AM_CFLAGS = -D_GNU_SOURCE

alsactl_SOURCES=alsactl.c state.c lock.c utils.c init_parse.c init_ucm.c \
		daemon.c monitor.c clean.c info.c

alsactl_CFLAGS=$(AM_CFLAGS) -D__USE_GNU \
               -DSYS_ASOUNDRC=\"$(ASOUND_STATE_DIR)/asound.state\" \
               -DSYS_LOCKPATH=\"$(ASOUND_LOCK_DIR)\" \
               -DSYS_LOCKFILE=\"asound.state.lock\" \
               -DSYS_PIDFILE=\"$(ALSACTL_PIDFILE_DIR)/alsactl.pid\"

noinst_HEADERS=alsactl.h list.h init_sysdeps.c init_utils_string.c \
               init_utils_run.c init_sysfs.c

udevrules_DATA = \
	90-alsa-restore.rules

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
	alsa-state.service \
	alsa-restore.service

install-data-hook:
	$(MKDIR_P) -m 0755 \
		$(DESTDIR)$(systemdsystemunitdir)/sound.target.wants
	( cd $(DESTDIR)$(systemdsystemunitdir)/sound.target.wants && \
		rm -f alsa-state.service alsa-restore.service && \
		$(LN_S) ../alsa-state.service alsa-state.service && \
		$(LN_S) ../alsa-restore.service alsa-restore.service)

endif

edit = \
	$(SED) -r -e 's,@sbindir\@,$(sbindir),g' \
		  -e 's,@mydatadir\@,$(mydatadir),g' \
		  -e 's,@daemonswitch\@,$(ALSACTL_DAEMONSWITCH),g' \
		  -e 's,@asoundrcfile\@,$(ASOUND_STATE_DIR)/asound.state,g' \
							< $< > $@ || rm $@

alsa-state.service: alsa-state.service.in
	$(edit)

alsa-restore.service: alsa-restore.service.in
	$(edit)

90-alsa-restore.rules: 90-alsa-restore.rules.in
	$(edit)

EXTRA_DIST += \
	alsa-state.service.in \
	alsa-restore.service.in \
	90-alsa-restore.rules.in

CLEANFILES = \
	alsa-state.service \
	alsa-restore.service \
	90-alsa-restore.rules

%.7: %.xml
	xmlto man $?
