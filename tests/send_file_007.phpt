--TEST--
http_send_file() NIL-NIL range
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
?>
--ENV--
HTTP_RANGE=bytes=0-0
--FILE--
<?php
http_send_file('data.txt');
?>
--EXPECTF--
Status: 206
X-Powered-By: PHP/%s
Accept-Ranges: bytes
Content-Range: bytes 0-0/1010
Content-Length: 1
Content-type: %s

0
