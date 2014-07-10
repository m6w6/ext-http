--TEST--
cookies empty state
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie;
$o = clone $c;
$a = array(
	"cookies" => array(),
	"extras" => array(),
	"flags" => 0,
	"expires" => -1,
	"path" => "",
	"domain" => "",
	"max-age" => -1,
);
var_dump($a == $c->toArray());
var_dump($a == $o->toArray());

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
