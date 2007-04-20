--TEST--
HttpRequest cookie API
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
checkcls("HttpRequest");
?>
--FILE--
<?php
echo "-TEST\n";

$r = new HttpRequest("http://dev.iworks.at/ext-http/.cookie.php");

$r->send();
$c[0] = $r->getResponseInfo("cookies");
if (!empty($c[0])) {
	var_dump('$c[0]', $c[0]);
}

var_dump($r->enableCookies());
$r->send();

$c[1] = $r->getResponseInfo("cookies");
if (empty($c[1])) {
	var_dump('$c[1]', $c[1]);
}

var_dump($r->resetCookies());
$r->send();

$c[2] = $r->getResponseInfo("cookies");
if ($c[1] === $c[2]) {
	var_dump('$c[1]', $c[1], '$c[2]', $c[2]);
}

$r->send();
$c[3] = $r->getResponseInfo("cookies");
if ($c[2] !== $c[3]) {
	var_dump('$c[2]', $c[2], '$c[3]', $c[3]);
}

echo "Done\n";
--EXPECTF--
%sTEST
bool(true)
bool(true)
Done
