<?php

class pool extends HttpRequestPool {
	private $url;
	private $cnt;
	
	static function fetch($url, $n, $c, $e, $p) {
		$pool = new pool;
		$pool->url = $url;
		$pool->cnt = $n;
		
		$pool->enablePipelining($p);
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
	
	function cnt() {
		return $this->cnt;
	}
}

class request extends HttpRequest {
	static $counter = 0;
	
	public $id;
	private $pool;
	
	static function init(pool $pool, $url) {
		#printf("init %d\n", $pool->cnt());
		$r = new request($url);
		$r->pool = $pool;
		$pool->attach($r);
		return $r;
	}
	
	function onFinish() {
		#printf("left %d\n", count($this->pool));
		#printf("done %d\n", $this->id);
		++self::$counter;
		$this->pool->detach($this);
		$this->pool->push();
	}
}

function usage() {
	global $argv;
	fprintf(STDERR, "Usage: %s -u <URL> -n <requests> -c <concurrency> -e (use libevent) -p (use pipelining)\n", $argv[0]);
	fprintf(STDERR, "\nDefaults: -u http://localhost/ -n 1000 -c 10\n\n");
	exit(-1);
}

isset($argv) or $argv = $_SERVER['argv'];
defined('STDERR') or define('STDERR', fopen('php://stderr', 'w'));

$opts = getopt("u:c:n:ep"); #var_dump($opts);
isset($opts["u"]) or $opts["u"] = "http://localhost/";
isset($opts["c"]) or $opts["c"] = 10;
isset($opts["n"]) or $opts["n"] = 500;

http_parse_message(http_head($opts["u"]))->responseCode == 200 or usage();

$time = microtime(true);
pool::fetch($opts["u"], $opts["n"], $opts["c"], isset($opts["e"]), isset($opts["p"]));
printf("\n> %10.6fs\n", microtime(true)-$time);

printf("\nMemory usage: %s/%s\n", number_format(memory_get_usage()),number_format(memory_get_peak_usage()));

if (request::$counter != $opts["n"]) { 
	printf("\nOnly %d finished\n", request::$counter);
	exit(1);
}
