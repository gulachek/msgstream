#!/bin/sh

function md {
	if [ ! -d "$1" ]; then
		mkdir "$1"
	fi
}

VENDOR="$PWD/vendor"
VENDORSRC="$VENDOR/src"

md "$VENDOR"
md "$VENDORSRC"
