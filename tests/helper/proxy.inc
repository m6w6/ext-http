<?php 

include "server.inc";

serve(function($client) {
	/* this might be a proxy connect or a standard request */
	$request = new http\Message($client, false);
	
	if ($request->getHeader("Proxy-Connection")) {
		$response = new http\Env\Response;
		$response->send($client);
		
		/* soak up the request following the connect */
		new http\Message($client, false);
	}
	
	/* return the initial message as response body */
	$response = new http\Env\Response;
	$response->getBody()->append($request);
	$response->send($client);
});
