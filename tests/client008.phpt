--TEST--
client pipelining
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
if (version_compare(http\Client\Curl\Versions\CURL, "7.62.0", ">=")) {
	die("skip CURL_VERSION >= 7.62 -- pipelining disabled\n");
}
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
