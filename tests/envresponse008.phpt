--TEST--
env response stream message
--SKIPIF--
<?php include "skipif.inc"; ?>
--ENV--
HTTP_ACCEPT_ENCODING=gzip
--FILE--
<?php

$f = tmpfile();

$r = new http\env\Response;
$r->setHeader("foo","bar");
$r->setContentEncoding(http\env\Response::CONTENT_ENCODING_GZIP);
$r->setBody(new http\message\Body(fopen(__FILE__,"r")));
$r->send($f);

rewind($f);
var_dump(stream_get_contents($f));

--EXPECTREGEX--
string\(\d+\) "HTTP\/1\.1 200 OK
Accept-Ranges: bytes
Foo: bar
Content-Encoding: gzip
Vary: Accept-Encoding
ETag: "\w+-\w+-\w+"
Last-Modified: \w+, \d+ \w+ \d{4} \d\d:\d\d:\d\d GMT

\x1f\x8b\x08.+
