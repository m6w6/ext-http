--TEST--
request header
--SKIPIF--
<? include "skipif.inc";
--POST--
a=b
--ENV--
HTTP_HOST=foo.bar
HTTP_ACCEPT=*/*
--FILE--
<?

var_dump(http\Env::getRequestHeader("nono"));
var_dump(http\Env::getRequestHeader("Host"));
var_dump(http\Env::getRequestHeader("content-type"));
var_dump(http\Env::getRequestHeader());
--EXPECTF--
NULL
string(%d) "foo.bar"
string(%d) "application/x-www-form-urlencoded"
array(4) {
  ["Host"]=>
  string(7) "foo.bar"
  ["Accept"]=>
  string(3) "*/*"
  ["Content-Length"]=>
  string(1) "3"
  ["Content-Type"]=>
  string(33) "application/x-www-form-urlencoded"
}
