--TEST--
message properties
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

class message extends http\Message {
	function __call($m, $args) {
		if (preg_match("/^test(get|set)(.*)\$/i", $m, $match)) {
			list(, $getset, $prop) = $match;
			$prop = strtolower($prop[0]).substr($prop,1);
			switch(strtolower($getset)) {
				case "get":
					return $this->$prop;
				case "set":
					$this->$prop = current($args);
					break;
			}
		}
	}
}

$test = new message;
var_dump(0 === $test->testGetType());
var_dump(null === $test->testGetBody());
var_dump(null === $test->testGetRequestMethod());
var_dump(null === $test->testGetRequestUrl());
var_dump(null === $test->testGetResponseStatus());
var_dump(null === $test->testGetResponseCode());
var_dump("1.1" === $test->testGetHttpVersion());
var_dump(array() === $test->testGetHeaders());
var_dump(null === $test->testGetParentMessage());
$test->testSetType(http\Message::TYPE_REQUEST);
var_dump(http\Message::TYPE_REQUEST === $test->testGetType());
var_dump(http\Message::TYPE_REQUEST === $test->getType());
$body = new http\Message\Body;
$test->testSetBody($body);
var_dump($body === $test->testGetBody());
var_dump($body === $test->getBody());
$file = fopen(__FILE__,"r");
$test->testSetBody($file);
var_dump($file === $test->testGetBody()->getResource());
var_dump($file === $test->getBody()->getResource());
$test->testSetBody("data");
var_dump("data" === (string) $test->testGetBody());
var_dump("data" === (string) $test->getBody());
$test->testSetRequestMethod("HEAD");
var_dump("HEAD" === $test->testGetRequestMethod());
var_dump("HEAD" === $test->getRequestMethod());
$test->testSetRequestUrl("/");
var_dump("/" === $test->testGetRequestUrl());
var_dump("/" === $test->getRequestUrl());
var_dump("HEAD / HTTP/1.1" === $test->getInfo());
$test->testSetType(http\Message::TYPE_RESPONSE);
$test->setResponseStatus("Created");
var_dump("Created" === $test->testGetResponseStatus());
var_dump("Created" === $test->getResponseStatus());
$test->setResponseCode(201);
var_dump(201 === $test->testGetResponseCode());
var_dump(201 === $test->getResponseCode());
$test->testSetResponseStatus("Ok");
var_dump("Ok" === $test->testGetResponseStatus());
var_dump("Ok" === $test->getResponseStatus());
$test->testSetResponseCode(200);
var_dump(200 === $test->testGetResponseCode());
var_dump(200 === $test->getResponseCode());
$test->testSetHttpVersion("1.0");
var_dump("1.0" === $test->testGetHttpVersion());
var_dump("1.0" === $test->getHttpVersion());
var_dump("HTTP/1.0 200 OK" === $test->getInfo());
$test->setHttpVersion("1.1");
var_dump("1.1" === $test->testGetHttpVersion());
var_dump("1.1" === $test->getHttpVersion());
var_dump("HTTP/1.1 200 OK" === $test->getInfo());
$test->setInfo("HTTP/1.1 201 Created");
var_dump("Created" === $test->testGetResponseStatus());
var_dump("Created" === $test->getResponseStatus());
var_dump(201 === $test->testGetResponseCode());
var_dump(201 === $test->getResponseCode());
var_dump("1.1" === $test->testGetHttpVersion());
var_dump("1.1" === $test->getHttpVersion());
$test->testSetHeaders(array("Foo" => "bar"));
var_dump(array("Foo" => "bar") === $test->testGetHeaders());
var_dump(array("Foo" => "bar") === $test->getHeaders());
var_dump("bar" === $test->getHeader("foo"));
var_dump(false === $test->getHeader("bar"));
$parent = new message;
$test->testSetParentMessage($parent);
var_dump($parent === $test->testGetParentMessage());
var_dump($parent === $test->getParentMessage());

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Done
