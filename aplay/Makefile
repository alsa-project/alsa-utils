#
# Makefile for aplay program
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@jcu.cz>
#

TOPDIR=..

include $(TOPDIR)/Makefile.conf

TARGET=aplay
TARGET1=arecord
OBJECTS=aplay.o

.c.o:
	$(CC) $(COPTS) $(INCLUDE) -c -o $*.o $<

all: $(TARGET)

$(TARGET): .depend $(OBJECTS)
	$(CC) $(SNDLIB) $(OBJECTS) -o $(TARGET)
	ln -sf $(TARGET) $(TARGET1)

clean:
	rm -f core .depend *.o *.orig *~

.depend:
	$(CPP) $(COPTS) $(INCLUDE) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
