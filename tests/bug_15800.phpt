--TEST--
Bug #15800 Double free when zval is separated in convert_to_*
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
checkcls('HttpRequest');
skipif(!function_exists('debug_zval_dump'), "need DEBUG version of PHP");
?>
--FILE--
<?php
echo "-TEST\n";
$o = array('ssl' => array('verifypeer'=>'1'));
debug_zval_dump($o);

$r = new HttpRequest('http://www.google.com');
$r->setOptions($o);
$r->send();
debug_zval_dump($o);

unset($r);
debug_zval_dump($o);

echo "Done\n";
?>
--EXPECTF--
%aTEST
array(1) refcount(2){
  ["ssl"]=>
  array(1) refcount(1){
    ["verifypeer"]=>
    string(1) "1" refcount(1)
  }
}
array(1) refcount(2){
  ["ssl"]=>
  array(1) refcount(1){
    ["verifypeer"]=>
    string(1) "1" refcount(2)
  }
}
array(1) refcount(2){
  ["ssl"]=>
  array(1) refcount(1){
    ["verifypeer"]=>
    string(1) "1" refcount(1)
  }
}
Done
