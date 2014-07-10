--TEST--
cookies expire
--SKIPIF--
<?php
include "skipif.inc";
?>
--INI--
date.timezone=UTC
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie("this=expires; expires=Tue, 24 Jan 2012 10:35:32 +0100");
var_dump($c->getCookie("this"));
var_dump($c->getExpires());

$o = clone $c;
$t = time();

$o->setExpires();
var_dump(-1 === $o->getExpires());
var_dump(-1 != $c->getExpires());

$o->setExpires($t);
var_dump($t === $o->getExpires());
var_dump($t != $c->getExpires());
var_dump(
	sprintf(
		"this=expires; expires=%s; ", 
		date_create("@$t")
			->setTimezone(new DateTimezone("UTC"))
			->format("D, d M Y H:i:s \\G\\M\\T")
	) === $o->toString()
);

?>
DONE
--EXPECT--
Test
string(7) "expires"
int(1327397732)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
DONE
