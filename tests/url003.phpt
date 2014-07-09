--TEST--
url modification
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$u = "http://user:pass@www.example.com:8080/path/file.ext".
			"?foo=bar&more[]=1&more[]=2#hash";

$tmp = new http\Url($u);
$mod = $tmp->mod(array("query" => "set=1"), http\Url::REPLACE);
var_dump($tmp->toArray() != $mod->toArray());
var_dump("set=1" === $mod->query);
var_dump("new_fragment" === $tmp->mod("#new_fragment")->fragment);

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
DONE
