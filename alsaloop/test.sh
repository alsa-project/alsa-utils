#!/bin/bash

#DBG="gdb --args "
#DBG="strace"
CFGFILE="/tmp/alsaloop.test.cfg"

test1() {
  echo "TEST1"
  $DBG ./alsaloop -C hw:1,0 -P hw:0,0 \
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

case "$1" in
test1) test1 ;;
test2) test2 ;;
*) test1 ;;
esac
