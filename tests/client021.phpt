--TEST--
client cookies
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

function dump($f) {
	return;
	readfile($f);
}

function cookies($client) {
	foreach ($client->getResponse()->getCookies() as $cookie) {
		echo trim($cookie), "\n";
	}
}

$tmpfile = tempnam(sys_get_temp_dir(), "cookie.");
$request = new http\Client\Request("GET", "http://localhost");

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	cookies($client->requeue($request)->send());
dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	cookies($client->requeue($request)->send());
dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	cookies($client->requeue($request)->send());
dump($tmpfile);
});

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	cookies($client->requeue($request)->send());
dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	cookies($client->requeue($request)->send());
dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	cookies($client->requeue($request)->send());
dump($tmpfile);
});

$request->setOptions(array("cookiestore" => $tmpfile));

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
});

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port, "cookiesession" => true));
	$client = new http\Client;
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
});

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port, "cookiesession" => false));
	$client = new http\Client;
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
	cookies($client->requeue($request)->send());
dump($tmpfile);
});


$c = new http\Client("curl", "test");
$c->configure(array("share_cookies" => false));
$c = null;
$request->setOptions(array("cookiestore" => null));

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	cookies($client->requeue($request)->send());
dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	cookies($client->requeue($request)->send());
dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client("curl", "test");
	cookies($client->requeue($request)->send());
dump($tmpfile);
});


unlink($tmpfile);

?>
===DONE===
--EXPECT--
Test
counter=1;
counter=1;
counter=1;
counter=1;
counter=2;
counter=3;
counter=1;
counter=2;
counter=3;
counter=4;
counter=5;
counter=6;
counter=1;
counter=1;
counter=1;
counter=2;
counter=3;
counter=4;
counter=1;
counter=1;
counter=1;
===DONE===
