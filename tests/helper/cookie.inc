<?php 

include "server.inc";

serve(function($client) {
	$request = new http\Message($client, false);
	$cookies = new http\Cookie($request->getHeader("cookie"));
	$response = new http\Env\Response;
	$response->setCookie($cookies->setCookie("counter", $cookies->getCookie("counter")+1));
	$response->send($client);
});
