SUBDIRS = init

sbin_PROGRAMS=alsactl
man_MANS=alsactl.1 alsactl_init.7
EXTRA_DIST=alsactl.1 alsactl_init.xml

alsactl_SOURCES=alsactl.c state.c utils.c init_parse.c
noinst_HEADERS=alsactl.h

%.7: %.xml
	xmlto man $?
