--TEST--
http_send() If-Range
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
?>
--FILE--
<?php
$_SERVER['HTTP_RANGE'] = 'bytes=0-1';
$_SERVER['HTTP_IF_RANGE'] = '"abcd"';
http_cache_etag('abc');
http_send_file(__FILE__);
?>
--EXPECTF--
X-Powered-By: %s
Cache-Control: private, must-revalidate, max-age=0
ETag: "abc"
Accept-Ranges: bytes
Content-Length: %d
Content-type: text/html

%s
