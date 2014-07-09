--TEST--
url as array
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$u = "http://user:pass@www.example.com:8080/path/file.ext".
			"?foo=bar&more[]=1&more[]=2#hash";

$url = new http\Url($u);
$url2 = new http\Url($url->toArray());
var_dump($url->toArray() === $url2->toArray());
?>
DONE
--EXPECT--
Test
bool(true)
DONE
