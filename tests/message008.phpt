--TEST--
message to callback
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$m = new http\Message("HTTP/1.1 200 Ok");
$m->addHeader("Content-Type", "text/plain");
$m->getBody()->append("this\nis\nthe\ntext");

$d = new http\Encoding\Stream\Deflate;
$s = "";
$m->toCallback(function ($m, $data) use ($d, &$s) {
	$s.=$d->update($data);
});
$s.=$d->finish();
var_dump($m->toString() === http\Encoding\Stream\Inflate::decode($s));

?>
Done
--EXPECT--
Test
bool(true)
Done
