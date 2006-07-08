--TEST--
HttpQueryString local
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--FILE--
<?php
echo "-TEST\n";

$q = new HttpQueryString(false, $array = array('a'=>'b','c'=>'3.4','r'=>array(1,2,3)));
var_dump($q->get());
var_dump($q->get('n'));
var_dump($q->get('a'));
var_dump($q->get('a', "i", 0, true));
var_dump($q->get('a', "string", 'hi!'));
var_dump($q->get('c'));
var_dump($q->get('c', HttpQueryString::TYPE_INT));
var_dump($q->get('c', HttpQueryString::TYPE_FLOAT));
var_dump($q->get('c', HttpQueryString::TYPE_BOOL));
var_dump($q->get('r'));
var_dump($q->get('r', HttpQueryString::TYPE_ARRAY));
var_dump($q->get('r', HttpQueryString::TYPE_OBJECT));

$q->set('z[0]=2');
$q->set(array('a'=>'b', 'c'=> "3.4"));
$q->set(array('a' => NULL));

var_dump($q);
var_dump($array);

echo "Done\n";
?>
--EXPECTF--
%sTEST
string(42) "a=b&c=3.4&r%5B0%5D=1&r%5B1%5D=2&r%5B2%5D=3"
NULL
string(1) "b"
int(0)
string(3) "hi!"
string(3) "3.4"
int(3)
float(3.4)
bool(true)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
object(stdClass)#%d (%d) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
object(HttpQueryString)#1 (2) {
  ["queryArray:private"]=>
  array(3) {
    ["c"]=>
    string(3) "3.4"
    ["r"]=>
    array(3) {
      [0]=>
      int(1)
      [1]=>
      int(2)
      [2]=>
      int(3)
    }
    ["z"]=>
    array(1) {
      [0]=>
      string(1) "2"
    }
  }
  ["queryString:private"]=>
  string(49) "c=3.4&r%5B0%5D=1&r%5B1%5D=2&r%5B2%5D=3&z%5B0%5D=2"
}
array(3) {
  ["a"]=>
  string(1) "b"
  ["c"]=>
  string(3) "3.4"
  ["r"]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
  }
}
Done