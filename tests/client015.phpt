--TEST--
http client event base
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

server("proxy.inc", function($port) {
	$client1 = new http\Client;
	$client2 = new http\Client;
	
	$client1->configure(array("use_eventloop" => true));
	$client2->configure(array("use_eventloop" => true));
	
	$client1->enqueue(new http\Client\Request("GET", "http://localhost:$port/"));
	$client2->enqueue(new http\Client\Request("GET", "http://localhost:$port/"));
	
	$client1->send();
	
	if (($r = $client1->getResponse())) {
		var_dump($r->getTransferInfo("response_code"));
	}
	if (($r = $client2->getResponse())) {
		var_dump($r->getTransferInfo("response_code"));
	}
	
});
?>
DONE
--EXPECT--
Test
int(200)
DONE
