--TEST--
http_send_data() multiple ranges
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5);
?>
--ENV--
HTTP_RANGE=bytes=0-0,-1
--FILE--
<?php
http_send_content_type('text/plain');
http_send_data("01");
?>
--EXPECTF--
Status: 206
X-Powered-By: PHP/%s
Accept-Ranges: bytes
Content-Type: multipart/byteranges; boundary=%d.%d


--%d.%d
Content-Type: text/plain
Content-Range: bytes 0-0/2

0
--%d.%d
Content-Type: text/plain
Content-Range: bytes 1-1/2

1
--%d.%d--
