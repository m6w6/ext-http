--TEST--
client features
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
	$client->enablePipelining(true);
	$client->enableEvents(true);
	
	$client->enqueue($request);
	$client->enqueue(clone $request);
	$client->enqueue(clone $request);
	
	$client->send();
	
	while ($client->getResponse()) {
		echo "R\n";
	}
}

?>
Done
--EXPECTREGEX--
Test
(?:R
R
R
)+Done
