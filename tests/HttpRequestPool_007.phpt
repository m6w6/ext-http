--TEST--
HttpRequestPool chain with libevent
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequest');
checkcls('HttpRequestPool');
$pool = new RequestPool;
skipif(!@$pool->enableEvents(), "need libevent support");
?>
--FILE--
<?php

echo "-TEST\n";

set_time_limit(0);
ini_set('error_reporting', E_ALL);
ini_set('html_errors', 0);

class Request extends HttpRequest
{
	public $pool;
	
	function onFinish()
	{
		$this->pool->detach($this);
		
		$u = $this->getUrl();
		$c = $this->getResponseCode();
		$b = $this->getResponseBody();
		
		printf("%d %s %d\n", $c, $u, strlen($b));
		
		if ($c == 200 && $this->pool->dir) {
			file_put_contents($this->pool->all[$u], $b);
		}
		
		if ($a = each($this->pool->rem)) {
			list($url, $file) = $a;
			$r = new Request(
				$url,
				HttpRequest::METH_GET,
				array(
					'redirect'	=> 5,
					'compress'	=> GZIP,
					'timeout'	=> TOUT,
					'connecttimeout' => TOUT,
					'lastmodified' => is_file($file)?filemtime($file):0
				)
			);
			$r->pool = $this->pool;
			$this->pool->attach($r);
		}
	}
	
}

class Pool extends HttpRequestPool
{
	public $all;
	public $rem;
	public $dir;
	
	public final function __construct($urls_file = 'urls.txt', $cache_dir = 'HttpRequestPool_cache')
	{
		$this->dir = (is_dir($cache_dir) or @mkdir($cache_dir)) ? $cache_dir : null;
		
		$urls = file($urls_file);
		shuffle($urls);
		foreach (array_map('trim', $urls) as $url) {
			$this->all[$url] = $this->dir ? $this->dir .'/'. md5($url) : null;
		}
		
		if (RMAX) {
			$now = array_slice($this->all, 0, RMAX);
			$this->rem = array_slice($this->all, RMAX);
		} else {
			$now = $urls;
			$this->rem = array();
		}
		
		$this->enableEvents();
		
		foreach ($now as $url => $file) {
			$r = new Request(
				$url,
				HttpRequest::METH_GET,
				array(
					'redirect'	=> 5,
					'compress'  => GZIP,
					'timeout'	=> TOUT,
					'connecttimeout' => TOUT,
					'lastmodified' => is_file($file)?filemtime($file):0
				)
			);
			$r->pool = $this;
			$this->attach($r);
		}
		
		$this->send();
	}
}

define('GZIP', true);
define('TOUT', 50);
define('RMAX', 10);
chdir(dirname(__FILE__));

$time = microtime(true);
$pool = new Pool();
printf("Elapsed: %0.3fs\n", microtime(true)-$time);

echo "Done\n";
?>
--EXPECTF--
%sTEST
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
%d %s %d
Elapsed: %fs
Done
