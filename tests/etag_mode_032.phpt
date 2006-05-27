--TEST--
sha1 etag
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--FILE--
<?php
ini_set('http.etag.mode', 'SHA1');
http_cache_etag();
http_send_data("abc\n");
?>
--EXPECTF--
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
Accept-Ranges: bytes
ETag: "03cfd743661f07975fa2f1220c5194cbaff48451"
Content-Length: 4
Content-type: %s

abc
