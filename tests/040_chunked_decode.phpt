--TEST--
http_chunked_decode()
--SKIPIF--
<?php 
extension_loaded('http') or die('ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('cannot run tests with CLI');
?>
--FILE--
<?php
$data = 
"02\r\n".
"ab\r\n".
"03\r\n".
"a\nc\r\n".
"04\r\n".
"abcd\r\n".
"0\r\n".
"abracadabra\n";
var_dump(http_chunked_decode($data));
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

string(9) "aba
cabcd"
