--TEST--
http_date() without timestamp
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
$t = time();
$d1 = http_date($t);
$d2 = http_date($t);
var_dump($d1 === $d2);
echo strtotime($d1), "\n$d1\n$d2\n";
?>
--EXPECTF--
%sTEST
bool(true)
%d
%s, %d %s %d %d:%d:%d GMT
%s, %d %s %d %d:%d:%d GMT

