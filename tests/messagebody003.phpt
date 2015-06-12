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
http\Exception\RuntimeException: http\Message\Body::append(): Failed to append 4 bytes to body; wrote 0 in %s:%d
Stack trace:
#0 %s(%d): http\Message\Body->append('nope')
#1 {main}
DONE
