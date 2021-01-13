#!/usr/bin/env php
<?php echo "# generated file; do not edit!\n"; ?>

name: ci
on:
  workflow_dispatch:
  push:
  pull_request:

jobs:
<?php

$gen = include __DIR__ . "/ci/gen-matrix.php";
$cur = "7.4";
$job = $gen->github([
"old-matrix" => [
// most useful for all additional versions except current
	"PHP" => ["7.0", "7.1", "7.2", "7.3"],
	"enable_debug" => "yes",
	"enable_maintainer_zts" => "yes",
	"enable_json" => "yes",
	"enable_hash" => "yes",
	"enable_iconv" => "yes",
], 
"cur-none" => [
// everything disabled for current
	"PHP" => $cur,
	"with_http_libicu_dir" => "no",
	"with_http_libidn_dir" => "no",
	"with_http_libidn2_dir" => "no",
	"with_http_libcurl_dir" => "no",
	"with_http_libevent_dir" => "no",
	"with_http_libbrotli_dir" => "no",
], 
"cur-dbg-zts" => [
// everything enabled for current, switching debug/zts
	"PHP" => $cur,
	"PECLs" => "event",			// for tests/client029.phpt
	"enable_sockets" => "yes",	// needed by pecl/event
	"enable_debug",
	"enable_maintainer_zts",
	"enable_json" => "yes",
	"enable_hash" => "yes",
	"enable_iconv" => "yes",
], 
"cur-cov" => [
// once everything enabled for current, with coverage
	"CFLAGS" => "-O0 -g --coverage",
	"CXXFLAGS" => "-O0 -g --coverage",
	"PHP" => $cur,
	"PECLs" => "event",			// for tests/client029.phpt
	"enable_sockets" => "yes",	// needed by pecl/event
	"enable_json" => "yes",
	"enable_hash" => "yes",
	"enable_iconv" => "yes",
	[
		"with_http_libicu_dir",
		"with_http_libidn_dir",
		"with_http_libidn2_dir",
	],
]]);

foreach ($job as $id => $env) {
    printf("  %s:\n", $id);
    printf("    name: %s\n", $id);
    if ($env["PHP"] == "master") {
        printf("    continue-on-error: true\n");
    }
    printf("    env:\n");
    foreach ($env as $key => $val) {
        printf("      %s: \"%s\"\n", $key, $val);
    }
?>
    runs-on: ubuntu-20.04
    steps:
      - uses: actions/checkout@v2
        with:
          submodules: true
      - name: Install
        run: |
          sudo apt-get install -y \
            php-cli \
            php-pear \
            libcurl4-openssl-dev \
            libevent-dev \
            libidn11-dev \
            libidn2-0-dev \
            libicu-dev \
            libevent-dev \
            libbrotli-dev \
            re2c
      - name: Prepare
        run: |
          make -f scripts/ci/Makefile php || make -f scripts/ci/Makefile clean php
          make -f scripts/ci/Makefile pecl PECL=m6w6/ext-raphf.git:raphf:master
          make -f scripts/ci/Makefile pecl PECL=m6w6/ext-propro.git:propro:master
          if test -n "$PECLs"; then
            IFS=$','
            for pecl in $PECLs; do
              make -f scripts/ci/Makefile pecl PECL=$pecl
            done
            unset IFS
          fi

      - name: Build
        run: |
          make -f scripts/ci/Makefile ext PECL=http
      - name: Test
        run: |
          make -f scripts/ci/Makefile test
<?php if (isset($env["CFLAGS"]) && strpos($env["CFLAGS"], "--coverage") != false) : ?>
      - name: Coverage
        if: success()
        run: |
          cd src/.libs
          bash <(curl -s https://codecov.io/bash) -X xcode -X coveragepy
<?php endif; ?>

<?php
}
