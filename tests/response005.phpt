--TEST--
http response cache positive
--SKIPIF--
<?php include "skipif.inc"; ?>
--GET--
a=b
--ENV--
HTTP_IF_MODIFIED_SINCE=Fri, 13 Feb 2009 23:31:32 GMT
--FILE--
<?php
$r = new http\Env\Response;
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
