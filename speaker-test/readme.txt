To make or build just type

make

To test: -
1) Just stereo sound from one stereo jack: -
./speaker-test -Dfront -c2
2) A 4 speaker setup from two stereo jacks: -
./speaker-test -Dsurround40 -c4
3) A 5.1 speaker setup from three stereo jacks: -
./speaker-test -Dsurround51 -c6
4) To send a nice low 75Hz tone to the Woofer and then exit without touching any other speakers: -
./speaker-test -Dplug:surround51 -c6 -s1 -f75

