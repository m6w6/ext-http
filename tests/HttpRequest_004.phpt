--TEST--
HttpRequest multiple posts
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";

$fields = array(
	array('int' => 1, 'dbl' => M_PI),
	array('str' => 'something', 'nil' => null)
);

echo "\nFirst Request\n";
$r = new HttpRequest('http://dev.iworks.at/ext-http/.print_request.php', HttpRequest::METH_POST);
$r->setPostFields($fields[0]);
$r->addPostFields($fields[1]);
var_dump($r->send()->getBody());
var_dump($fields);

echo "\nSecond Request\n";
$r->setPostFields($fields);
var_dump($r->send()->getBody());
var_dump($fields);

echo "\nThird Request\n";
$r->addPostFields(array('x' => 'X'));
var_dump($r->send()->getBody());
var_dump($fields);

echo "\nFourth Request\n";
$r->setPostFields(array());
var_dump($r->send()->getBody());
var_dump($fields);

echo "Done\n";
?>
--EXPECTF--
%sTEST

First Request
string(149) "Array
(
    [int] => 1
    [dbl] => 3.1415926535898
    [str] => something
    [nil] => 
)
string(44) "int=1&dbl=3.1415926535898&str=something&nil="
"
array(2) {
  [0]=>
  array(2) {
    ["int"]=>
    int(1)
    ["dbl"]=>
    float(3.1415926535898)
  }
  [1]=>
  array(2) {
    ["str"]=>
    string(9) "something"
    ["nil"]=>
    NULL
  }
}

Second Request
string(285) "Array
(
    [0] => Array
        (
            [int] => 1
            [dbl] => 3.1415926535898
        )

    [1] => Array
        (
            [str] => something
            [nil] => 
        )

)
string(72) "0%5Bint%5D=1&0%5Bdbl%5D=3.1415926535898&1%5Bstr%5D=something&1%5Bnil%5D="
"
array(2) {
  [0]=>
  array(2) {
    ["int"]=>
    int(1)
    ["dbl"]=>
    float(3.1415926535898)
  }
  [1]=>
  array(2) {
    ["str"]=>
    string(9) "something"
    ["nil"]=>
    NULL
  }
}

Third Request
string(302) "Array
(
    [0] => Array
        (
            [int] => 1
            [dbl] => 3.1415926535898
        )

    [1] => Array
        (
            [str] => something
            [nil] => 
        )

    [x] => X
)
string(76) "0%5Bint%5D=1&0%5Bdbl%5D=3.1415926535898&1%5Bstr%5D=something&1%5Bnil%5D=&x=X"
"
array(2) {
  [0]=>
  array(2) {
    ["int"]=>
    int(1)
    ["dbl"]=>
    float(3.1415926535898)
  }
  [1]=>
  array(2) {
    ["str"]=>
    string(9) "something"
    ["nil"]=>
    NULL
  }
}

Fourth Request
string(13) "string(0) ""
"
array(2) {
  [0]=>
  array(2) {
    ["int"]=>
    int(1)
    ["dbl"]=>
    float(3.1415926535898)
  }
  [1]=>
  array(2) {
    ["str"]=>
    string(9) "something"
    ["nil"]=>
    NULL
  }
}
Done

