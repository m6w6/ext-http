--TEST--
url parser multibyte/utf-8/topct
--SKIPIF--
<?php
include "skipif.inc";
defined("http\\Url::PARSE_TOIDN") or
	die("skip need http\\Url::PARSE_TOIDN support");
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
	"http://mike:paßwort@𐌀𐌁𐌂.it/for/€/?by=¢#ø"
);

foreach ($urls as $url) {
	var_dump(new http\Url($url, null, http\Url::PARSE_MBUTF8|http\Url::PARSE_TOPCT|http\Url::PARSE_TOIDN));
}
?>
DONE
--EXPECTF--
Test
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(4) "http"
  ["user"]=>
  string(4) "mike"
  ["pass"]=>
  string(12) "pa%C3%9Fwort"
  ["host"]=>
  string(13) "xn--097ccd.it"
  ["port"]=>
  NULL
  ["path"]=>
  string(15) "/for/%E2%82%AC/"
  ["query"]=>
  string(9) "by=%C2%A2"
  ["fragment"]=>
  string(6) "%C3%B8"
}
DONE
