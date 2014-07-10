--TEST--
message body to callback
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = new http\Message\Body(fopen(__FILE__,"r"));
$s = "";
$file->toCallback(
	function($body, $string) use (&$s) { $s.=$string; }
);
var_dump($s === (string) $file);

?>
DONE
--EXPECT--
Test
bool(true)
DONE
