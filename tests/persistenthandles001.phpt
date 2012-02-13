--TEST--
persistent handles
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php
(new http\Request\Factory(array("persistentHandleId" => "foo")))
    ->createRequest("http://dev.iworks.at")
    ->setOptions(array("connecttimeout"=> 90, "timeout" =>300))
    ->send(); 
$r = (new http\Request\Factory(array("persistentHandleId" => "bar")))
    ->createRequest("http://dev.iworks.at")
    ->setOptions(array("connecttimeout"=> 90, "timeout" =>300));
    
var_dump(http\Env::statPersistentHandles()); 
http\Env::cleanPersistentHandles(); 
var_dump(http\Env::statPersistentHandles());

$r->send();

var_dump(http\Env::statPersistentHandles());
?>
DONE
--EXPECTF--
object(stdClass)#%d (3) {
  ["http_request_datashare.curl"]=>
  array(0) {
  }
  ["http_request_pool.curl"]=>
  array(0) {
  }
  ["http_request.curl"]=>
  array(2) {
    ["foo"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(1)
    }
    ["bar"]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(0)
    }
  }
}
object(stdClass)#%d (3) {
  ["http_request_datashare.curl"]=>
  array(0) {
  }
  ["http_request_pool.curl"]=>
  array(0) {
  }
  ["http_request.curl"]=>
  array(1) {
    ["bar"]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(0)
    }
  }
}
object(stdClass)#%d (3) {
  ["http_request_datashare.curl"]=>
  array(0) {
  }
  ["http_request_pool.curl"]=>
  array(0) {
  }
  ["http_request.curl"]=>
  array(1) {
    ["bar"]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(0)
    }
  }
}
DONE
