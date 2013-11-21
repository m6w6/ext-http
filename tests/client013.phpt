--TEST--
client observers
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class Client extends http\Client {
	public $pi;
}
class ProgressObserver1 implements SplObserver {
	function update(SplSubject $c, $r = null) {
		if ($c->getProgressInfo($r)) $c->pi .= "-";
	}
}
class ProgressObserver2 implements SplObserver {
	function update(SplSubject $c, $r = null) {
		if ($c->getProgressInfo($r)) $c->pi .= ".";
	}
}
class CallbackObserver implements SplObserver {
	public $callback;
	function __construct($callback) {
		$this->callback = $callback;
	}
	function update(SplSubject $c, $r = null) {
		call_user_func($this->callback, $c, $r);
	}
}

$client = new Client;
$client->attach($o1 = new ProgressObserver1);
$client->attach($o2 = new ProgressObserver2);
$client->attach(
		$o3 = new CallbackObserver(
				function ($c, $r) {
					$p = (array) $c->getProgressInfo($r);
					var_dump(array_key_exists("started", $p));
					var_dump(array_key_exists("finished", $p));
					var_dump(array_key_exists("dlnow", $p));
					var_dump(array_key_exists("ulnow", $p));
					var_dump(array_key_exists("dltotal", $p));
					var_dump(array_key_exists("ultotal", $p));
					var_dump(array_key_exists("info", $p));
				}
		)
);

$client->enqueue(new http\Client\Request("GET", "http://dev.iworks.at/ext-http/"))->send();
var_dump(1 === preg_match("/(\.-)+/", $client->pi));
var_dump(3 === count($client->getObservers()));
$client->detach($o1);
var_dump(2 === count($client->getObservers()));
$client->detach($o2);
var_dump(1 === count($client->getObservers()));
$client->detach($o3);
var_dump(0 === count($client->getObservers()));

?>
Done
--EXPECTREGEX--
Test\n(bool\(true\)\n)+Done
