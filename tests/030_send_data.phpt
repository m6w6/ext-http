--TEST--
http_send_data()
--SKIPIF--
<?php 
extension_loaded('http') or die('ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('cannot run tests with CLI');
?>
--FILE--
<?php
http_content_type('text/plain');
http_send_data(str_repeat('123abc', 1));
?>
--EXPECTREGEX--
.+\s+.+\s+(abc|[123abc]{,100000})