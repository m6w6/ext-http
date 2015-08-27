--TEST--
invalid HTTP info
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

function trap($cb) {
	try {
		$cb();
	} catch (Exception $e) { 
		echo $e,"\n"; 
	}
}

trap(function() {
	echo new http\Message("HTTP/1.1 99 Apples in my Basket");
});

trap(function() {
	echo new http\Message("CONNECT HTTP/1.1");
});

echo new http\Message("HTTP/1.1");
echo new http\Message("CONNECT www.example.org:80 HTTP/1.1");

?>
===DONE===
--EXPECTF--
Test
http\Exception\BadMessageException: http\Message::__construct(): Failed to parse headers: unexpected character '\057' at pos 4 of 'HTTP/1.1 99 Apples in my Basket' in %sinfo002.php:%d
Stack trace:
#0 %sinfo002.php(%d): http\Message->__construct('HTTP/1.1 99 App...')
#1 %sinfo002.php(%d): {closure}()
#2 %sinfo002.php(%d): trap(Object(Closure))
#3 {main}
http\Exception\BadMessageException: http\Message::__construct(): Failed to parse headers: unexpected character '\040' at pos 7 of 'CONNECT HTTP/1.1' in %sinfo002.php:%d
Stack trace:
#0 %sinfo002.php(%d): http\Message->__construct('CONNECT HTTP/1....')
#1 %sinfo002.php(%d): {closure}()
#2 %sinfo002.php(%d): trap(Object(Closure))
#3 {main}
HTTP/1.1 200
CONNECT www.example.org:80 HTTP/1.1
===DONE===
