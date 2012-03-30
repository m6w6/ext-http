--TEST--
HttpRequestPool chain
--SKIPIF--
<?php
include 'skipif.inc';
?>
--FILE--
<?php

echo "-TEST\n";

set_time_limit(0);
ini_set('error_reporting', E_ALL);
ini_set('html_errors', 0);

class Pool extends \http\Curl\Client\Pool
{
	private $all;
	private $rem;
	private $dir;
	
	private $factory;
	
	public final function run($factory, $urls_file = 'data/urls.txt', $cache_dir = 'HttpRequestPool_cache')
	{
		$this->factory = $factory;
		$this->dir = (is_dir($cache_dir) or @mkdir($cache_dir)) ? $cache_dir : null;
		
		foreach (array_map('trim', file($urls_file)) as $url) {
			$this->all[$url] = $this->dir ? $this->dir .'/'. md5($url) : null;
		}
		
		$this->send();
	}
	
	public final function send()
	{
		if (RMAX) {
			$now = array_slice($this->all, 0, RMAX);
			$this->rem = array_slice($this->all, RMAX);
		} else {
			$now = $urls;
			$this->rem = array();
		}
		
		foreach ($now as $url => $file) {
			$this->attach(
				$this->factory->createClient(
					array(
						'redirect'	=> 5,
						'compress'  => GZIP,
						'timeout'	=> TOUT,
						'connecttimeout' => TOUT,
						'lastmodified' => is_file($file)?filemtime($file):0
					)
				)->setRequest(new http\Client\Request("GET", $url))
			);
		}
		
		while ($this->once()) {
			if (!$this->wait()) {
				throw new http\Exception;
			}
		}
	}
	
	protected final function once()
	{
		try {
			$rc = parent::once();
		} catch (http\Exception $x) {
			// a request may have thrown an exception,
			// but it is still save to continue
			echo $x->getMessage(), "\n";
		}
		
		foreach ($this->getFinished() as $r) {
			$this->detach($r);
			
			$u = $r->getRequest()->getRequestUrl();
			$c = $r->getResponseMessage()->getResponseCode();
            try {
    			$b = $r->getResponseMessage()->getBody();
            } catch (\Exception $e) {
                echo $e->getMessage(), "\n";
                $b = "";
            }
			
			printf("%d %s %d\n", $c, $u, strlen($b));
			
			if ($c == 200 && $this->dir) {
				file_put_contents($this->all[$u], $b);
			}
			
			if ($a = each($this->rem)) {
				list($url, $file) = $a;
				$this->attach(
					$this->factory->createClient(
						array(
							'redirect'	=> 5,
							'compress'	=> GZIP,
							'timeout'	=> TOUT,
							'connecttimeout' => TOUT,
							'lastmodified' => is_file($file)?filemtime($file):0
						)
					)->setRequest(new http\Client\Request("GET", $url))
				);
			}
		}
		return $rc;
	}
}

define('GZIP', true);
define('TOUT', 300);
define('RMAX', 10);
chdir(__DIR__);

$time = microtime(true);
$factory = new http\Client\Factory(array("driver" => "curl", "clientPoolClass" => "Pool"));
$factory->createPool()->run($factory);
printf("Elapsed: %0.3fs\n", microtime(true)-$time);

echo "Done\n";
?>
--EXPECTF--
%aTEST
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
