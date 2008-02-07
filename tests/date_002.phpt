--TEST--
http_date() without timestamp
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
ini_set('date.timezone', 'GMT');
$d = http_date();
$t = strtotime($d);
var_dump($t > 1);
echo "$t\n$d\nDone\n";
?>
--EXPECTF--
%aTEST
bool(true)
%d
%a, %d %a %d %d:%d:%d GMT
Done
