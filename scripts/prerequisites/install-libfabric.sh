#!/bin/bash
set -eu
export TMPDIR=/var/tmp
WORKPATH=`mktemp -d`
INSTALL_PREFIX="/usr/local"
if [[ $# -gt 0 ]]; then
    INSTALL_PREFIX=$1
fi

echo "Using INSTALL_PREFIX=${INSTALL_PREFIX}"

cd ${WORKPATH}
git clone https://github.com/ofiwg/libfabric.git
cd libfabric
#git checkout fcf0f2ec3c7109e06e09d3650564df8d2dfa12b6
#git checkout tags/v1.7.0
git checkout tags/v1.12.1
libtoolize
./autogen.sh
./configure --prefix=${INSTALL_PREFIX} --disable-memhooks-monitor
make -j `lscpu | grep "^CPU(" | awk '{print $2}'`
make install
