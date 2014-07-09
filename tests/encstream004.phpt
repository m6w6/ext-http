--TEST--
encoding stream chunked flush
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$dech = new http\Encoding\Stream\Dechunk(http\Encoding\Stream::FLUSH_FULL);
$file = file(__FILE__);
$data = "";
foreach ($file as $i => $line) {
	$dech = clone $dech;
	if ($i % 2) {
		$data .= $dech->update(sprintf("%lx\r\n%s\r\n", strlen($line), $line));
	} else {
		$data .= $dech->update(sprintf("%lx\r\n", strlen($line)));
		$data .= $dech->flush();
		$data .= $dech->update($line);
		$data .= $dech->flush();
		$data .= $dech->update("\r\n");
	}
	$dech->flush();
	$dech->done() and printf("uh-oh done() reported true!\n");
}
$data .= $dech->update("0\r\n");
var_dump($dech->done());
$data .= $dech->finish();
var_dump(implode("", $file) === $data);
?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
