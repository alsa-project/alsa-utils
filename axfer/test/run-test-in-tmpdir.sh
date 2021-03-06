#!/bin/sh

bin="$1"

test -z ${bin} && exit 90
test ! -x ${bin} && exit 91
test -z ${TMPDIR} && exit 92
test ! -d ${TMPDIR} && exit 93

tmp_dir=$(mktemp -d ${TMPDIR}/container-test.XXXXX)
cur_dir=$(pwd)

echo ${tmp_dir}
cd ${tmp_dir}
${cur_dir}/${bin}
retval=$?
cd ${cur_dir}
rm -rf ${tmp_dir}
exit $retval
