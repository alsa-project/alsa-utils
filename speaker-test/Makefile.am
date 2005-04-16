SPEAKER_TEST_VERSION = 0.0.8
INCLUDES = -I$(top_srcdir)/include -DVERSION=\"$(SPEAKER_TEST_VERSION)\"
SUBDIRS= samples

bin_PROGRAMS = speaker-test
speaker_test_SOURCES = speaker-test.c pink.c
man_MANS = speaker-test.1
EXTRA_DIST = readme.txt speaker-test.1 pink.h

