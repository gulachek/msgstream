#!/bin/sh

set -x

if [ ! -f script/install-gtest.sh ]; then
	echo "Please run from project root!"
	exit 1
fi

. script/util.sh

ARCHIVE="https://github.com/google/googletest/archive/refs/heads/main.tar.gz"
GTEST="$VENDORSRC/googletest"

md "$GTEST"

untar -d "$GTEST" -u "$ARCHIVE"

cmake -DBUILD_GMOCK=OFF "-DCMAKE_INSTALL_PREFIX=$VENDOR" -S "$GTEST" -B "$GTEST/build"
cmake --build "$GTEST/build"
cmake --install "$GTEST/build" --prefix "$VENDOR"
