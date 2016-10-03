--TEST--
client eventloop recursion
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php
echo "Test\n";

include "helper/server.inc";

class test implements SplObserver {
	function update(SplSubject $client) {
		$client->once();
	}
}
server("proxy.inc", function($port) {
	$client = new http\Client;
	$client->configure(array(
			"use_eventloop" => true,
	));
	$client->attach(new test);
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/"), function($r) {
		var_dump($r->getResponseCode());
	});
	$client->send();
});

?>
===DONE===
--EXPECTF--
Test
int(200)
===DONE===