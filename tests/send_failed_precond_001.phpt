--TEST--
http_send() failed precondition
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkver(5.1);
?>
--ENV--
HTTP_RANGE=bytes=0-1
HTTP_IF_UNMODIFIED_SINCE=Thu, 01 Jan 1970 00:16:40 GMT
--FILE--
<?php
http_cache_last_modified();
http_send_file(__FILE__);
?>
--EXPECTF--
Status: 412
X-Powered-By: %s
Cache-Control: private, must-revalidate, max-age=0
Last-Modified: %s
Accept-Ranges: bytes
Content-type: text/html
