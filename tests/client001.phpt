--TEST--
client drivers
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
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
