--TEST--
http_send_file() oversized range
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--ENV--
HTTP_RANGE=bytes=-1111
--FILE--
<?php
http_send_file('data.txt');
?>
--EXPECTF--
Status: 416
X-Powered-By: PHP/%s
Content-type: %s
