--TEST--
md5 etag
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--FILE--
<?php
ini_set('http.etag.mode', 'MD5');
http_cache_etag();
http_send_data("abc\n");
?>
--EXPECTF--
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
Accept-Ranges: bytes
ETag: "0bee89b07a248e27c83fc3d5951213c1"
Content-Length: 4
Content-type: %s

abc
