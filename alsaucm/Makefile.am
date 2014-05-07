bin_PROGRAMS = \
        alsaucm

alsaucm_SOURCES = usecase.c

AM_CPPFLAGS = \
         -Wall -I$(top_srcdir)/include

alsaucm_LDADD = -lasound

# local build
AM_CPPFLAGS += -I$(top_srcdir)/../alsa-lib/include
alsaucm_LDADD += -L$(top_srcdir)/../alsa-lib/src/.libs
