#
# Makefile for alsactl program
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@jcu.cz>
#

TOPDIR=..

include $(TOPDIR)/Makefile.conf

CFLAGS=
TARGET=alsactl
OBJECTS=alsactl.o setup.o alsactl_parser.o alsactl_lexer.o

# parser / lexer debug
#CFLAGS += -DYYDEBUG
#FLEXFLAGS = -d
#BISONFLAGS = -v

.c.o:
	$(CC) $(COPTS) $(CFLAGS) $(INCLUDE) -c -o $*.o $<

all: $(TARGET)

$(TARGET): .depend $(OBJECTS)
	$(CC) $(SNDLIB) $(OBJECTS) -o $(TARGET)

alsactl_lexer.c: alsactl_lexer.l
	$(FLEX) $(FLEXFLAGS) -i -o$@ $<

alsactl_parser.c: alsactl_parser.y
	$(BISON) $(BISONFLAGS) -do $@ $<

clean:
	rm -f core alsactl .depend *.o *.orig *~ \
              alsactl_lexer.c alsactl_parser.[ch] alsactl_parser.output

.depend:
	$(CPP) $(COPTS) $(INCLUDE) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
