--TEST--
client drivers
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	var_dump($client instanceof http\Client);
}

?>
Done
--EXPECTREGEX--
Test
(?:bool\(true\)
)+Done
