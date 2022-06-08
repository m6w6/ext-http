--TEST--
env reset
--SKIPIF--
<?php include "skipif.inc"; ?>
--POST--
a=b
--ENV--
HTTP_HOST=foo.bar
HTTP_ACCEPT=*/*
--FILE--
<?php

var_dump(http\Env::getRequestHeader("nono"));
var_dump(http\Env::getRequestHeader("Host"));
var_dump(http\Env::getRequestHeader("content-type"));
$hdr = http\Env::getRequestHeader();
ksort($hdr);
var_dump($hdr);

$_SERVER["HTTP_NONO"] = "yesyes";
unset($_SERVER["HTTP_HOST"]);

var_dump(http\Env::getRequestHeader("nono"));
var_dump(http\Env::getRequestHeader("Host"));

http\Env::reset();

var_dump(http\Env::getRequestHeader("nono"));
var_dump(http\Env::getRequestHeader("Host"));
var_dump(http\Env::getRequestHeader("content-type"));
$hdr = http\Env::getRequestHeader();
ksort($hdr);
var_dump($hdr);
?>
==DONE==
--EXPECTF--
NULL
string(%d) "foo.bar"
string(%d) "application/x-www-form-urlencoded"
array(4) {
  ["Accept"]=>
  string(3) "*/*"
  ["Content-Length"]=>
  string(1) "3"
  ["Content-Type"]=>
  string(33) "application/x-www-form-urlencoded"
  ["Host"]=>
  string(7) "foo.bar"
}
NULL
string(%d) "foo.bar"
string(%d) "yesyes"
NULL
string(%d) "application/x-www-form-urlencoded"
array(4) {
  ["Accept"]=>
  string(3) "*/*"
  ["Content-Length"]=>
  string(1) "3"
  ["Content-Type"]=>
  string(33) "application/x-www-form-urlencoded"
  ["Nono"]=>
  string(6) "yesyes"
}
==DONE==
