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
$cur = "8.2";
$job = $gen->github([
"next" => [
    "PHP" => ["master"],
    "enable_debug" => "yes",
    "enable_zts" => "yes",
    "enable_iconv" => "yes",
    "TEST_PHP_ARGS" => "-d error_reporting=24575" // ignore E_DEPRECATED
],
"old" => [
    "PHP" => ["8.1", "8.0"],
    "enable_debug" => "yes",
    "enable_zts" => "yes",
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
    "enable_debug",
    "enable_zts",
    "enable_iconv" => "yes",
],
"cur-cov" => [
// once everything enabled for current, with coverage
    "CFLAGS" => "-O0 -g --coverage",
    "CXXFLAGS" => "-O0 -g --coverage",
    "PHP" => $cur,
    "enable_iconv" => "yes",
    [
        // mutually exclusive
        "with_http_libicu_dir",
        "with_http_libidn_dir",
        "with_http_libidn2_dir",
    ],
]]);
foreach ($job as $id => $env) {
    printf("  %s:\n", $id);
    printf("    name: \"%s (%s)\"\n", $id, $env["PHP"]);
    if ($env["PHP"] == "master") {
        printf("    continue-on-error: true\n");
    }
    printf("    env:\n");
    foreach ($env as $key => $val) {
        printf("      %s: \"%s\"\n", $key, $val);
    }
?>
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: true
      - name: Info
        run: |
          locale -a && locale
      - name: Install
        run: |
          sudo apt-get install -y \
            php-cli \
            php-pear \
            libcurl4-openssl-dev \
            libidn-dev \
            libidn2-0-dev \
            libicu-dev \
            libevent-dev \
            libbrotli-dev \
            re2c
      - name: Prepare
        run: |
          make -f scripts/ci/Makefile php || make -f scripts/ci/Makefile clean php
          make -f scripts/ci/Makefile pecl PECL=m6w6/ext-raphf.git:raphf:master
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
