--TEST--
env response cookie
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

$r = new http\Env\Response;
$c = new http\Cookie;
$c->addCookie("foo","bar");
$c->setMaxAge(60);
$r->setCookie($c);
$r->setCookie("baz");
$r->setCookie(123);
$r->send(STDOUT);

?>
--EXPECT--
HTTP/1.1 200 OK
Set-Cookie: foo=bar; max-age=60; 
Set-Cookie: baz=1; 
Set-Cookie: 123=1; 
ETag: ""
Transfer-Encoding: chunked

0

