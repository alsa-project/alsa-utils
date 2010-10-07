#!/bin/bash

#DBG="gdb --args "
#DBG="strace"
CFGFILE="/tmp/alsaloop.test.cfg"

test1() {
  echo "TEST1"
  $DBG ./alsaloop -C hw:1,0 -P plughw:0,0 \
    --tlatency 50000 \
    --mixer "name='Master Playback Volume'@name='Master Playback Volume'" \
    --mixer "name='Master Playback Switch'@name='Master Playback Switch'" \
    --mixer "name='PCM Playback Volume'"
}

test2() {
  echo "TEST2"
cat > $CFGFILE <<EOF
# first job
-C hw:1,0,0 -P hw:0,0,0 --tlatency 50000 --thread 1 \
    --mixer "name='Master Playback Volume'@name='Master Playback Volume'" \
    --mixer "name='Master Playback Switch'@name='Master Playback Switch'" \
    --mixer "name='PCM Playback Volume'"
# next line - second job
-C hw:1,0,1 -P hw:0,1,0 --tlatency 50000 --thread 2
EOF
  $DBG ./alsaloop -d --config $CFGFILE
}

test3() {
  echo "TEST3"
cat > $CFGFILE <<EOF
-C hw:1,0,0 -P plug:dmix:0 --tlatency 50000 --thread 0 \
    --mixer "name='Master Playback Volume'@name='Master Playback Volume'" \
    --mixer "name='Master Playback Switch'@name='Master Playback Switch'" \
    --mixer "name='PCM Playback Volume'"
-C hw:1,0,1 -P plug:dmix:0 --tlatency 50000 --thread 1
-C hw:1,0,2 -P plug:dmix:0 --tlatency 50000 --thread 2
-C hw:1,0,3 -P plug:dmix:0 --tlatency 50000 --thread 3
-C hw:1,0,4 -P plug:dmix:0 --tlatency 50000 --thread 4
-C hw:1,0,5 -P plug:dmix:0 --tlatency 50000 --thread 5
-C hw:1,0,6 -P plug:dmix:0 --tlatency 50000 --thread 6
-C hw:1,0,7 -P plug:dmix:0 --tlatency 50000 --thread 7
EOF
  $DBG ./alsaloop --config $CFGFILE
}

test4() {
  echo "TEST4"
  $DBG ./alsaloop -C hw:1,0 -P plughw:0,0 -a off -r 11025 \
    --tlatency 50000 \
    --mixer "name='Master Playback Volume'@name='Master Playback Volume'" \
    --mixer "name='Master Playback Switch'@name='Master Playback Switch'" \
    --mixer "name='PCM Playback Volume'"
}

case "$1" in
test1) test1 ;;
test2) test2 ;;
test3) test3 ;;
test4) test4 ;;
*) test1 ;;
esac
