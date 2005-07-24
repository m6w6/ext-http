--TEST--
INI entries
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
var_dump(ini_set('http.cache_log', 'cache.log'));
var_dump(ini_get('http.cache_log'));
var_dump(ini_set('http.allowed_methods', 'POST, HEAD, GET'));
var_dump(ini_get('http.allowed_methods'));
echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
string(9) "cache.log"
bool(true)
string(15) "POST, HEAD, GET"
Done

