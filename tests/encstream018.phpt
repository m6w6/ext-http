--TEST--
encoding stream brotli with explicit flush
--SKIPIF--
<?php
include "skipif.inc";
class_exists("http\\Encoding\\Stream\\Enbrotli") or die("SKIP need brotli support");
?>
--FILE--
<?php
echo "Test\n";

$enc = new http\Encoding\Stream\Enbrotli;
$dec = new http\Encoding\Stream\Debrotli;
$file = file(__FILE__);
$data = "";
foreach ($file as $line) {
	$data .= $dec->flush();
	if (strlen($temp = $enc->update($line))) {
		$data .= $dec->update($temp);
		$data .= $dec->flush();
	}
	if (strlen($temp = $enc->flush())) {
		$data .= $dec->update($temp);
		$data .= $dec->flush();
	}
}
if (strlen($temp = $enc->finish())) {
	$data .= $dec->update($temp);
}
var_dump($enc->done());
$data .= $dec->finish();
var_dump($dec->done());
var_dump(implode("", $file) === $data);

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
DONE
