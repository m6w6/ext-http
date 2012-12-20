--TEST--
http response stream cache negative
--SKIPIF--
<?php include "skipif.inc"; ?>
--GET--
a=b
--ENV--
HTTP_IF_MODIFIED_SINCE=Fri, 13 Feb 2009 23:31:30 GMT
HTTP_IF_NONE_MATCH=0000-00-0000
--FILE--
<?php
$f = tmpfile();
$r = new http\Env\Response;
$r->setBody(new http\Message\Body(fopen(__FILE__,"rb")));
$r->setEtag("abc");
$r->setLastModified(1234567891);
$r->send($f);
rewind($f);
var_dump(stream_get_contents($f));
?>
--EXPECTF--
string(355) "HTTP/1.1 200 OK%c
Accept-Ranges: bytes%c
X-Powered-By: %s%c
ETag: "abc"%c
Last-Modified: %s%c
%c
<?php
$f = tmpfile();
$r = new http\Env\Response;
$r->setBody(new http\Message\Body(fopen(__FILE__,"rb")));
$r->setEtag("abc");
$r->setLastModified(1234567891);
$r->send($f);
rewind($f);
var_dump(stream_get_contents($f));
?>
"

