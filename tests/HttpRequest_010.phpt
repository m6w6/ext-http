--TEST--
HttpRequest cookie API
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--FILE--
<?php
echo "-TEST\n";

$r = new HttpRequest("http://dev.iworks.at/.cookie.php");
$r->recordHistory = true;

$r->send();
$c[0] = $r->getResponseInfo("cookies");
var_dump(empty($c[0]));

$r->setOptions(array("cookiesession" => true));

$r->send();
$c[1] = $r->getResponseInfo("cookies");
var_dump(empty($c[1]));

$r->resetCookies();

$r->send();
$c[2] = $r->getResponseInfo("cookies");
var_dump($c[1] === $c[2]);

$r->send();
$c[3] = $r->getResponseInfo("cookies");
var_dump($c[2] === $c[3]);

echo "Done\n";
--EXPECTF--
%sTEST
bool(true)
bool(false)
bool(false)
bool(true)
Done
