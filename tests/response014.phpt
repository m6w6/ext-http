--TEST--
response invalid ranges
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
$f = tmpfile();
$req = new http\Env\Request;
$req->setHeader("Range", "bytes=321-123,123-0");
$res = new http\Env\Response;
$res->getBody()->append("foobar");
$res->setEnvRequest($req);
$res->send($f);
rewind($f);
var_dump(stream_get_contents($f));
--EXPECTF--
string(96) "HTTP/1.1 416 Requested Range Not Satisfiable
Accept-Ranges: bytes
Content-Range: bytes */6

"

