--TEST--
proxy - send proxy headers for a proxy request
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
	$r = new http\Client\Request("GET", "http://www.example.com/");
	$r->setOptions(array(
		"timeout" => 3,
		"proxytunnel" => true,
		"proxyheader" => array("Hello" => "there!"),
		"proxyhost" => "localhost",
		"proxyport" => $port,
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
CONNECT www.example.com:80 HTTP/1.1
Host: www.example.com:80
User-Agent: PECL_HTTP/%s PHP/%s libcurl/%s
Proxy-Connection: Keep-Alive
Hello: there!
Content-Length: 0
===DONE===
