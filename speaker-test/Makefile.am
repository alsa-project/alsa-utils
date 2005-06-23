SPEAKER_TEST_VERSION = 0.0.8
INCLUDES = -I$(top_srcdir)/include -DSPEAKER_TEST_VERSION=\"$(SPEAKER_TEST_VERSION)\"
SUBDIRS= samples
LDADD = $(LIBINTL)

bin_PROGRAMS = speaker-test
speaker_test_SOURCES = speaker-test.c pink.c
man_MANS = speaker-test.1
EXTRA_DIST = readme.txt speaker-test.1 pink.h

