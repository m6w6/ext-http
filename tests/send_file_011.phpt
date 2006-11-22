--TEST--
http_send_file() NUM-NIL range
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--ENV--
HTTP_RANGE=bytes=1000- 
--FILE--
<?php
http_send_file('data.txt');
?>
--EXPECTF--
Status: 206
X-Powered-By: PHP/%s
Accept-Ranges: bytes
Content-Range: bytes 1000-1009/1010
Content-Length: 10
Content-type: %s

123456789
