--TEST--
encoding stream brotli without flush
--SKIPIF--
<?php
include "skipif.inc";
class_exists("http\\Encoding\\Stream\\Enbrotli") or die("SKIP need brotli support");
?>
--FILE--
<?php
echo "Test\n";

$defl = new http\Encoding\Stream\Enbrotli;
$infl = new http\Encoding\Stream\Debrotli;
$file = file(__FILE__);
$data = "";
foreach ($file as $line) {
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
