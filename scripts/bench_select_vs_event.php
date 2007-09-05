<?php

class pool extends HttpRequestPool {
	private $url;
	private $cnt;
	
	static function fetch($url, $n, $c, $e) {
		$pool = new pool;
		$pool->url = $url;
		$pool->cnt = $n;
		
		$pool->enableEvents($e);
		
		for ($i = 0; $i < $c; ++$i) {
			$pool->push();
		}
		try {
			$pool->send();
		} catch (Exception $ex) {
			echo $ex, "\n";
		}
	}
	
	function push() {
		if ($this->cnt > 0) {
			request::init($this, $this->url)->id = $this->cnt--;
		}
	}
}

class request extends HttpRequest {
	static $counter = 0;
	
	public $id;
	private $pool;
	
	static function init(pool $pool, $url) {
		$r = new request($url);
		$r->pool = $pool;
		$pool->attach($r);
		return $r;
	}
	
	function onFinish() {
		++self::$counter;
		$this->pool->detach($this);
		$this->pool->push();
	}
}

function usage() {
	global $argv;
	fprintf(STDERR, "Usage: %s -u <URL> -n <requests> -c <concurrency> -e (use libevent)\n", $argv[0]);
	fprintf(STDERR, "\nDefaults: -u http://localhost/ -n 1000 -c 10\n\n");
	exit(-1);
}

isset($argv) or $argv = $_SERVER['argv'];
defined('STDERR') or define('STDERR', fopen('php://stderr', 'w'));

$opts = getopt("u:c:n:e");
isset($opts["u"]) or $opts["u"] = "http://localhost/";
isset($opts["c"]) or $opts["c"] = 10;
isset($opts["n"]) or $opts["n"] = 1000;

http_parse_message(http_head($opts["u"]))->responseCode == 200 or usage();

$time = microtime(true);
pool::fetch($opts["u"], $opts["n"], $opts["c"], isset($opts["e"]));
printf("\n> %10.6fs\n", microtime(true)-$time);

request::$counter == $opts["n"] or printf("\nOnly %d finished\n", request::$counter);
