#!/bin/bash

if test -n "$with_http_libbrotli_dir" && test "$with_http_libbrotli_dir" != "no"; then
	git clone https://github.com/google/brotli.git /home/travis/brotli.git
	cd /home/travis/brotli.git
	git checkout v1.0.2
	./bootstrap
	./configure --prefix=/home/travis/brotli
	make -j2
	make install
fi
