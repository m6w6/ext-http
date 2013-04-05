--TEST--
client response transfer info
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("GET", "http://www.example.org");

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$response = $client->enqueue($request)->send()->getResponse();
	var_dump($response->getTransferInfo("response_code"));
	var_dump(count($response->getTransferInfo()));
}
?>
Done
--EXPECTREGEX--
Test
(?:int\([1-5]\d\d\)
int\(\d\d\)
)+Done
