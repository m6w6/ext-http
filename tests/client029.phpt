--TEST--
client curl user handler
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
_ext("event");
?> 
--FILE--
<?php 
echo "Test\n";

class UserHandler implements http\Client\Curl\User
{
	private $evbase;
	private $client;
	private $run;
	private $ios = [];
	private $timeout;


	function __construct(http\Client $client, EventBase $evbase) {
		$this->evbase = $evbase;
		$this->client = $client;
	}
	
	function init($run) {
		$this->run = $run;
	}
	
	function timer(int $timeout_ms) {
		echo "T";
		if (isset($this->timeout)) {
			$this->timeout->add($timeout_ms/1000);
		} else {
			$this->timeout = Event::timer($this->evbase, function() {
				if (!call_user_func($this->run, $this->client)) {
					if ($this->timeout) {
						$this->timeout->del();
						$this->timeout = null;
					}
				}
			});
			$this->timeout->add($timeout_ms/1000);
		}
	}
	
	function socket($socket, int $action) {
		echo "S";
		
		switch ($action) {
		case self::POLL_NONE:
			break;
		case self::POLL_REMOVE:
			if (isset($this->ios[(int) $socket])) {
				echo "U";
				$this->ios[(int) $socket]->del();
				unset($this->ios[(int) $socket]);
			}
			break;
			
		default:
			$ev = 0;
			if ($action & self::POLL_IN) {
				$ev |= Event::READ;
			}
			if ($action & self::POLL_OUT) {
				$ev |= Event::WRITE;
			}
			if (isset($this->ios[(int) $socket])) {
				$this->ios[(int) $socket]->set($this->evbase, 
						$socket, $ev, $this->onEvent($socket));
			} else {
				$this->ios[(int) $socket] = new Event($this->evbase, 
						$socket, $ev, $this->onEvent($socket));
			}
			break;
		}
	}
	
	function onEvent($socket) {
		return function($watcher, $events) use($socket) {
			$action = 0;
			if ($events & Ev::READ) {
				$action |= self::POLL_IN;
			}
			if ($events & Ev::WRITE) {
				$action |= self::POLL_OUT;
			}
			if (!call_user_func($this->run, $this->client, $socket, $action)) {
				if ($this->timeout) {
					$this->timeout->del();
					$this->timeout = null;
				}
			}
		};
	}
	
	function once() {
		throw new BadMethodCallException("this test uses EventBase::loop()");
	}
	
	function wait(int $timeout_ms = null) {
		throw new BadMethodCallException("this test uses EventBase::loop()");
	}
	
	function send() {
		throw new BadMethodCallException("this test uses EventBase::loop()");
	}
}


include "helper/server.inc";

server("proxy.inc", function($port) {
	$evbase = new EventBase;
	$client = new http\Client;
	$client->configure([
			"use_eventloop" => new UserHandler($client, $evbase)
	]);
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/"), function($r) {
		var_dump($r->getResponseCode());
	});
	$evbase->loop();
});

?>
===DONE===
--EXPECTREGEX--
Test
T*[ST]+U+T*int\(200\)
===DONE===
