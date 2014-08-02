--TEST--
encoding stream zlib auto flush
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$defl = new http\Encoding\Stream\Deflate(http\Encoding\Stream::FLUSH_FULL);
$infl = new http\Encoding\Stream\Inflate;

for ($f = fopen(__FILE__, "rb"); !feof($f); $data = fread($f, 0x100)) {
	$infl = clone $infl;
	$defl = clone $defl;
	if (isset($data)) {
		if ($data !== $d=$infl->update($defl->update($data))) {
			printf("uh-oh »%s« != »%s«\n", $data, $d);
		}
	}
}

echo $infl->update($defl->finish());
echo $infl->finish();
?>
DONE
--EXPECT--
Test
DONE
