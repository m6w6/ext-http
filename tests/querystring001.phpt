--TEST--
query string
--SKIPIF--
<?php
include("skipif.inc");
?>
--GET--
str=abc&num=-123&dec=123.123&bool=1&arr[]=1&arr[]=2&ma[l1][l2]=2&ma[l2][l3][l4]=3
--FILE--
<?php
echo "Test\n";

printf("\nGlobal instance:\n");
$q = http\QueryString::getGlobalInstance();
printf("%s\n", $q);

printf("\nStandard getters:\n");
var_dump($q->getString("str"));
var_dump($q->getInt("num"));
var_dump($q->getFloat("dec"));
var_dump($q->getInt("dec"));
var_dump($q->getFloat("dec"));
var_dump($q->getBool("bool"));
var_dump($q->getInt("bool"));
var_dump($q->getBool("num"));
var_dump($q->getInt("num"));
var_dump($q->getArray("arr"));
var_dump($q->getArray("ma"));
var_dump($q->getObject("arr"));
var_dump($q->getObject("ma"));

$s = $q->toString();

printf("\nClone modifications do not alter global instance:\n");
$q->mod(array("arr" => array(3 => 3)));
printf("%s\n", $q);

printf("\nClone modifications do not alter standard instance:\n");
$q2 = new http\QueryString($s);
$q3 = $q2->mod(array("arr" => array(3 => 3)));
printf("%s\n%s\n", $q2, $q3);
#var_dump($q2, $q3);

printf("\nIterator:\n");
$it = new RecursiveIteratorIterator($q2, RecursiveIteratorIterator::SELF_FIRST);
foreach ($it as $k => $v) {
	$i = $it->getDepth()*8;
	@printf("%{$i}s: %s\n", $k, $v); 
}

printf("\nReplace a multi dimensional key:\n");
printf("%s\n", $q2->mod(array("ma" => null))->set(array("ma" => array("l1" => false))));

printf("\nXlate:\n");
$qu = new http\QueryString("ü=ö");
printf("utf8:   %s\n", $qu);
printf("latin1: %s\n", method_exists($qu, "xlate") ? $qu->xlate("utf-8", "latin1") : "%FC=%F6");

printf("\nOffsets:\n");
var_dump($q2["ma"]);
$q2["ma"] = array("bye");
var_dump($q2["ma"]);
var_dump(isset($q2["ma"]));
unset($q2["ma"]);
var_dump(isset($q2["ma"]));

echo "Done\n";
?>
--EXPECTF--
Test

Global instance:
str=abc&num=-123&dec=123.123&bool=1&arr%5B0%5D=1&arr%5B1%5D=2&ma%5Bl1%5D%5Bl2%5D=2&ma%5Bl2%5D%5Bl3%5D%5Bl4%5D=3

Standard getters:
string(3) "abc"
int(-123)
float(123.123)
int(123)
float(123.123)
bool(true)
int(1)
bool(true)
int(-123)
array(2) {
  [0]=>
  string(1) "1"
  [1]=>
  string(1) "2"
}
array(2) {
  ["l1"]=>
  array(1) {
    ["l2"]=>
    string(1) "2"
  }
  ["l2"]=>
  array(1) {
    ["l3"]=>
    array(1) {
      ["l4"]=>
      string(1) "3"
    }
  }
}
object(stdClass)#%d (2) {
  [0]=>
  string(1) "1"
  [1]=>
  string(1) "2"
}
object(stdClass)#%d (2) {
  ["l1"]=>
  array(1) {
    ["l2"]=>
    string(1) "2"
  }
  ["l2"]=>
  array(1) {
    ["l3"]=>
    array(1) {
      ["l4"]=>
      string(1) "3"
    }
  }
}

Clone modifications do not alter global instance:
str=abc&num=-123&dec=123.123&bool=1&arr%5B0%5D=1&arr%5B1%5D=2&ma%5Bl1%5D%5Bl2%5D=2&ma%5Bl2%5D%5Bl3%5D%5Bl4%5D=3

Clone modifications do not alter standard instance:
str=abc&num=-123&dec=123.123&bool=1&arr%5B0%5D=1&arr%5B1%5D=2&ma%5Bl1%5D%5Bl2%5D=2&ma%5Bl2%5D%5Bl3%5D%5Bl4%5D=3
str=abc&num=-123&dec=123.123&bool=1&arr%5B0%5D=1&arr%5B1%5D=2&arr%5B3%5D=3&ma%5Bl1%5D%5Bl2%5D=2&ma%5Bl2%5D%5Bl3%5D%5Bl4%5D=3

Iterator:
str: abc
num: -123
dec: 123.123
bool: 1
arr: Array
       0: 1
       1: 2
ma: Array
      l1: Array
              l2: 2
      l2: Array
              l3: Array
                      l4: 3

Replace a multi dimensional key:
str=abc&num=-123&dec=123.123&bool=1&arr%5B0%5D=1&arr%5B1%5D=2&ma%5Bl1%5D=

Xlate:
utf8:   %C3%BC=%C3%B6
latin1: %FC=%F6

Offsets:
array(2) {
  ["l1"]=>
  array(1) {
    ["l2"]=>
    string(1) "2"
  }
  ["l2"]=>
  array(1) {
    ["l3"]=>
    array(1) {
      ["l4"]=>
      string(1) "3"
    }
  }
}
array(1) {
  [0]=>
  string(3) "bye"
}
bool(true)
bool(false)
Done
