<?php

function dump_headers($stream, array $headers) {
	if (!is_resource($stream)) {
		$stream = fopen("php://output", "w");
	}
	ksort($headers);
	foreach ($headers as $key => $val) {
		fprintf($stream, "%s: %s\n", $key, $val);
	}
	fprintf($stream, "\n");
}

function dump_message($stream, http\Message $msg, $parent = false) {
	if (!is_resource($stream)) {
		$stream = fopen("php://output", "w");
	}
	fprintf($stream, "%s\n", $msg->getInfo());
	dump_headers($stream, $msg->getHeaders());
	$msg->getBody()->toStream($stream);

	if ($parent && ($msg = $msg->getParentMessage())) {
		dump_message($stream, $msg, true);
	}
}

function dump_responses($client, array $expect_cookie = []) {
	while (($r = $client->getResponse())) {
		dump_headers(null, $r->getHeaders());
		if ($expect_cookie) {
			$got_cookies = array_merge(...array_map(function($c) {
				return $c->getCookies();
			}, $r->getCookies()));
			if ($expect_cookie != $got_cookies) {
				var_dump($expect_cookie, $got_cookies);
				echo $r->toString(true);
			}
		}
	}

}
?>
