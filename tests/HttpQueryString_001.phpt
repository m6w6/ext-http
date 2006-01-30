--TEST--
HttpQueryString global
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
?>
--FILE--
<?php
echo "-TEST\n";

$_GET = array('a'=>'b','c'=>'3.4','r'=>array(1,2,3));
$_SERVER['QUERY_STRING'] = 'a=b&c=3.4&r[0]=1&r[1]=2&r[2]=3';

var_dump(HttpQueryString::getInstance()->get());
var_dump(HttpQueryString::getInstance()->get('n'));
var_dump(HttpQueryString::getInstance()->get('a'));
var_dump(HttpQueryString::getInstance()->get('a', "i", 0, true));
var_dump(HttpQueryString::getInstance()->get('a', "string", 'hi!'));
var_dump(HttpQueryString::getInstance()->get('c'));
var_dump(HttpQueryString::getInstance()->get('c', HttpQueryString::TYPE_INT));
var_dump(HttpQueryString::getInstance()->get('c', HttpQueryString::TYPE_FLOAT));
var_dump(HttpQueryString::getInstance()->get('c', HttpQueryString::TYPE_BOOL));
var_dump(HttpQueryString::getInstance()->get('r'));
var_dump(HttpQueryString::getInstance()->get('r', HttpQueryString::TYPE_ARRAY));
var_dump(HttpQueryString::getInstance()->get('r', HttpQueryString::TYPE_OBJECT));

HttpQueryString::getInstance()->set('z', array(2));

HttpQueryString::getInstance()->mod(array('a'=>'b', 'c'=> "3.4"));
HttpQueryString::getInstance()->del('a');

var_dump(HttpQueryString::getInstance());
var_dump($_GET);
var_dump($_SERVER['QUERY_STRING']);

echo "Done\n";
?>
--EXPECTF--
%sTEST
string(30) "a=b&c=3.4&r[0]=1&r[1]=2&r[2]=3"
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
  &array(3) {
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
      int(2)
    }
  }
  ["queryString:private"]=>
  &string(33) "c=3.4&r[0]=1&r[1]=2&r[2]=3&z[0]=2"
}
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
    int(2)
  }
}
string(33) "c=3.4&r[0]=1&r[1]=2&r[2]=3&z[0]=2"
Done