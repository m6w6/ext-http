--TEST--
http_redirect() with session
--SKIPIF--
<?php 
extension_loaded('http') or die('ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('cannot run tests with CLI');
?>
--FILE--
<?php
session_start();
http_redirect('redirect', array('a' => 1), true);
?>
--EXPECTF--
Status: 302
Content-type: text/html
X-Powered-By: PHP/%s
Set-Cookie: PHPSESSID=%s; path=/
Expires: %s
Cache-Control: %s
Pragma: %s
Location: http://localhost/redirect?a=1&PHPSESSID=%s
