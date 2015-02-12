--TEST--
pipelining
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php 

include "helper/server.inc";

echo "Test\n";

server("pipeline.inc", function($port, $stdin, $stdout, $stderr) {
	/* tell the server we're about to send 3 pipelined messages */
	fputs($stdin, "3\n");
	
	$client = new http\Client(null);
	$client->configure(["pipelining" => true, "max_host_connections" => 0]);
	
	/* this is just to let curl know the server may be capable of pipelining */
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port"));
	$client->send();
	
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/1"));
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/2"));
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/3"));
	$client->send();
	
	while (($response = $client->getResponse())) {
		echo $response;
	}
});

?>
===DONE===
--EXPECT--
Test
HTTP/1.1 200 OK
X-Req: /3
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
X-Req: /2
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
X-Req: /1
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
X-Req: /
Etag: ""
X-Original-Transfer-Encoding: chunked
===DONE===
