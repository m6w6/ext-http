--TEST--
client proxy - send proxy headers for a proxy request
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
	$r = new http\Client\Request("GET", "http://www.example.com/");
	$r->setOptions(array(
			"timeout" => 10,
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
});

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
===DONE===
