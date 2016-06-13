<?php 

include "server.inc";

function respond($client, $msg) {
	$r = new http\Env\Response;
	$r->setEnvRequest($msg)
		->setHeader("X-Req", $msg->getRequestUrl())
		->send($client);
}

serve(function($client) {
	$R = array(STDIN); $W = $E = array();
	if (!stream_select($R, $W, $E, 10, 0)) {
		logger("Client %d timed out", (int) $client);
		return;
	}
	$count = trim(fgets(STDIN));
	logger("Expecting %d messages from client %d", $count, (int) $client);
	/* the peek message */
	respond($client, new http\Message($client, false));
	logger("Handled the peek request of client %d", (int) $client);
	/* pipelined messages */
	$req = array();
	for ($i=0; $i < $count; ++ $i) {
		$req[] = $m = new http\Message($client, false);
		logger("Read request no. %d from client %d (%s)", $i+1, (int) $client, $m->getRequestUrl());
	}
	foreach ($req as $i => $msg) {
		respond($client, $msg);
		logger("Sent response no. %d to client %d", $i+1, (int) $client);
	}
});
