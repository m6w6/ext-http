--TEST--
client observer
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

class Observer implements SplObserver
{
	function update(SplSubject $client, http\Client\Request $request = null, StdClass $progress = null) {
		echo "P";
		if ($progress->info !== "prepare" && $client->getProgressInfo($request) != $progress) {
			var_dump($progress);
		}
	}
}

server("proxy.inc", function($port, $stdin, $stdout, $stderr) {
	foreach (http\Client::getAvailableDrivers() as $driver) {
		$client = new http\Client($driver);
		$client->attach(new Observer);
		$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/"));
		$client->send();
	}
});

?>

Done
--EXPECTREGEX--
Test
P+
Done
