--TEST--
response deflate
--SKIPIF--
<?php
include "skipif.inc";
?>
--GET--
dummy=1
--FILE--
<?php

$req = new http\Env\Request;
$req->setHeader("Accept-Encoding", "deflate");

$res = new http\Env\Response;
$res->setCacheControl("public, max-age=3600");
$res->setContentEncoding(http\Env\Response::CONTENT_ENCODING_GZIP);
$res->getBody()->append("foobar");

$res->setEnvRequest($req);
$res->send();
?>
--EXPECTHEADERS--
Content-Encoding: deflate
Vary: Accept-Encoding
Cache-Control: public, max-age=3600
--EXPECTREGEX--
^\x78\x9c.+

