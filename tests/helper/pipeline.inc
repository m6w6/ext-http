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
	logger("Expecting %d messages", $count);
	/* the peek message */
	respond($client, new http\Message($client, false));
	logger("Handled the peek request");
	/* pipelined messages */
	$req = array();
	for ($i=0; $i < $count; ++ $i) {
		$req[] = new http\Message($client, false);
		logger("Read request no. %d", $i+1);
	}
	foreach ($req as $i => $msg) {
		respond($client, $msg);
		logger("Sent response no. %d", $i+1);
	}
});
