--TEST--
client static cookies
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

function x($a) {
	$r[key($a)]=end($a);
	$r[key($a)]=reset($a);
	return $r;
}

server("env.inc", function($port) {
	$client = new http\Client;
	$client->setCookies(array("test" => "bar"));
	$client->addCookies(array("foo" => "test"));

	$request = new http\Client\Request("GET", "http://localhost:$port");
	$client->enqueue($request);
	$client->send();
	echo $client->getResponse()->getBody()->toString();

	$request->setOptions(array("cookies" => x($client->getCookies())));
	$client->requeue($request);
	$client->send();

	echo $client->getResponse()->getBody()->toString();
});
?>
Done
--EXPECT--
Test
Array
(
    [test] => bar
    [foo] => test
)
Array
(
    [test] => test
    [foo] => bar
)
Done
