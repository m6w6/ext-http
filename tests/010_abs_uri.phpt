--TEST--
http_absolute_uri() with relative paths
--SKIPIF--
<?php 
extension_loaded('http') or die('ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('cannot run tests with CLI');
?>
--FILE--
<?php
echo http_absolute_uri('page'), "\n";
echo http_absolute_uri('with/some/path/'), "\n";
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

http://localhost/page
http://localhost/with/some/path/
