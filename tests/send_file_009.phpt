--TEST--
http_send_file() NUM-NUM range
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--ENV--
HTTP_RANGE=bytes=5-9
--FILE--
<?php
http_send_file('data.txt');
?>
--EXPECTF--
Status: 206
X-Powered-By: PHP/%s
Accept-Ranges: bytes
Content-Range: bytes 5-9/1010
Content-Length: 5
Content-type: %s

56789
