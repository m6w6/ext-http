--TEST--
parse cookie
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--FILE--
<?php
echo "-TEST\n";

var_dump(http_parse_cookie('foo')->cookies['foo']);
var_dump(http_parse_cookie('foo;')->cookies['foo']);
var_dump(http_parse_cookie('foo ')->cookies['foo']);
var_dump(http_parse_cookie('foo ;')->cookies['foo']);
var_dump(http_parse_cookie('foo ; ')->cookies['foo']);
var_dump(http_parse_cookie('foo=')->cookies['foo']);
var_dump(http_parse_cookie('foo=;')->cookies['foo']);
var_dump(http_parse_cookie('foo =')->cookies['foo']);
var_dump(http_parse_cookie('foo =;')->cookies['foo']);
var_dump(http_parse_cookie('foo= ')->cookies['foo']);
var_dump(http_parse_cookie('foo= ;')->cookies['foo']);

var_dump(http_parse_cookie('foo=1')->cookies['foo']);
var_dump(http_parse_cookie('foo=1;')->cookies['foo']);
var_dump(http_parse_cookie('foo=1 ;')->cookies['foo']);
var_dump(http_parse_cookie('foo= 1;')->cookies['foo']);
var_dump(http_parse_cookie('foo = 1;')->cookies['foo']);
var_dump(http_parse_cookie('foo = 1 ;')->cookies['foo']);
var_dump(http_parse_cookie('foo=1')->cookies['foo']);
var_dump(http_parse_cookie('foo= 1')->cookies['foo']);

var_dump(http_parse_cookie('foo="1"')->cookies['foo']);
var_dump(http_parse_cookie('foo="1" ')->cookies['foo']);
var_dump(http_parse_cookie('foo="1";')->cookies['foo']);
var_dump(http_parse_cookie('foo = "1" ;')->cookies['foo']);
var_dump(http_parse_cookie('foo= "1" ')->cookies['foo']);

var_dump(http_parse_cookie('foo=""')->cookies['foo']);
var_dump(http_parse_cookie('foo="\""')->cookies['foo']);
var_dump(http_parse_cookie('foo=" "')->cookies['foo']);
var_dump(http_parse_cookie('foo= "')->cookies['foo']);
var_dump(http_parse_cookie('foo=" ')->cookies['foo']);
var_dump(http_parse_cookie('foo= " ')->cookies['foo']);

echo "Done\n";
?>
--EXPECTF--
%sTEST
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(1) "1"
string(0) ""
string(1) """
string(1) " "
string(1) """
string(1) """
string(1) """
Done
