--TEST--
encoding stream brotli auto flush
--SKIPIF--
<?php
include "skipif.inc";
class_exists("http\\Encoding\\Stream\\Enbrotli") or die("SKIP need brotli support");

?>
--FILE--
<?php
echo "Test\n";

$defl = new http\Encoding\Stream\Enbrotli(http\Encoding\Stream::FLUSH_FULL);
$infl = new http\Encoding\Stream\Debrotli;

for ($f = fopen(__FILE__, "rb"); !feof($f); $data = fread($f, 0x100)) {
	if (isset($data)) {
		if ($data !== $d=$infl->update($defl->update($data))) {
			printf("uh-oh »%s« != »%s«\n", $data, $d);
		}
	}
}

echo $infl->update($defl->finish());
echo $infl->finish();

var_dump($infl->done(), $defl->done());
?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
