--TEST--
crash with querystring and exception from error handler
--SKIPIF--
<?php
include "skipif.inc";
if (version_compare(PHP_VERSION, "7", ">=")) {
	die("skip PHP>=7\n");
}
?>
--GET--
q[]=1&r[]=2
--FILE--
<?php 
echo "Test\n";

set_error_handler(function($c,$e) { throw new Exception($e); });

try {
	$q = http\QueryString::getGlobalInstance();
	var_dump($q->get("q","s"));
} catch (\Exception $e) {
	echo $e->getMessage(),"\n";
}
try {
	$r = new http\Env\Request;
	var_dump($r->getQuery("r", "s"));
} catch (\Exception $e) {
	echo $e->getMessage(),"\n";
}

?>
===DONE===
--EXPECT--
Test
Array to string conversion
Array to string conversion
===DONE===
