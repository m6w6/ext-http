--TEST--
url parser userinfo
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
	"s://:@",
	"s://u@",
	"s://u:@",
	"s://u:p@",
	"s://user:pass@",
	"s://user:pass@host",
	"s://u@h",
	"s://user@h",
	"s://u@host",
	"s://user:p@h",
	"s://user:pass@h",
	"s://user:pass@host",
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

s://:@
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(0) ""
  ["pass"]=>
  string(0) ""
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

s://u@
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(1) "u"
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

s://u:@
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(1) "u"
  ["pass"]=>
  string(0) ""
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

s://u:p@
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(1) "u"
  ["pass"]=>
  string(1) "p"
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

s://user:pass@
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(4) "user"
  ["pass"]=>
  string(4) "pass"
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

s://user:pass@host
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(4) "user"
  ["pass"]=>
  string(4) "pass"
  ["host"]=>
  string(4) "host"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://u@h
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(1) "u"
  ["pass"]=>
  NULL
  ["host"]=>
  string(1) "h"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://user@h
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(4) "user"
  ["pass"]=>
  NULL
  ["host"]=>
  string(1) "h"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://u@host
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(1) "u"
  ["pass"]=>
  NULL
  ["host"]=>
  string(4) "host"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://user:p@h
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(4) "user"
  ["pass"]=>
  string(1) "p"
  ["host"]=>
  string(1) "h"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://user:pass@h
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(4) "user"
  ["pass"]=>
  string(4) "pass"
  ["host"]=>
  string(1) "h"
  ["port"]=>
  NULL
  ["path"]=>
  NULL
  ["query"]=>
  NULL
  ["fragment"]=>
  NULL
}

s://user:pass@host
object(http\Url)#1 (8) {
  ["scheme"]=>
  string(1) "s"
  ["user"]=>
  string(4) "user"
  ["pass"]=>
  string(4) "pass"
  ["host"]=>
  string(4) "host"
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
