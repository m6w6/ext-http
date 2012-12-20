--TEST--
http response cache positive with env message
--SKIPIF--
<?php include "skipif.inc"; ?>
--GET--
dummy=1
--FILE--
<?php
$e = new http\Env\Request;
$e->setHeader("If-Modified-Since", "Fri, 13 Feb 2009 23:31:32 GMT");
$r = new http\Env\Response;
$r->setEnvRequest($e);
$r->setBody(new http\Message\Body(fopen(__FILE__,"rb")));
$r->setEtag("abc");
$r->setLastModified(1234567891);
$r->isCachedByEtag("If-None-Match") and die("Huh? etag? really?\n");
$r->isCachedByLastModified("If-Modified-Since") or die("yep, I should be cached");
$r->send();
?>
--EXPECTHEADERS--
HTTP/1.1 304 Not Modified
ETag: "abc"
Last-Modified: Fri, 13 Feb 2009 23:31:31 GMT
--EXPECT--
