--TEST--
HttpQueryString w/ objects
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
?>
--FILE--
<?php
echo "-TEST\n";
class test_props {
	public $bar;
	public $baz;
	protected $dont_show;
	private $dont_show2;
	function __construct() {
		$this->bar = (object) array("baz"=>1);
		$this->dont_show = 'xxx';
		$this->dont_show2 = 'zzz';
	}
}
$foo = new test_props;
var_dump($q = new HttpQueryString(false, $foo));
$foo->bar->baz = 0;
var_dump($q->mod($foo));
echo "Done\n";
?>
--EXPECTF--
%aTEST
object(HttpQueryString)#3 (2) {
  ["queryArray%s]=>
  array(1) {
    ["bar"]=>
    array(1) {
      ["baz"]=>
      int(1)
    }
  }
  ["queryString%s]=>
  string(14) "bar%5Bbaz%5D=1"
}
object(HttpQueryString)#4 (2) {
  ["queryArray%s]=>
  array(1) {
    ["bar"]=>
    array(1) {
      ["baz"]=>
      int(0)
    }
  }
  ["queryString%s]=>
  string(14) "bar%5Bbaz%5D=0"
}
Done
