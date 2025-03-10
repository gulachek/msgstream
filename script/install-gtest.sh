#!/bin/bash

set -x
set -e

if [ ! -f script/install-gtest.sh ]; then
	echo "Please run from project root!"
	exit 1
fi

. script/util.sh

download \
	-u "https://github.com/google/googletest/archive/refs/heads/main.tar.gz" \
	-c "2be01a3a16c58a72edd48cd11f0457da8cc9bcfbface2425011a444928ba9723" \
	-o "$VENDORSRC/gtest-download.tgz"
	
GTEST="$VENDORSRC/googletest"

md "$GTEST"

# TODO - seems like I'm recreating package managers.
# I like being explicit about all dependencies being pulled
# in for licensing concerns, etc. However, this seems like
# potentially a solved problem that I'm rescripting. Albeit,
# this isn't that much work to maintain right now.

untar -f "$VENDORSRC/gtest-download.tgz" -d "$GTEST" 

cmake -DBUILD_GMOCK=OFF "-DCMAKE_INSTALL_PREFIX=$VENDOR" -S "$GTEST" -B "$GTEST/build"
cmake --build "$GTEST/build"
cmake --install "$GTEST/build"
