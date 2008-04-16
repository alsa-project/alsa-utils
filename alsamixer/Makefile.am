AM_CFLAGS = @CURSES_CFLAGS@ -DCURSESINC="@CURSESINC@"
LDADD = @CURSESLIB@

bin_PROGRAMS = alsamixer
man_MANS = alsamixer.1
EXTRA_DIST = alsamixer.1
alsamixer_INCLUDES = -I$(top_srcdir)/include

#LDFLAGS = -static
#CFLAGS += -g -Wall
