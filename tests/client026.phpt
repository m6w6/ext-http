--TEST--
client stream 128M
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
	$client = new http\Client;
	$request = new http\Client\Request("PUT", "http://localhost:$port");
	$request->setContentType("application/octet-stream");
	for ($i = 0, $data = str_repeat("a",1024); $i < 128*1024; ++$i) {
		$request->getBody()->append($data);
	}
	$request->setOptions(["expect_100_timeout" => 0]);
	$client->enqueue($request);
	$client->send();
	var_dump($client->getResponse()->getHeaders());
});

?>
===DONE===
--EXPECTF--
Test
array(5) {
  ["Accept-Ranges"]=>
  string(5) "bytes"
  ["Etag"]=>
  string(%d) "%s"
  ["Last-Modified"]=>
  string(%d) "%s"
  ["X-Original-Transfer-Encoding"]=>
  string(7) "chunked"
  ["Content-Length"]=>
  int(134217973)
}
exception 'http\Exception\BadMessageException' with message 'Empty message received from stream' in %sproxy.inc:%d
Stack trace:
#0 %sproxy.inc(%d): http\Message->__construct(Resource id #%d, false)
#1 %sserver.inc(%d): {closure}(Resource id #%d)
#2 %sproxy.inc(%d): serve(Object(Closure))
#3 {main}
===DONE===
