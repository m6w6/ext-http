--TEST--
Bug #71719 (Buffer overflow in HTTP url parsing functions)
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";
try {
	echo new http\Message(file_get_contents(__DIR__."/data/bug71719.bin"), false);
} catch (Exception $e) {
	echo $e;
}
?>

===DONE===
--EXPECTF--
Test
%r(exception ')?%rhttp\Exception\BadMessageException%r(' with message '|: )%rhttp\Message::__construct(): Could not parse HTTP protocol version 'HTTP/%s.0'%r'?%r in %sbug71719.php:5
Stack trace:
#0 %sbug71719.php(5): http\Message->__construct('%r(\?\?|\\x80\\xAC)%rTd 5 HTTP/1.1...', false)
#1 {main}
===DONE===
