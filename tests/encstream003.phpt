--TEST--
encoding stream chunked error
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$s = "3\nis \nbetter than\n1\n";
var_dump(http\Encoding\Stream\Dechunk::decode($s));
$s = "3\r\nis \r\nbetter than\r\n1\r\n";
var_dump(http\Encoding\Stream\Dechunk::decode($s));
$s = "3\nis \nreally better than\n1\n";
var_dump(http\Encoding\Stream\Dechunk::decode($s));
?>
DONE
--EXPECTF--
Test

Warning: http\Encoding\Stream\Dechunk::decode(): Expected LF at pos 8 of 20 but got 0x74 in %s on line %d

Warning: http\Encoding\Stream\Dechunk::decode(): Truncated message: chunk size 190 exceeds remaining data size 11 at pos 9 of 20 in %s on line %d
string(14) "is ter than
1
"

Warning: http\Encoding\Stream\Dechunk::decode(): Expected CRLF at pos 10 of 24 but got 0x74 0x74 in %s on line %d

Warning: http\Encoding\Stream\Dechunk::decode(): Truncated message: chunk size 190 exceeds remaining data size 12 at pos 12 of 24 in %s on line %d
string(15) "is er than
1
"

Warning: http\Encoding\Stream\Dechunk::decode(): Expected chunk size at pos 6 of 27 but got trash in %s on line %d
bool(false)
DONE
