--TEST--
http_send_data() HTTP_SENDBUF_SIZE long string
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
?>
--FILE--
<?php
http_throttle(0.01, 20);
http_send_data('00000000000000000000');
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s
Accept-Ranges: bytes

00000000000000000000