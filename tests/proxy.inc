<?php 

foreach (range(8000, 9000) as $port) {
	if (($server = stream_socket_server("tcp://localhost:$port"))) {
		fprintf(STDERR, "%s\n", $port);
		if (($client = stream_socket_accept($server))) {
			/* this might be a proxy connect or a standard request */
			$request = new http\Message($client, false);
			
			if ($request->getHeader("Proxy-Connection")) {
				$response = new http\Env\Response;
				$response->setHeader("Content-Length", 0);
				$response->send($client);
				
				/* soak up the request following the connect */
				new http\Message($client, false);
			}
			
			/* return the initial message as response body */
			$response = new http\Env\Response;
			$response->getBody()->append($request);
			$response->send($client);
		}
		return;
	}
}