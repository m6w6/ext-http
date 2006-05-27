--TEST--
ob crc32 etag (may fail because PHPs crc32 is actually crc32b)
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmax(5.0);
?>
--FILE--
<?php
ini_set('http.etag.mode', 'crc32');
http_cache_etag();
print("abc\n");
?>
--EXPECTF--
Content-type: %s
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
ETag: "4e818847"

abc
