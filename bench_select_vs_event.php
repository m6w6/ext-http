<?php

use http\request\Pool as HttpRequestPool;
use http\Request as HttpRequest;

$factory = new http\request\Factory("curl", array("requestPoolClass" => "pool", "requestClass" => "request"));

class pool extends HttpRequestPool {
	private $url;
	private $cnt;

    private $factory;

    static function fetch($factory, $url, $n, $c, $e, $p) {
        $pool = $factory->createPool();
        $pool->factory = $factory;
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
            $this->factory->createRequest()->init($this, $this->url)->id = $this->cnt--;
		}
	}
}

class request extends HttpRequest implements SplObserver {
	static $counter = 0;
	
	public $id;
	private $pool;
	
	function init(pool $pool, $url) {
		$this->setUrl($url);
		$this->pool = $pool;
        $this->attach($this);
        $pool->attach($this);
		return $this;
	}
	
    function update(SplSubject $r) {
        if ($r->getProgress()->finished) {
            ++self::$counter;
            $this->pool->detach($this);
            $this->detach($this);
            $this->pool->push();
        }
	}
}

function usage($e = null) {
    global $argv;
    if ($e) {
        fprintf(STDERR, "ERROR: %s\n\n", $e);
    }
	fprintf(STDERR, "Usage: %s -u <URL> -n <requests> -c <concurrency> [-p (enable pipelining)] [-e (use libevent)]\n", $argv[0]);
	fprintf(STDERR, "\nDefaults: -u http://localhost/ -n 1000 -c 10\n\n");
	exit(-1);
}

isset($argv) or $argv = $_SERVER['argv'];
defined('STDERR') or define('STDERR', fopen('php://stderr', 'w'));

$opts = getopt("u:c:n:e");
isset($opts["u"]) or $opts["u"] = "http://localhost/";
isset($opts["c"]) or $opts["c"] = 10;
isset($opts["n"]) or $opts["n"] = 1000;

try {
    ($c=$factory->createRequest($opts["u"])->send()->getResponseCode()) == 200 or usage("Received response code $c");
} catch (Exception $ex) {
    usage($ex->getMessage());
}

$argc > 1 or usage();

$time = microtime(true);
pool::fetch($factory, $opts["u"], $opts["n"], $opts["c"], isset($opts["e"]), isset($opts["p"]));
printf("\n> %10.6fs (%3.2fM)\n", microtime(true)-$time, memory_get_peak_usage(true)/1024/1024);

request::$counter == $opts["n"] or printf("\nOnly %d finished\n", request::$counter);
