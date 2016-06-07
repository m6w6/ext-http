--TEST--
client pipelining
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php 

include "helper/server.inc";

echo "Test\n";

function dump($response) {
	echo $response->getHeader("X-Req"),"\n";
}

server("pipeline.inc", function($port, $stdin, $stdout, $stderr) {
	/* tell the server we're about to send 3 pipelined messages */
	fputs($stdin, "3\n");
	
	$client = new http\Client(null);
	$client->configure(array("pipelining" => true, "max_host_connections" => 0));
	
	/* this is just to let curl know the server may be capable of pipelining */
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port"), "dump");
	$client->send();
	
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/1"), "dump");
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/2"), "dump");
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/3"), "dump");
	$client->send();
});

?>
===DONE===
--EXPECT--
Test
/
/1
/2
/3
===DONE===
