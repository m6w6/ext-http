--TEST--
message part
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$p = new http\Message;
$p->addHeader("Content-Type", "text/plain");
$p->getBody()->append("data");
	
$m = new http\Message("HTTP/1.1 200");
$m->getBody()->addPart($p);
echo $m;

?>
Done
--EXPECTF--
Test
HTTP/1.1 200
Content-Length: %d
Content-Type: multipart/form-data; boundary="%x.%x"

--%x.%x
Content-Type: text/plain
Content-Length: 4

data
--%x.%x--
Done
