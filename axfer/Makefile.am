bin_PROGRAMS = \
	axfer

man_MANS = \
	axfer.1 \
	axfer-list.1 \
	axfer-transfer.1

# To include headers for gettext and version.
AM_CPPFLAGS = \
	-I$(top_srcdir)/include

# Unit tests.
SUBDIRS = \
	test

LIBRT = @LIBRT@
LDADD = \
	$(LIBINTL) \
	$(LIBRT)

noinst_HEADERS = \
	misc.h \
	subcmd.h \
	container.h \
	mapper.h \
	xfer.h \
	xfer-libasound.h \
	frame-cache.h
	waiter.h

axfer_SOURCES = \
	misc.h \
	subcmd.h \
	main.c \
	subcmd-list.c \
	container.h \
	container.c \
	container-riff-wave.c \
	container-au.c \
	container-voc.c \
	container-raw.c \
	mapper.h \
	mapper.c \
	mapper-single.c \
	mapper-multiple.c \
	xfer.h \
	xfer.c \
	xfer-options.c \
	xfer-libasound.h \
	xfer-libasound.c \
	frame-cache.h \
	frame-cache.c \
	xfer-libasound-irq-rw.c \
	subcmd-transfer.c \
	xfer-libasound-irq-mmap.c \
	waiter.h \
	waiter.c \
	waiter-poll.c \
	waiter-select.c \
	waiter-epoll.c \
	xfer-libasound-timer-mmap.c

if HAVE_FFADO
axfer_SOURCES += xfer-libffado.c
LDADD += -lffado
endif

EXTRA_DIST = \
	axfer.1 \
	axfer-list.1 \
	axfer-transfer.1
