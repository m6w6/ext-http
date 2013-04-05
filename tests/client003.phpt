--TEST--
client once & wait
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("GET", "http://www.example.org/");

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->enqueue($request);
	
	while ($client->once()) {
		$client->wait(.1);
	}
	
	if (!$client->getResponse()) {
		var_dump($client);
	}
}
?>
Done
--EXPECT--
Test
Done
