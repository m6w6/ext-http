--TEST--
cookies expire as date
--SKIPIF--
<?php
include "skipif.inc";
?>
--INI--
date.timezone=UTC
--FILE--
<?php
echo "Test\n";

$d = new DateTime;
$c = new http\Cookie(array("expires" => $d->format(DateTime::RFC1123)));
var_dump($d->format("U") == $c->getExpires());

?>
DONE
--EXPECT--
Test
bool(true)
DONE
