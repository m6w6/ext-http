--TEST--
url parser with paths
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
	"s:a/",
	"ss:aa/",
	"s:/a/",
	"ss:/aa/",
	"s://a/",
	"s://h/a",
	"ss://hh/aa",
	"s:///a/b",
	"ss:///aa/bb",
);

foreach ($urls as $url) {
	printf("\n%s\n", $url);
	var_dump(new http\Url($url, null, 0));
}
?>
DONE
--EXPECTF--
Test

s:a/
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
  string(2) "a/"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

ss:aa/
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
  string(3) "aa/"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s:/a/
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
  string(3) "/a/"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

ss:/aa/
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
  string(4) "/aa/"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://a/
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(1) "a"
  ["port"]=>
  NULL
  ["path"]=>
  string(1) "/"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://h/a
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
  string(2) "/a"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

ss://hh/aa
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
  string(3) "/aa"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s:///a/b
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
  string(4) "/a/b"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

ss:///aa/bb
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
  string(6) "/aa/bb"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}
DONE
