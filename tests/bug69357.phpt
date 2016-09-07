--TEST--
Bug #69357 (HTTP/1.1 100 Continue overriding subsequent 200 response code with PUT request)
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php 
echo "Test\n";

include "helper/server.inc";

server("upload.inc", function($port) {
	$b = new \http\Message\Body;
	$b->append("foo");
	$r = new \http\Client\Request("PUT", "http://localhost:$port/", array(), $b);
	$c = new \http\Client;
	$c->setOptions(array("expect_100_timeout" => 0));
	$c->enqueue($r)->send();
	
	var_dump($c->getResponse($r)->getInfo());
	var_dump($c->getResponse($r)->getHeaders());
});

?>
===DONE===
--EXPECTF--
Test
string(15) "HTTP/1.1 200 OK"
array(4) {
  ["Accept-Ranges"]=>
  string(5) "bytes"
  ["Etag"]=>
  string(10) ""%x""
  ["X-Original-Transfer-Encoding"]=>
  string(7) "chunked"
  ["Content-Length"]=>
  int(%d)
}
===DONE===
