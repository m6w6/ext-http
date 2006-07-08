--TEST--
HttpQueryString w/ objects
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--FILE--
<?php
echo "-TEST\n";
$foo = (object) array("bar" => (object) array("baz" => 1), "\0*\0prop" => "dontshow");
$foo->bar->baz = 1;
var_dump($q = new HttpQueryString(false, $foo));
$foo->bar->baz = 0;
var_dump($q->mod($foo));
echo "Done\n";
?>
--EXPECTF--
%sTEST
object(HttpQueryString)#3 (2) {
  ["queryArray:private"]=>
  array(1) {
    ["bar"]=>
    array(1) {
      ["baz"]=>
      int(1)
    }
  }
  ["queryString:private"]=>
  string(14) "bar%5Bbaz%5D=1"
}
object(HttpQueryString)#4 (2) {
  ["queryArray:private"]=>
  array(1) {
    ["bar"]=>
    array(1) {
      ["baz"]=>
      int(0)
    }
  }
  ["queryString:private"]=>
  string(14) "bar%5Bbaz%5D=0"
}
Done
