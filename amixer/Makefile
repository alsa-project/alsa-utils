#
# Makefile for Mixer
#

TOPDIR = ..

include $(TOPDIR)/Makefile.conf

OBJECTS = amixer.o amain.o

.SUFFIXES:
.SUFFIXES: .o .cpp .c .s .S

.cpp.o:
	$(CXX) $(COPTS) $(INCLUDE) -c -o $*.o $<

all: amixer

amixer.o: amixer.cpp
	$(CXX) $(COPTS) $(INCLUDE) -c -o amixer.o amixer.cpp
amain.o: amain.cpp
	$(CXX) $(COPTS) $(INCLUDE) -c -o amain.o amain.cpp
	
amixer: $(OBJECTS)
	$(CXX) -o amixer $(SNDLIB) $(OBJECTS)

clean:
	rm -f *.o amixer *~
 
