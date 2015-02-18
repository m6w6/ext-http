--TEST--
message parser with nonblocking stream
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php 
echo "Test\n";

$parser = new http\Message\Parser;
$socket = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
stream_set_blocking($socket[0], 0);

$message = array(
"GET / HTTP/1.1\n",
"Host: localhost\n",
"Content-length: 3\n",
"\n",
"OK\n"	
);

while ($message) {
	$line = array_shift($message);
	$parser->stream($socket[0], 0, $msg);
	fwrite($socket[1], $line);
	$parser->stream($socket[0], 0, $msg);
}

var_dump($msg, (string) $msg->getBody());

?>
DONE
--EXPECTF--
Test
object(http\Message)#%d (9) {
  ["type":protected]=>
  int(1)
  ["body":protected]=>
  object(http\Message\Body)#%d (0) {
  }
  ["requestMethod":protected]=>
  string(3) "GET"
  ["requestUrl":protected]=>
  string(1) "/"
  ["responseStatus":protected]=>
  string(0) ""
  ["responseCode":protected]=>
  int(0)
  ["httpVersion":protected]=>
  string(3) "1.1"
  ["headers":protected]=>
  array(3) {
    ["Host"]=>
    string(9) "localhost"
    ["Content-Length"]=>
    string(1) "3"
    ["X-Original-Content-Length"]=>
    string(1) "3"
  }
  ["parentMessage":protected]=>
  NULL
}
string(3) "OK
"
DONE
