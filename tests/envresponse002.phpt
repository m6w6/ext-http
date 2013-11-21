--TEST--
env response cache negative
--SKIPIF--
<?php include "skipif.inc"; ?>
--GET--
a=b
--ENV--
HTTP_IF_MODIFIED_SINCE=Fri, 13 Feb 2009 23:31:30 GMT
HTTP_IF_NONE_MATCH=0000-00-0000
--FILE--
<?php
$r = new http\Env\Response;
$r->setBody(new http\Message\Body(fopen(__FILE__,"rb")));
$r->setEtag("abc");
$r->setLastModified(1234567891);
$r->send();
?>
--EXPECTHEADERS--
ETag: "abc"
Last-Modified: Fri, 13 Feb 2009 23:31:31 GMT
--EXPECT--
<?php
$r = new http\Env\Response;
$r->setBody(new http\Message\Body(fopen(__FILE__,"rb")));
$r->setEtag("abc");
$r->setLastModified(1234567891);
$r->send();
?>
