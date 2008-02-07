--TEST--
gzip stream filters
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib support");
?>
--FILE--
<?php

echo "-TEST\n";

$d = file_get_contents(__FILE__);
$n = tempnam(dirname(__FILE__), 'hsf');

$f = fopen($n, 'wb');
stream_filter_append($f, 'http.deflate', STREAM_FILTER_WRITE, HTTP_DEFLATE_TYPE_GZIP);
fwrite($f, $d);
fflush($f);
fwrite($f, $d);
fclose($f);
var_dump($d.$d == http_inflate(file_get_contents($n)));

$f = fopen($n, 'wb');
stream_filter_append($f, 'http.deflate', STREAM_FILTER_WRITE);
fwrite($f, $d);
fflush($f);
fwrite($f, $d);
fclose($f);
var_dump($d.$d == http_inflate(file_get_contents($n)));

$f = fopen($n, 'wb');
stream_filter_append($f, 'http.deflate', STREAM_FILTER_WRITE, HTTP_DEFLATE_TYPE_RAW);
fwrite($f, $d);
fflush($f);
fwrite($f, $d);
fclose($f);
var_dump($d.$d == http_inflate(file_get_contents($n)));

unlink($n);

echo "Done\n";
?>
--EXPECTF--
%aTEST
bool(true)
bool(true)
bool(true)
Done
