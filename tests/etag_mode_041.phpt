--TEST--
ob crc32 etag (may fail because PHPs crc32 is actually crc32b)
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin("5.2.5");
?>
--FILE--
<?php
ini_set('http.etag.mode', extension_loaded('hash')?'crc32b':'crc32');
http_cache_etag();
print("abc\n");
?>
--EXPECTF--
X-Powered-By: PHP/%a
Cache-Control: private, must-revalidate, max-age=0
ETag: "4e818847"
Content-type: %a

abc
