--TEST--
http_send() If-Range
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmax(5);
?>
--FILE--
<?php
$_SERVER['HTTP_RANGE'] = 'bytes=0-1';
$_SERVER['HTTP_IF_RANGE'] = '"abc"';
http_cache_etag('abc');
http_send_file(__FILE__);
?>
--EXPECTF--
Status: 206
X-Powered-By: %s
Cache-Control: private, must-revalidate, max-age=0
ETag: "abc"
Accept-Ranges: bytes
Content-Range: bytes 0-1/%d
Content-Length: 2

<?
