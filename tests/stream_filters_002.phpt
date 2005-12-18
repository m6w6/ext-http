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
stream_filter_append($f, 'http.gzencode', STREAM_FILTER_WRITE);
fwrite($f, $d);
fflush($f);
fclose($f);
$gzencoded = file_get_contents($n);

$f = fopen($n, 'wb');
stream_filter_append($f, 'http.deflate', STREAM_FILTER_WRITE);
fwrite($f, $d);
fflush($f);
fclose($f);
$deflated = file_get_contents($n);

var_dump($d == http_gzdecode($gzencoded));
var_dump($d == http_inflate($deflated));

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
bool(true)
Done
