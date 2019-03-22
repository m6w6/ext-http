<?php 

include "server.inc";

serve(function($client) {
	$request = new http\Message($client, false);
	$response = new http\Env\Response;
	$old_cookies = new http\Cookie($request->getHeader("Cookie"));
	$new_cookies = new http\Cookie;
	$new_cookies->setCookie("temp", $old_cookies->getCookie("temp") ?: microtime(true));
	$response->setCookie($new_cookies);
	$new_cookies = new http\Cookie;
	$new_cookies->setCookie("perm", $old_cookies->getCookie("perm") ?: microtime(true));
	$new_cookies->setExpires(time()+3600);
	$response->setCookie($new_cookies);
	$response->send($client);
});
