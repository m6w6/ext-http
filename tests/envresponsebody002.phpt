--TEST--
env response body error
--SKIPIF--
<?php
include "skipif.inc";
?>
--INI--
output_buffering=1
--FILE--
<?php
echo "Test\n";
ob_flush();
try {
	$r = new http\Env\Response;
	var_dump((string) $r->getBody());
} catch (http\Exception $e) {
	echo $e->getMessage(),"\n";
}
?>
Done
--EXPECTF--
Test
Could not fetch response body, output has already been sent at %senvresponsebody002.php:3
Done
