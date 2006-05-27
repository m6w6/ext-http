--TEST--
crc32 etag (may fail because PHPs crc32 is actually crc32b)
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--FILE--
<?php
ini_set('http.etag.mode', extension_loaded('hash')?'crc32b':'crc32');
http_cache_etag();
http_send_data("abc\n");
?>
--EXPECTF--
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
Accept-Ranges: bytes
ETag: "4e818847"
Content-Length: 4
Content-type: %s

abc
