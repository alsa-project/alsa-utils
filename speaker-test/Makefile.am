AM_CPPFLAGS = -I$(top_srcdir)/include
SUBDIRS= samples
LDADD = $(LIBINTL) -lm

bin_PROGRAMS = speaker-test
speaker_test_SOURCES = speaker-test.c pink.c st2095.c
man_MANS = speaker-test.1
EXTRA_DIST = readme.txt speaker-test.1 pink.h st2095.h

