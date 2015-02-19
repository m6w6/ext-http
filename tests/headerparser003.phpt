--TEST--
header parser with nonblocking stream
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$parser = new http\Header\Parser;
$socket = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
stream_set_blocking($socket[0], 0);

$headers = array(
"GET / HTTP/1.1\n",
"Host: localhost","\n",
"Content","-length: 3\n",
"\n",
);

while ($headers) {
	$line = array_shift($headers);
	$parser->stream($socket[0], 0, $hdrs);
	fwrite($socket[1], $line);
	var_dump($parser->getState());
	var_dump($parser->stream($socket[0], 0, $hdrs));
}

var_dump($hdrs);

?>
DONE
--EXPECT--
Test
int(0)
int(1)
int(1)
int(2)
int(2)
int(3)
int(3)
int(1)
int(1)
int(3)
int(3)
int(5)
array(2) {
  ["Host"]=>
  string(9) "localhost"
  ["Content-Length"]=>
  string(1) "3"
}
DONE
