#
# Makefile for Alsa Mixer
#

TOPDIR = ..

include $(TOPDIR)/Makefile.conf

OBJECTS = alsamixer.o

.SUFFIXES:
.SUFFIXES: .o .cpp .c .s .S

.c.o:
	$(CC) $(COPTS) $(INCLUDE) -DNCURSESINC=$(NCURSESINC) -c -o $*.o $<

all: alsamixer

alsamixer: $(OBJECTS)
	$(CPP) -o alsamixer $(OBJECTS) $(SNDLIB) $(NCURSESLIB)

clean:
	rm -f *.o alsamixer *~
