--TEST--
client curl user handler
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
_ext("ev");

?> 
--XFAIL--
ext-ev leaks
--FILE--
<?php 
echo "Test\n";

class UserHandler implements http\Client\Curl\User
{
	private $client;
	private $run;
	private $ios = [];
	private $timeout;


	function __construct(http\Client $client) {
		$this->client = $client;
	}
	
	function init($run) {
		$this->run = $run;
	}
	
	function timer(int $timeout_ms) {
		echo "T";
		if (isset($this->timeout)) {
			$this->timeout->set($timeout_ms/1000, 0);
			$this->timeout->start();
		} else {
			$this->timeout = new EvTimer($timeout_ms/1000, 0, function() {
				if (!call_user_func($this->run, $this->client)) {
					if ($this->timeout) {
						$this->timeout->stop();
						$this->timeout = null;
					}
				}
			});
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
				$this->ios[(int) $socket]->stop();
				unset($this->ios[(int) $socket]);
			}
			break;
			
		default:
			$ev = 0;
			if ($action & self::POLL_IN) {
				$ev |= Ev::READ;
			}
			if ($action & self::POLL_OUT) {
				$ev |= Ev::WRITE;
			}
			if (isset($this->ios[(int) $socket])) {
				$this->ios[(int) $socket]->set($socket, $ev);
			} else {
				$this->ios[(int) $socket] = new EvIo($socket, $ev, function($watcher, $events) use($socket) {
					$action = 0;
					if ($events & Ev::READ) {
						$action |= self::POLL_IN;
					}
					if ($events & Ev::WRITE) {
						$action |= self::POLL_OUT;
					}
					if (!call_user_func($this->run, $this->client, $socket, $action)) {
						if ($this->timeout) {
							$this->timeout->stop();
							$this->timeout = null;
						}
					}
				});
			}
			break;
		}
	}
	
	function once() {
		throw new BadMethodCallException("this test uses Ev::run()");
	}
	
	function wait(int $timeout_ms = null) {
		throw new BadMethodCallException("this test uses Ev::run()");
	}
	
	function send() {
		throw new BadMethodCallException("this test uses Ev::run()");
	}
}


include "helper/server.inc";

server("proxy.inc", function($port) {
	$client = new http\Client;
	$client->configure([
			"use_eventloop" => new UserHandler($client)
	]);
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/"), function($r) {
		var_dump($r->getResponseCode());
	});
	Ev::run();
});

?>
===DONE===
--EXPECTREGEX--
Test
T[ST]+U+int\(200\)
===DONE===
