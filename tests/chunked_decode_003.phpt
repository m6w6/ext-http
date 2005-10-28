--TEST--
http_chunked_decode() truncated message
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
"04\r\n".
"ra\nc\r\n".
"06\r\n".
"adabra\r\n".
"ff\r\n".
"\nall we got\n";
var_dump(http_chunked_decode($data));
?>
--EXPECTF--
%sTEST
%sWarning%s:%shttp_chunked_decode()%sTruncated message: chunk size %d exceeds remaining data size %d at pos %d of %d%s
string(24) "abra
cadabra
all we got
"
