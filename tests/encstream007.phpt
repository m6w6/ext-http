--TEST--
encoding stream zlib without flush
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$defl = new http\Encoding\Stream\Deflate;
$infl = new http\Encoding\Stream\Inflate;
$file = file(__FILE__);
$data = "";
foreach ($file as $line) {
	$infl = clone $infl;
	$defl = clone $defl;
	if (strlen($temp = $defl->update($line))) {
		foreach(str_split($temp) as $byte) {
			$data .= $infl->update($byte);
		}
	}
}
if (strlen($temp = $defl->finish())) {
	$data .= $infl->update($temp);
}
$data .= $infl->finish();
var_dump(implode("", $file) === $data);
?>
DONE
--EXPECT--
Test
bool(true)
DONE
