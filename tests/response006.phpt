--TEST--
http response stream
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$f = tmpfile();

$r = new http\Env\Response;
$r->addHeader("foo", array("bar","baz"));
$r->getBody()->append("foo");

$r->send($f);

rewind($f);
var_dump(stream_get_contents($f));
?>
Done
--EXPECT--
Test
string(77) "HTTP/1.1 200 OK
Accept-Ranges: bytes
Foo: bar, baz
ETag: "8c736521"

foo"
Done
