--TEST--
message body append error
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = new http\Message\Body(fopen(__FILE__, "r"));
try {
	$file->append("nope");
} catch (Exception $e) {
	echo $e, "\n";
}

?>
DONE
--EXPECTF--
Test
exception 'http\Exception\RuntimeException' with message 'http\Message\Body::append(): Failed to append 4 bytes to body; wrote 0' in /home/mike/src/ext-http.git/tests/messagebody003.php:6
Stack trace:
#0 %s(%d): http\Message\Body->append('nope')
#1 {main}
DONE
