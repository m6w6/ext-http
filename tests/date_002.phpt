--TEST--
http_date() without timestamp
--SKIPIF--
<?php 
include 'skip.inc';
?>
--FILE--
<?php
$d1 = http_date();
$d2 = http_date();
var_dump($d1 === $d2);
echo strtotime($d1), "\n$d1\n$d2\n";
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

bool(true)
%d
%s, %d %s %d %d:%d:%d GMT
%s, %d %s %d %d:%d:%d GMT
