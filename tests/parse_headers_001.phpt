--TEST--
http_parse_headers()
--SKIPIF--
<?php 
include 'skip.inc';
?>
--FILE--
<?php
print_r(http_parse_headers(
"Host: localhost\r\n".
"Nospace:here\r\n".
"Muchspace:  there   \r\n".
"Empty:\r\n".
"Empty2: \r\n".
": invalid\r\n".
" : bogus\r\n".
"Folded: one\r\n".
"\ttwo\r\n".
"  three\r\n".
"stop\r\n"
));
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

Array
(
    [Host] => localhost
    [Nospace] => here
    [Muchspace] => there
    [Empty] => 
    [Empty2] => 
    [Folded] => one
	two
  three
)