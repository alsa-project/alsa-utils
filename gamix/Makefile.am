bin_PROGRAMS = gamix

INCLUDES = -DLOCALEDIR=\""$(datadir)/locale"\"

CFLAGS += -DPACKAGE=\""alsa-utils\"" -DVERSION=\""$VERSION\""
gamix_SOURCES = gamix.h main.c mkmixer.c probe.c catch.c conf_w.c
gamix_LDFLAGS = $(GTK_LIBS)

EXTRA_DIST = ChangeLog
