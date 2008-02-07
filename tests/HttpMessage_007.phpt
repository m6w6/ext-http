--TEST--
HttpMessage::reverse()
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
?>
--FILE--
<?php
echo "-TEST\n";
$s = "GET /first HTTP/1.1\nHTTP/1.1 200 Ok-first\nGET /second HTTP/1.1\nHTTP/1.1 200 Ok-second\nGET /third HTTP/1.1\nHTTP/1.1 200 Ok-third\n";
echo HttpMessage::fromString($s)->toString(true);
echo "===\n";
echo HttpMessage::fromString($s)->reverse()->toString(true);

$m = new HttpMessage($s);
$r = $m->reverse();
unset($m);
var_dump($r->count());
echo $r->toString(true);

echo "Done\n";
?>
--EXPECTF--
%aTEST
GET /first HTTP/1.1
HTTP/1.1 200 Ok-first
GET /second HTTP/1.1
HTTP/1.1 200 Ok-second
GET /third HTTP/1.1
HTTP/1.1 200 Ok-third
===
HTTP/1.1 200 Ok-third
GET /third HTTP/1.1
HTTP/1.1 200 Ok-second
GET /second HTTP/1.1
HTTP/1.1 200 Ok-first
GET /first HTTP/1.1
int(6)
HTTP/1.1 200 Ok-third
GET /third HTTP/1.1
HTTP/1.1 200 Ok-second
GET /second HTTP/1.1
HTTP/1.1 200 Ok-first
GET /first HTTP/1.1
Done
