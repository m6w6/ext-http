--TEST--
http_split_response()
--SKIPIF--
<?php 
extension_loaded('http') or die('ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('cannot run tests with CLI');
?>
--FILE--
<?php
var_export(http_split_response("HTTP/1.1 200 Ok\r\nContent-Type: text/plain\r\nContent-Language: de-AT\r\nDate: Sat, 22 Jan 2005 18:10:02 GMT\r\n\r\nHallo Du!)"));
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

array (
  0 => 
  array (
    'Status' => '200 Ok',
    'Content-Type' => 'text/plain',
    'Content-Language' => 'de-AT',
    'Date' => 'Sat, 22 Jan 2005 18:10:02 GMT',
  ),
  1 => 'Hallo Du!',
)