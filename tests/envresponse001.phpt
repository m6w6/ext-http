--TEST--
env response message
--SKIPIF--
<?php include "skipif.inc"; ?>
--POST--
a=b
--ENV--
HTTP_ACCEPT_ENCODING=gzip
--FILE--
<?php

$r = new http\env\Response;
$r->setHeader("foo","bar");
$r->setContentEncoding(http\env\Response::CONTENT_ENCODING_GZIP);
$r->setBody(new http\message\Body(fopen(__FILE__,"r")));
$r->send();

--EXPECTHEADERS--
Foo: bar
Content-Encoding: gzip
Vary: Accept-Encoding
--EXPECTREGEX--
^\x1f\x8b\x08.+
