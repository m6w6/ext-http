--TEST--
url as string
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
var_dump((string) $url == (string) new http\Url((string) $url));

?>
DONE
--EXPECT--
Test
bool(true)
DONE
