--TEST--
client proxy - don't send proxy headers for a standard request
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

server("proxy.inc", function($port, $stdin, $stdout, $stderr) {
	echo "Server on port $port\n";
	$c = new http\Client;
	$r = new http\Client\Request("GET", "http://localhost:$port/");
	$r->setOptions(array(
		"timeout" => 3,
		"proxyheader" => array("Hello" => "there!"),
	));
	try {
		$c->enqueue($r)->send();
	} catch (Exception $e) {
		echo $e;
	}
	echo $c->getResponse()->getBody();
	unset($r, $client);
});

?>
===DONE===
--EXPECTF--
Test
Server on port %d
GET / HTTP/1.1
Accept: */*
Host: localhost:%d
User-Agent: PECL_HTTP/%s PHP/%s libcurl/%s

===DONE===
