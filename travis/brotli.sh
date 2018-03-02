#!/bin/bash

BROTLI_DIR="$with_http_libbrotli_dir"
BROTLI_SRC="$BROTLI_DIR.git"

if test -n "$BROTLI_DIR" && test "$BROTLI_DIR" != "no"; then
	if test -d "$BROTLI_SRC"; then
		cd "$BROTLI_SRC"
		git pull
	else
		git clone https://github.com/google/brotli.git "$BROTLI_SRC"
		cd "$BROTLI_SRC"
	fi
	git checkout $1
	./bootstrap
	./configure -C --prefix="$BROTLI_DIR"
	make -j ${JOBS:2}
	make install INSTALL=install
fi
