--TEST--
factory
--SKIPIF--
<?php
include "skipif.inc";
in_array("curl", http\Client\Factory::getAvailableDrivers()) or die ("skip CURL support missing");
?>
--FILE--
<?php
echo "Test\n";

class MyClient extends http\Curl\Client {}
class MyPool extends http\Curl\Client\Pool {}
class MyShare extends http\Curl\Client\DataShare {}
  
class MyFactory extends http\Client\Factory {
	protected $driver = "curl";
	protected $persistentHandleId = "My";
	protected $clientClass = "MyClient";
	protected $clientPoolClass = "MyPool";
	protected $clientDataShareClass = "MyShare";
	
	protected $dummy = "foo";
}

$f = new MyFactory(array("driver" => "curl"));
$r = $f->createClient();
$p = $f->createPool();
$s = $f->createDataShare();

$r->setRequest(new http\Client\Request("GET", "http://localhost/"));
$x = $f->createPool($r);
$y = $f->createDatashare($r);

var_dump(
	array_map("get_class", array($f,$r,$p,$s,$x,$y)), 
	$f->getDriver()
);

foreach (array("Client", "Pool", "DataShare") as $type) {
	try {
		$f = new http\Client\Factory(array("driver" => "nonexistant"));
		var_dump($f->{"create$type"}());
	} catch (Exception $e) {
		echo $e->getMessage(), "\n";
	}
}

echo "Done\n";
?>
--EXPECTF--
Test
array(6) {
  [0]=>
  string(9) "MyFactory"
  [1]=>
  string(8) "MyClient"
  [2]=>
  string(6) "MyPool"
  [3]=>
  string(7) "MyShare"
  [4]=>
  string(6) "MyPool"
  [5]=>
  string(7) "MyShare"
}
string(4) "curl"
clients are not supported by this driver
pools are not supported by this driver
datashares are not supported by this driver
Done
