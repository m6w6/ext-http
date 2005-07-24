--TEST--
http_chunked_decode()
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
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
%sTEST
string(9) "aba
cabcd"

