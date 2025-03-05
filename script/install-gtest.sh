#!/bin/sh

if [ ! -f script/install-gtest.sh ]; then
	echo "Please run from project root!"
	exit 1
fi

function md {
	if [ ! -d "$1" ]; then
		mkdir "$1"
	fi
}

ARCHIVE="https://github.com/google/googletest/archive/refs/heads/main.tar.gz"
VENDOR="vendor"
SRC="$VENDOR/src"
GTEST="$SRC/googletest"

md "$VENDOR"
md "$SRC"
md "$GTEST"

pushd "$GTEST"
curl -L "$ARCHIVE" | tar xz --strip-components=1
popd

cmake -DBUILD_GMOCK=OFF "-DCMAKE_INSTALL_PREFIX=$VENDOR" -S "$GTEST" -B "$GTEST/build"
cmake --build "$GTEST/build"
cmake --install "$GTEST/build" --prefix "$VENDOR"
