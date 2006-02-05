--TEST--
http_build_str
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";

parse_str($s = "a=b", $q);
var_dump($s === http_build_str($q, null, "&"));

parse_str($s = "a=b&c[0]=1", $q);
var_dump($s === http_build_str($q, null, "&"));

parse_str($s = "a=b&c[0]=1&d[e]=f", $q);
var_dump($s === http_build_str($q, null, "&"));

var_dump("foo[0]=1&foo[1]=2&foo[2][0]=3" === http_build_str(array(1,2,array(3)), "foo", "&"));

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
bool(true)
bool(true)
bool(true)
Done
