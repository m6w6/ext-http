<?php

include "dump.inc";
include "server.inc";

serve(function($client) {
	/* this might be a proxy connect or a standard request */
	$request = new http\Message($client, false);

	if ($request->getHeader("Proxy-Connection")) {
		$response = new http\Env\Response;
		$response->setEnvRequest($request);
		$response->send($client);

		/* soak up the request following the connect */
		new http\Message($client, false);
	}

	/* return the initial message as response body */
	$response = new http\Env\Response;
	$response->setHeader("X-Request-Content-Length", $request->getBody()->stat("size"));
	/* avoid OOM with $response->getBody()->append($request); */
	dump_message($response->getBody()->getResource(), $request);
	$response->send($client);
});
