--TEST--
url parser with query
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
	"s:?q",
	"ss:?qq",
	"s:/?q",
	"ss:/?qq",
	"s://?q",
	"ss://?qq",
	"s://h?q",
	"ss://hh?qq",
	"s://h/p?q",
	"ss://hh/pp?qq",
	"s://h:123/p/?q",
	"ss://hh:123/pp/?qq",
);

foreach ($urls as $url) {
	printf("\n%s\n", $url);
	var_dump(new http\Url($url, null, 0));
}
?>
DONE
--EXPECTF--
Test

s:?q
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  NULL
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  string(1) "q"
  ["fragment"]=>
  NULL
}

ss:?qq
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(2) "ss"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  NULL
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  string(2) "qq"
  ["fragment"]=>
  NULL
}

s:/?q
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  NULL
  ["port"]=>
  NULL
  ["path"]=>
  string(1) "/"
  ["query"]=>
  string(1) "q"
  ["fragment"]=>
  NULL
}

ss:/?qq
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(2) "ss"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  NULL
  ["port"]=>
  NULL
  ["path"]=>
  string(1) "/"
  ["query"]=>
  string(2) "qq"
  ["fragment"]=>
  NULL
}

s://?q
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  NULL
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  string(1) "q"
  ["fragment"]=>
  NULL
}

ss://?qq
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(2) "ss"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  NULL
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  string(2) "qq"
  ["fragment"]=>
  NULL
}

s://h?q
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(1) "h"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  string(1) "q"
  ["fragment"]=>
  NULL
}

ss://hh?qq
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(2) "ss"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(2) "hh"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  string(2) "qq"
  ["fragment"]=>
  NULL
}

s://h/p?q
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(1) "h"
  ["port"]=>
  NULL
  ["path"]=>
  string(2) "/p"
  ["query"]=>
  string(1) "q"
  ["fragment"]=>
  NULL
}

ss://hh/pp?qq
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(2) "ss"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(2) "hh"
  ["port"]=>
  NULL
  ["path"]=>
  string(3) "/pp"
  ["query"]=>
  string(2) "qq"
  ["fragment"]=>
  NULL
}

s://h:123/p/?q
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(1) "h"
  ["port"]=>
  int(123)
  ["path"]=>
  string(3) "/p/"
  ["query"]=>
  string(1) "q"
  ["fragment"]=>
  NULL
}

ss://hh:123/pp/?qq
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(2) "ss"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(2) "hh"
  ["port"]=>
  int(123)
  ["path"]=>
  string(4) "/pp/"
  ["query"]=>
  string(2) "qq"
  ["fragment"]=>
  NULL
}
DONE
