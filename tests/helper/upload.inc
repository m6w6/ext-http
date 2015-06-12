<?php 

include "dump.inc";
include "server.inc";

serve(function($client) {
	$request = new http\Message($client, false);
	
	if ($request->getHeader("Expect") === "100-continue") {
		$response = new http\Env\Response;
		$response->setEnvRequest($request);
		$response->setResponseCode(100);
		$response->send($client);
	}
	
	/* return the initial message as response body */
	$response = new http\Env\Response;
	/* avoid OOM with $response->getBody()->append($request); */
	dump_message($response->getBody()->getResource(), $request);
	$response->send($client);
});
