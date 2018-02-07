--TEST--
url parser multibyte/locale/topct
--SKIPIF--
<?php
include "skipif.inc";
if (!defined("http\\Url::PARSE_MBLOC") or
	!defined("http\\Url::PARSE_TOIDN") or
	!utf8locale()) {
	die("skip need http\\Url::PARSE_MBLOC|http\Url::PARSE_TOIDN support and LC_CTYPE=*.UTF-8");
}

?>
--FILE--
<?php
echo "Test\n";
setlocale(LC_CTYPE, "C.UTF-8");

$urls = array(
	"http://mike:paßwort@𐌀𐌁𐌂.it/for/€/?by=¢#ø"
);

foreach ($urls as $url) {
	var_dump(new http\Url($url, null, http\Url::PARSE_MBLOC|http\Url::PARSE_TOPCT|http\Url::PARSE_TOIDN));
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
