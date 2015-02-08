--TEST--
proxy - don't send proxy headers for a standard request
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

echo "Test\n";

$spec = array(array("pipe","r"), array("pipe","w"), array("pipe","w"));
if (($proc = proc_open(PHP_BINARY . " proxy.inc", $spec, $pipes, __DIR__))) {
	$port = trim(fgets($pipes[2]));
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
	while (!feof($pipes[1])) {
		echo fgets($pipes[1]);
	}
	unset($r, $client);
}
?>
===DONE===
--EXPECTF--
Test
Server on port %d
GET / HTTP/1.1
User-Agent: PECL_HTTP/%s PHP/%s libcurl/%s
Host: localhost:%d
Accept: */*
Content-Length: 0
===DONE===
