--TEST--
client proxy - send proxy headers for a proxy request
--SKIPIF--
<?php 
include "skipif.inc";
skip_online_test();
skip_client_test();
$client = new http\Client("curl");
array_key_exists("proxyheader", $client->getAvailableOptions())
	or die("skip need libcurl with CURLOPT_PROXYHEADER support\n");
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
Hello: there!
Host: www.example.com:80
%r(Proxy-Connection: Keep-Alive
)?%rUser-Agent: PECL_HTTP/%s PHP/%s libcurl/%s

===DONE===
