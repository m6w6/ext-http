--TEST--
http_absolute_uri() with proto
--SKIPIF--
<?php 
extension_loaded('http') or die('ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('cannot run tests with CLI');
?>
--FILE--
<?php
echo http_absolute_uri('sec', 'https'), "\n";
echo http_absolute_uri('/pub', 'ftp'), "\n";
echo http_absolute_uri('/', null), "\n";
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

https://localhost/sec
ftp://localhost/pub
http://localhost/
