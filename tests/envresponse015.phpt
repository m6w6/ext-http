--TEST--
env response send replaced body using multiple ob_start
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

$r = new http\Env\Response;

ob_start($r);
echo "bar";
ob_end_flush();

$b = $r->getBody();
$r->setBody(new http\Message\Body);

ob_start($r);
echo "foo: $b\n";
ob_end_flush();

$f = fopen("php://memory", "r+");

$r->send($f);

fseek($f, 0, SEEK_SET);
echo stream_get_contents($f);

?>
--EXPECT--
HTTP/1.1 200 OK
Accept-Ranges: bytes
ETag: "fc8305a1"
Transfer-Encoding: chunked

9
foo: bar

0

