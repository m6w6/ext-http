--TEST--
message to stream
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

$f = tmpfile();
$m->toStream($f);
rewind($f);
var_dump((string) $m === stream_get_contents($f));
fclose($f);

?>
Done
--EXPECT--
Test
bool(true)
Done
