--TEST--
factory
--SKIPIF--
<?php
include "skipif.inc";
in_array("curl", http\request\Factory::getAvailableDrivers()) or die ("skip CURL support missing");
?>
--FILE--
<?php
echo "Test\n";

class MyRequest extends http\Request {}
class MyPool extends http\request\Pool {}
class MyShare extends http\request\DataShare {}
  
class MyFactory extends http\request\Factory {
	protected $driver = "curl";
	protected $persistentHandleId = "My";
	protected $requestClass = "MyRequest";
	protected $requestPoolClass = "MyPool";
	protected $requestDataShareClass = "MyShare";
	
	protected $dummy = "foo";
}

$f = new MyFactory(array("driver" => "curl"));
$r = $f->createRequest();
$p = $f->createPool();
$s = $f->createDataShare();

var_dump(array_map("get_class", array($f,$r,$p,$s)));

echo "Done\n";
?>
--EXPECTF--
Test
array(4) {
  [0]=>
  string(9) "MyFactory"
  [1]=>
  string(9) "MyRequest"
  [2]=>
  string(6) "MyPool"
  [3]=>
  string(7) "MyShare"
}
Done
