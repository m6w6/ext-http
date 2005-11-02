--TEST--
http_send_data() last modified caching
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--FILE--
<?php
http_cache_last_modified(-5);
http_send_data("abc\n");
?>
--EXPECTF--
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
Last-Modified: %s, %d %s %d %d:%d:%d GMT
Accept-Ranges: bytes
Content-Length: 4
Content-type: %s

abc
