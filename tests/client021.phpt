--TEST--
client cookies
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
if (0 === strpos(http\Client\Curl\Versions\CURL, "7.64.0") ||
	0 === strpos(http\Client\Curl\Versions\CURL, "7.88.1")) {
	die("skip - cookie handling broken or crashes with libcurl v" . http\Client\Curl\Versions\CURL ."\n");
}
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

function dump() {
	global $tmpfile, $section;
	printf("# %s\n", $section);
	foreach (file($tmpfile) as $line) {
		if ($line[0] === "#" || $line === "\n") {
			continue;
		}
		printf("%s:\t%s", $tmpfile, $line);
	}
}

function send_and_check($client, $cmp) {
	global $section, $request;
	$client->requeue($request)->send();
	foreach ($client->getResponse()->getCookies() as $list) {
		foreach ($list->getCookies() as $name => $value) {
			if ($cmp[$name] != $value) {
				printf("# %s\nExpected %s=%s, got %s\n",
					$section, $name, $cmp[$name], $value);
			}
		}
	}
	#dump();
}

$tmpfile = tempnam(sys_get_temp_dir(), "cookie.");
$request = new http\Client\Request("GET", "http://localhost");

$section = "distinct clients";

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	send_and_check($client, ["counter" => 1]);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	send_and_check($client, ["counter" => 1]);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	send_and_check($client, ["counter" => 1]);
});

$section = "reusing curl handles";

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	send_and_check($client, ["counter" => 1]);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	send_and_check($client, ["counter" => 2]);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	send_and_check($client, ["counter" => 3]);
});

$section = "distict client with persistent cookies";

$request->setOptions(array("cookiestore" => $tmpfile));

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	send_and_check($client, ["counter" => 1]);
	send_and_check($client, ["counter" => 2]);
	send_and_check($client, ["counter" => 3]);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	send_and_check($client, ["counter" => 4]);
	send_and_check($client, ["counter" => 5]);
	send_and_check($client, ["counter" => 6]);
});

$section = "distinct client with persistent cookies, but session cookies removed";

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port, "cookiesession" => true));
	$client = new http\Client;
	send_and_check($client, ["counter" => 1]);
	send_and_check($client, ["counter" => 1]);
	send_and_check($client, ["counter" => 1]);
});

$section = "distinct client with persistent cookies, and session cookies kept";

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port, "cookiesession" => false));
	$client = new http\Client;
	send_and_check($client, ["counter" => 2]);
	send_and_check($client, ["counter" => 3]);
	send_and_check($client, ["counter" => 4]);
});

$section = "reusing curl handles without persistent cookies and disabling cookie_share";

$c = new http\Client("curl", "test");
$c->configure(array("share_cookies" => false));
$c = null;
$request->setOptions(array("cookiestore" => null));

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	send_and_check($client, ["counter" => 1]);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	send_and_check($client, ["counter" => 1]);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	send_and_check($client, ["counter" => 1]);
});


unlink($tmpfile);

?>
===DONE===
--EXPECT--
Test
===DONE===
