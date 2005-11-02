--TEST--
ob sha1 etag
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5.1);
skipif(!http_support(HTTP_SUPPORT_MHASHETAGS), 'need mhash support');
?>
--FILE--
<?php
ini_set('http.etag_mode', HTTP_ETAG_SHA1);
http_cache_etag();
print("abc\n");
?>
--EXPECTF--
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
ETag: "03cfd743661f07975fa2f1220c5194cbaff48451"
Content-type: %s

abc
