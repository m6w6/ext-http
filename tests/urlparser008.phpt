--TEST--
url parser ipv6
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
	"s://[a:80",
	"s://[0]",
	"s://[::1]:80",
	"s://mike@[0:0:0:0:0:FFFF:204.152.189.116]/foo",
);

foreach ($urls as $url) {
	try {
		printf("\n%s\n", $url);
		var_dump(new http\Url($url, null, 0));
	} catch (Exception $e) {
		echo $e->getMessage(),"\n";
	}
}
?>
DONE
--EXPECTF--
Test

s://[a:80
http\Url::__construct(): Failed to parse hostinfo; expected ']' at pos 5 in '[a:80'

s://[0]
http\Url::__construct(): Failed to parse hostinfo; unexpected '[' at pos 0 in '[0]'

s://[::1]:80
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  NULL
  ["pass"]=>
  NULL
  ["host"]=>
  string(5) "[::1]"
  ["port"]=>
  int(80)
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://mike@[0:0:0:0:0:FFFF:204.152.189.116]/foo
object(http\Url)#%d (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(4) "mike"
  ["pass"]=>
  NULL
  ["host"]=>
  string(24) "[::ffff:204.152.189.116]"
  ["port"]=>
  NULL
  ["path"]=>
  string(4) "/foo"
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}
DONE
