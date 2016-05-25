--TEST--
client configuration
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

server("pipeline.inc", function($port, $stdin) {
	fputs($stdin, "2\n");
	
	$request = new http\Client\Request("GET", "http://localhost:$port");
	
	$client = new http\Client();
	$client->configure(array("pipelining" => true, "use_eventloop" => true));

	$client->enqueue($request);
	$client->send();
	
	$client->enqueue(clone $request);
	$client->enqueue(clone $request);

	$client->send();

	while ($client->getResponse()) {
		echo "R\n";
	}
});

?>
Done
--EXPECTREGEX--
Test
(?:R
R
R
)+Done
