--TEST--
persistent handles
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php
(new http\Client\Factory(array("persistentHandleId" => "foo")))
    ->createClient()->setRequest(new http\Client\Request("GET", "http://dev.iworks.at"))
    ->setOptions(array("connecttimeout"=> 90, "timeout" =>300))
    ->send(null); 
$r = (new http\Client\Factory(array("persistentHandleId" => "bar")))
    ->createClient()->setRequest(new http\Client\Request("GET", "http://dev.iworks.at"))
    ->setOptions(array("connecttimeout"=> 90, "timeout" =>300));
    
var_dump(http\Env::statPersistentHandles()); 
http\Env::cleanPersistentHandles(); 
var_dump(http\Env::statPersistentHandles());

$r->send(null);

var_dump(http\Env::statPersistentHandles());
?>
DONE
--EXPECTF--
object(stdClass)#%d (3) {
  ["http_client.curl"]=>
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
  ["http_client_pool.curl"]=>
  array(0) {
  }
  ["http_client_datashare.curl"]=>
  array(0) {
  }
}
object(stdClass)#%d (3) {
  ["http_client.curl"]=>
  array(1) {
    ["bar"]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(0)
    }
  }
  ["http_client_pool.curl"]=>
  array(0) {
  }
  ["http_client_datashare.curl"]=>
  array(0) {
  }
}
object(stdClass)#%d (3) {
  ["http_client.curl"]=>
  array(1) {
    ["bar"]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(0)
    }
  }
  ["http_client_pool.curl"]=>
  array(0) {
  }
  ["http_client_datashare.curl"]=>
  array(0) {
  }
}
DONE
