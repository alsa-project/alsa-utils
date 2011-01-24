bin_PROGRAMS = \
        alsaucm

alsaucm_SOURCES = usecase.c

INCLUDES = \
         -Wall -I$(top_srcdir)/include

alsaucm_LDADD = -lasound

# local build
INCLUDES += -I$(top_srcdir)/../alsa-lib/include
alsaucm_LDADD += -L$(top_srcdir)/../alsa-lib/src/.libs
