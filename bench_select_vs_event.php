<?php

function usage($e = null) {
    global $argv;
    if ($e) {
        fprintf(STDERR, "ERROR: %s\n\n", $e);
    }
	fprintf(STDERR, "Usage: %s -u <URL> -n <requests> -c <concurrency> [-p (enable pipelining)] [-e (use libevent)]\n", $argv[0]);
	fprintf(STDERR, "\nDefaults: -u http://localhost/ -n 1000 -c 10\n\n");
	exit(-1);
}

function push($client, $url, &$n) {
	if ($n-- > 0) {
		$req = new http\Client\Request("GET", $url);
		$client->enqueue($req, function($response) use ($client, $req, $url, &$n) {
			global $count; ++$count;
			push($client, $url, $n);
			return true; // dequeue
		});
	}
}

isset($argv) or $argv = $_SERVER['argv'];
defined('STDERR') or define('STDERR', fopen('php://stderr', 'w'));

$opts = getopt("u:c:n:e");
isset($opts["u"]) or $opts["u"] = "http://localhost/";
isset($opts["c"]) or $opts["c"] = 10;
isset($opts["n"]) or $opts["n"] = 1000;

$argc > 1 or usage();
var_Dump($opts);
$time = microtime(true);
$count = 0;
$client = new http\Client;

$client->enablePipelining($opts["p"]===false);
$client->enableEvents($opts["e"]===false);

for ($i = 0, $x = $opts["n"]; $i < $opts["c"]; ++$i) {
	push($client, $opts["u"], $x);
}

try {
	$client->send();
} catch (Exception $e) {
	echo $e;
}

printf("\n> %10.6fs (%3.2fM)\n", microtime(true)-$time, memory_get_peak_usage(true)/1024/1024);

$count == $opts["n"] or printf("\nOnly %d finished\n", $count);
