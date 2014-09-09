--TEST--
client once & wait with events
--SKIPIF--
<?php
include "skipif.inc";
try {
	$client = new http\Client;
	if (!$client->enableEvents())
		throw new Exception("need events support");
} catch (Exception $e) {
	die("skip ".$e->getMessage());
}
skip_online_test();
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("GET", "http://www.example.org/");

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->enableEvents(true);
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
