--TEST--
http_date() without timestamp
--SKIPIF--
<?php
include 'skip.inc';
?>
--INI--
date.timezone=GMT
--FILE--
<?php
echo "-TEST\n";
$d = http_date();
$t = strtotime($d);
var_dump($t > 1);
echo "$t\n$d\nDone\n";
?>
--EXPECTF--
%sTEST
bool(true)
%d
%s, %d %s %d %d:%d:%d GMT
Done
