--TEST--
http_date() with timestamp
--SKIPIF--
<?php 
include 'skip.inc';
?>
--FILE--
<?php
echo http_date(1), "\n";
echo http_date(1234567890), "\n";
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

Thu, 01 Jan 1970 00:00:01 GMT
Fri, 13 Feb 2009 23:31:30 GMT
