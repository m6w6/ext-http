<?php 

include "server.inc";

serve(function($client) {
	$request = new http\Message($client, false);
	$cookies = new http\Cookie();
	$cookies->setCookie("foo", "bar");
	$cookies->setCookie("bar", "foo");
	$response = new http\Env\Response;
	$response->setCookie($cookies);
	$response->send($client);
});
