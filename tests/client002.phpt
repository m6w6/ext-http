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
	#[ReturnTypeWillChange]
	function update(SplSubject $client, http\Client\Request $request = null, StdClass $progress = null) {
		echo "P";
		/* fence against buggy infof() calls in some curl versions */
		$compare = $client->getProgressInfo($request);
		if ($progress->info !== "prepare" && $compare && $compare != $progress) {
			var_dump($progress, $compare);
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
