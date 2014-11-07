--TEST--
url parser multibyte/utf-8/topct
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
	"http://mike:paßwort@sörver.net/for/€/?by=¢#ø"
);

foreach ($urls as $url) {
	var_dump(new http\Url($url, null, http\Url::PARSE_MBUTF8|http\Url::PARSE_TOPCT));
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
  string(11) "sörver.net"
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
