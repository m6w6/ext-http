#!/usr/bin/env php
<?php echo "# generated file; do not edit!\n"; ?>

name: curl-matrix
on:
  workflow_dispatch:
  push:

jobs:
<?php

if ($argc > 1) {
  $curlver = array_map(fn($v) => strtr($v, ".", "_"), array_unique(array_slice($argv, 1)));
} else {
  $curlver = array_unique(
    iterator_to_array(
      (function() {
        $split = function($sep, $subject, $def = [""]) {
          return array_filter(array_map("trim", explode($sep, $subject))) + $def;
        };
        foreach (file(__DIR__."/curlver.dist") as $line) {
          $rec = $split(":", $split("#", $line)[0]);
          if (!empty($rec[1])) foreach ($split(" ", $rec[1], []) as $dist_ver) {
            yield strtr($dist_ver, ".", "_");
          }
        }
      })()
    )
  );
}

rsort($curlver, SORT_NATURAL);

$gen = include __DIR__ . "/ci/gen-matrix.php";
$job = $gen->github([
"curl" => [
    "PHP" => "8.2",
    "CURL" => $curlver,
    "enable_debug" => "yes",
    "enable_iconv" => "yes",
    "with_http_libcurl_dir" => "/opt",
]]);
foreach ($job as $id => $env) {
    printf("  curl-%s:\n", $env["CURL"]);
    printf("    name: curl-%s\n", $env["CURL"]);
    printf("    continue-on-error: true\n");
    printf("    env:\n");
    foreach ($env as $key => $val) {
        printf("      %s: \"%s\"\n", $key, $val);
    }
?>
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v2
        with:
          submodules: true
          path: http
      - uses: actions/checkout@v2
        with:
          repository: curl/curl
          path: curl
<?php if ($env["CURL"] !== "master") : ?>
          ref: curl-<?=$env["CURL"]?> #
<?php endif; ?>
      - name: Install
        run: |
          echo 'deb-src http://azure.archive.ubuntu.com/ubuntu jammy main' | sudo tee -a /etc/apt/sources.list && \
          echo 'deb-src http://azure.archive.ubuntu.com/ubuntu jammy-updates main' | sudo tee -a /etc/apt/sources.list && \
          sudo apt-get update -y &&  \
          sudo apt-get build-dep -y libcurl4-openssl-dev && \
          sudo apt-get install -y \
            php-cli \
            php-pear \
            libidn11-dev \
            libidn2-0-dev \
            libicu-dev \
            libevent-dev \
            libbrotli-dev \
            re2c
      - name: Curl
        run: |
          sudo chmod +x /usr/share/libtool/build-aux/ltmain.sh
          sudo ln -s /usr/share/libtool/build-aux/ltmain.sh /usr/bin/libtool
          cd curl
          ./buildconf
          ./configure --prefix=/opt --disable-dependency-tracking --with-ssl --with-openssl --without-libssh2 --disable-ldap
          make -j2
          make install
      - name: Prepare
        run: |
          cd http
          make -f scripts/ci/Makefile php || make -f scripts/ci/Makefile clean php
          make -f scripts/ci/Makefile pecl PECL=m6w6/ext-raphf.git:raphf:master
      - name: Build
        run: |
          cd http
          make -f scripts/ci/Makefile ext PECL=http
      - name: Test
        run: |
          cd http
          make -f scripts/ci/Makefile test

<?php
}
