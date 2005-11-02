--TEST--
http_send_data() HTTP_SENDBUF_SIZE long string
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--FILE--
<?php
http_throttle(0.01, 1);
http_send_data('00000000000000000000');
?>
--EXPECTF--
X-Powered-By: PHP/%s
Accept-Ranges: bytes
Content-Length: 20
Content-type: %s

00000000000000000000
