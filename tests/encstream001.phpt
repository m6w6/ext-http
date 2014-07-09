--TEST--
encoding stream chunked static
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = file(__FILE__);
$cenc = array_reduce(
	$file,
	function($data, $line) {
		return $data . sprintf("%lx\r\n%s\r\n", strlen($line), $line);
	}
) . "0\r\n";

var_dump(implode("", $file) === http\Encoding\Stream\Dechunk::decode($cenc));

?>
DONE
--EXPECT--
Test
bool(true)
DONE

