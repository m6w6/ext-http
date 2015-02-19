--TEST--
url parser
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
	"s:",
	"ss:",
	"s:a",
	"ss:aa",
	"s://",
	"ss://",
	"s://a",
	"ss://aa",
);

foreach ($urls as $url) {
	printf("\n%s\n", $url);
	var_dump(new http\Url($url, null, 0));
}

?>
DONE
--EXPECTF--
Test

s:
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
  NULL
  ["fragment"]=>
  NULL
}

ss:
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
  NULL
  ["fragment"]=>
  NULL
}

s:a
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
  string(1) "a"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

ss:aa
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
  string(2) "aa"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://
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
  NULL
  ["fragment"]=>
  NULL
}

ss://
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
  NULL
  ["fragment"]=>
  NULL
}

s://a
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
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

ss://aa
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(2) "ss"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(2) "aa"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}
DONE
