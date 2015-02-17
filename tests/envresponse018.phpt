--TEST--
env response don't generate stat based etag for temp stream
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 
echo "Test\n";

$b = new http\Message\Body(fopen("php://temp/maxmemory:8", "r+"));
$b->append("1234567890\n");

$r = new http\Env\Response;
$r->setBody($b);
$r->send(STDOUT);

?>
===DONE===
--EXPECTF--
Test
HTTP/1.1 200 OK
Accept-Ranges: bytes
ETag: "%x"
Last-Modified: %s
Transfer-Encoding: chunked

b
1234567890

0

===DONE===
