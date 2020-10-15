#!/bin/bash

cd `dirname $0`
dir=`pwd`

TOOLCHAIN_DIR=/home/viorel/work/opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu

if test -d ../alsa-lib/utils && ! test -r `aclocal --print-ac-dir`/alsa.m4; then
  alsa_m4_flags="-I ../alsa-lib/utils"
fi
aclocal $alsa_m4_flags $ACLOCAL_FLAGS
# save original files to avoid stupid modifications by gettextize
cp Makefile.am Makefile.am.ok
cp configure.ac configure.ac.ok
gettextize -c -f --no-changelog
echo "EXTRA_DIST = gettext.m4" > m4/Makefile.am
cp Makefile.am.ok Makefile.am
cp configure.ac.ok configure.ac
aclocal -I m4 $alsa_m4_flags
autoheader -f
automake -f --foreign --copy --add-missing
touch depcomp		# for older automake
autoconf -f
export CFLAGS='-O2 -Wall -pipe -g'
export LDFLAGS="-L$dir/../alsa-bin/lib"
export PATH="$TOOLCHAIN_DIR/bin:$PATH"

args="--with-alsa-prefix=$dir/../alsa-bin --with-alsa-inc-prefix=$dir/../alsa-bin/include"
args="$args --with-systemdsystemunitdir=$dir/../alsa-bin/lib/systemd/system"
args="$args --with-udev-rules-dir=$dir/../alsa-bin/lib/udev/rules.d"
args="$args --disable-nls --disable-alsamixer --disable-xmlto"
./configure --host=aarch64-linux-gnu --prefix=$dir/../alsa-bin $args || exit 1

make clean
make
make install
