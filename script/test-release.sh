#!/bin/sh

set -e
set -x

if [ ! -f script/test-release.sh ]; then
	echo "Please run from project root!"
	exit 1
fi

. script/util.sh

node make.mjs dist-msgstream

VERSION="$(jq -r .version package.json)"
SRC="$(pwd)"
BUILD="$SRC/build"

MSGSTREAM="$VENDORSRC/msgstream"
md "$MSGSTREAM"

pushd "$MSGSTREAM"
tar xfz "$BUILD/msgstream-$VERSION.tgz" --strip-components=1
popd

cmake -S "$MSGSTREAM" -B "$MSGSTREAM/build"
cmake --build "$MSGSTREAM/build"
cmake --install "$MSGSTREAM/build" --prefix "$VENDOR"

TEST="$SRC/test/release"

echo "Testing pkgconfig"
rm -rf "$TEST/build"
node "$TEST/make.mjs" --srcdir "$TEST" --outdir "$TEST/build" test

echo "Testing CMake"
rm -rf "$TEST/build"
cmake -DCMAKE_PREFIX_PATH="$VENDOR" -S "$TEST" -B "$TEST/build"
cmake --build "$TEST/build"
"$TEST/build/header_size"
