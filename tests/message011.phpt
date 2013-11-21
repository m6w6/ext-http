--TEST--
message headers
--SKIPIF--
<?php
include "skipif.inc";
?>
--INI--
date.timezone=UTC
--FILE--
<?php

echo "Test\n";

class strval {
	private $str;
	function __construct($str) {
		$this->str = $str;
	}
	function __toString() {
		return (string) $this->str;
	}
}

$m = new http\Message;
$m->addHeaders(array("foo"=>"bar","bar"=>"foo"));
var_dump(array("Foo"=>"bar", "Bar"=>"foo") === $m->getHeaders());
$m->addHeaders(array("key"=>"val","more"=>"Stuff"));
var_dump(array("Foo"=>"bar", "Bar"=>"foo","Key"=>"val","More"=>"Stuff") === $m->getHeaders());
$m = new http\Message("GET / HTTP/1.1");
$m->addHeader("Accept", "text/html");
$m->addHeader("Accept", "text/xml;q=0");
$m->addHeader("Accept", "text/plain;q=0.5");
var_dump(
		"GET / HTTP/1.1\r\n".
		"Accept: text/html, text/xml;q=0, text/plain;q=0.5\r\n" ===
		$m->toString()
);
$m = new http\Message("HTTP/1.1 200 Ok");
$m->addHeader("Bool", true);
$m->addHeader("Int", 123);
$m->addHeader("Float", 1.23);
$m->addHeader("Array", array(1,2,3));
$m->addHeader("Object", new strval("test"));
$m->addHeader("Set-Cookie",
		array(
				array(
						"cookies" => array("foo" => "bar"),
						"expires" => date_create("2012-12-31 22:59:59 GMT")->format(
								DateTime::COOKIE
						),
						"path" => "/somewhere"
				)
		)
);
$m->addHeader("Set-Cookie", "val=0");

var_dump(
		"HTTP/1.1 200 Ok\r\n".
		"Bool: true\r\n".
		"Int: 123\r\n".
		"Float: 1.23\r\n".
		"Array: 1, 2, 3\r\n".
		"Object: test\r\n".
		"Set-Cookie: foo=bar; path=/somewhere; expires=Mon, 31 Dec 2012 22:59:59 GMT; \r\n".
		"Set-Cookie: val=0\r\n" ===
		$m->toString()
);

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
bool(true)
Done
