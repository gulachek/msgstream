#!/bin/sh

set -e
set -x

# Validates and sets VERSION
. script/parse-validate-version.sh

PKG="${1:?Usage: $0 <msgstream-x.x.x.tgz>}"

if [ "$(basename $PKG)" != "msgstream-$VERSION.tgz" ]; then
	echo "Unexpected tarball '$PKG' given to $0"
	exit 1
fi

gh release upload "v$VERSION" "$PKG#Source Package"
