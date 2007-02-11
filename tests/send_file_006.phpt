--TEST--
http_send_file() first/last byte range
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
?>
--ENV--
HTTP_RANGE=bytes=0-0,-1
--FILE--
<?php
http_send_content_type("text/plain");
http_send_file('data.txt');
?>
--EXPECTF--
Status: 206
X-Powered-By: PHP/%s
Accept-Ranges: bytes
Content-Type: multipart/byteranges; boundary=%d.%d


--%d.%d
Content-Type: text/plain
Content-Range: bytes 0-0/1010

0
--%d.%d
Content-Type: text/plain
Content-Range: bytes 1009-1009/1010



--%d.%d--
