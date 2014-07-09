--TEST--
encoding stream zlib with explicit flush
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
	$data .= $infl->flush();
	if (strlen($temp = $defl->update($line))) {
		$data .= $infl->update($temp);
		$data .= $infl->flush();
	}
	if (strlen($temp = $defl->flush())) {
		$data .= $infl->update($temp);
		$data .= $infl->flush();
	}
	$defl->done() or printf("uh-oh stream's not done yet!\n");
}
if (strlen($temp = $defl->finish())) {
	$data .= $infl->update($temp);
}
var_dump($defl->done());
$data .= $infl->finish();
var_dump($infl->done());
var_dump(implode("", $file) === $data);

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
DONE
