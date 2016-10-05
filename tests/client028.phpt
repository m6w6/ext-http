--TEST--
client curl user handler
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?> 
--FILE--
<?php 
echo "Test\n";

class UserHandler implements http\Client\Curl\User
{
	private $client;
	private $run;
	private $fds = array(
			"R" => array(),
			"W" => array()
	);
	private $R = array();
	private $W = array();
	private $timeout = 1000;
	
	function __construct(http\Client $client) {
		$this->client = $client;
	}
	
	function init($run) {
		$this->run = $run;
	}
	
	function timer(int $timeout_ms) {
		echo "T";
		$this->timeout = $timeout_ms;
	}
	
	function socket($socket, int $action) {
		echo "S";
		
		switch ($action) {
		case self::POLL_NONE:
			break;
		case self::POLL_REMOVE:
			if (false !== ($r = array_search($socket, $this->fds["R"], true))) {
				echo "U";
				unset($this->fds["R"][$r]);
			}
			
			if (false !== ($w = array_search($socket, $this->fds["W"], true))) {
				echo "U";
				unset($this->fds["W"][$w]);
			}
			
			break;
			
		default:
			if ($action & self::POLL_IN) {
				if (!in_array($socket, $this->fds["R"], true)) {
					$this->fds["R"][] = $socket;
				}
			}
			if ($action & self::POLL_OUT) {
				if (!in_array($socket, $this->fds["W"], true)) {
					$this->fds["W"][] = $socket;
				}
			}
			break;
		}
	}
	
	function once() {
		echo "O";
		
		foreach ($this->W as $w) {
			call_user_func($this->run, $this->client, $w, self::POLL_OUT);
		}
		foreach ($this->R as $r) {
			call_user_func($this->run, $this->client, $r, self::POLL_IN);
		}
		return count($this->client);
	}
	
	function wait(int $timeout_ms = null) {
		echo "W";
		
		if ($timeout_ms === null) {
			$timeout_ms = $this->timeout;
		}
		$ts = floor($timeout_ms / 1000);
		$tu = ($timeout_ms % 1000) * 1000;
		
		extract($this->fds);
		
		if (($wfds = count($R) + count($W))) {
			$nfds = stream_select($R, $W, $E, $ts, $tu);
		} else {
			$nfds = 0;
		}
		$this->R = (array) $R;
		$this->W = (array) $W;
		
		if ($nfds === false) {
			return false;
		}
		if (!$nfds) {
			if (!$wfds) {
				echo "S";
				time_nanosleep($ts, $tu*1000);
			}
			call_user_func($this->run, $this->client);
		}
			
		return true;
	}
	
	function send() {
		$this->wait();
		$this->once();
	}
}


include "helper/server.inc";

server("proxy.inc", function($port) {
	$client = new http\Client;
	$client->configure(array(
			"use_eventloop" => new UserHandler($client)
	));
	$client->enqueue(new http\Client\Request("GET", "http://localhost:$port/"), function($r) {
		var_dump($r->getResponseCode());
	});
	$client->send();
});

?>
===DONE===
--EXPECTREGEX--
Test
[ST][WST]*([OWST]*)+U+int\(200\)
===DONE===
