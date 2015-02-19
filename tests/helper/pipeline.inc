<?php 

include "server.inc";

function respond($client, $msg) {
	$r = new http\Env\Response;
	$r->setEnvRequest($msg)
		->setHeader("X-Req", $msg->getRequestUrl())
		->send($client);
}

serve(function($client) {
	$count = trim(fgets(STDIN));
	
	/* the peek message */
	respond($client, new http\Message($client, false));
	
	/* pipelined messages */
	$req = array();
	for ($i=0; $i < $count; ++ $i) {
		$req[] = new http\Message($client, false);
	}
	foreach ($req as $msg) {
		respond($client, $msg);
	}
});
