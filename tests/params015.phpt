--TEST--
header params rfc5987 regression
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";
$p = new http\Params(array("attachment"=>array("filename"=>"foo.bar")));
var_dump($p->params);
var_dump((string)$p);
?>
===DONE===
--EXPECT--
Test
array(1) {
  ["attachment"]=>
  array(1) {
    ["filename"]=>
    string(7) "foo.bar"
  }
}
string(27) "attachment;filename=foo.bar"
===DONE===
