--TEST--
mhash etag
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
?>
--FILE--
<?php
ini_set('http.etag_mode', HTTP_ETAG_MHASH_WHIRLPOOL);
http_cache_etag();
http_send_data("abc\n");
?>
--EXPECTF--
Content-type: %s
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
Accept-Ranges: bytes
ETag: "53efa9e423f86dabd449b3e23dd0350def661b9e7055b23ceb2230c8b61bc0766514957ea9d349a88ef794715a1a17a409b549edfd6f43d696e63407fff3541c"
Content-Length: 4

abc
