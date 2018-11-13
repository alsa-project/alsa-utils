bin_PROGRAMS = \
	axfer

# To include headers for gettext and version.
AM_CPPFLAGS = \
	-I$(top_srcdir)/include

# Unit tests.
SUBDIRS =

LIBRT = @LIBRT@
LDADD = \
	$(LIBINTL) \
	$(LIBRT)

noinst_HEADERS = \
	misc.h \
	subcmd.h \
	container.h

axfer_SOURCES = \
	misc.h \
	subcmd.h \
	main.c \
	subcmd-list.c \
	container.h \
	container.c \
	container-riff-wave.c
