<?php

class strval {
	private $str;
	function __construct($str) {
		$this->str = $str;
	}
	function __toString() {
		return (string) $this->str;
	}
}

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

class MessageTest extends PHPUnit_Framework_TestCase
{
	function testProperties() {
		$test = new message;
		$this->assertEquals(0, $test->testGetType());
		$this->assertEquals(null, $test->testGetBody());
		$this->assertEquals("", $test->testGetRequestMethod());
		$this->assertEquals("", $test->testGetRequestUrl());
		$this->assertEquals("", $test->testGetResponseStatus());
		$this->assertEquals(0, $test->testGetResponseCode());
		$this->assertEquals("1.1", $test->testGetHttpVersion());
		$this->assertEquals(array(), $test->testGetHeaders());
		$this->assertEquals(null, $test->testGetParentMessage());
		$test->testSetType(http\Message::TYPE_REQUEST);
		$this->assertEquals(http\Message::TYPE_REQUEST, $test->testGetType());
		$this->assertEquals(http\Message::TYPE_REQUEST, $test->getType());
		$body = new http\Message\Body;
		$test->testSetBody($body);
		$this->assertEquals($body, $test->testGetBody());
		$this->assertEquals($body, $test->getBody());
		$test->testSetRequestMethod("HEAD");
		$this->assertEquals("HEAD", $test->testGetRequestMethod());
		$this->assertEquals("HEAD", $test->getRequestMethod());
		$test->testSetRequestUrl("/");
		$this->assertEquals("/", $test->testGetRequestUrl());
		$this->assertEquals("/", $test->getRequestUrl());
		$this->assertEquals("HEAD / HTTP/1.1", $test->getInfo());
		$test->testSetType(http\Message::TYPE_RESPONSE);
		$test->setResponseStatus("Created");
		$this->assertEquals("Created", $test->testGetResponseStatus());
		$this->assertEquals("Created", $test->getResponseStatus());
		$test->setResponseCode(201);
		$this->assertEquals(201, $test->testGetResponseCode());
		$this->assertEquals(201, $test->getResponseCode());
		$test->testSetResponseStatus("Ok");
		$this->assertEquals("Ok", $test->testGetResponseStatus());
		$this->assertEquals("Ok", $test->getResponseStatus());
		$test->testSetResponseCode(200);
		$this->assertEquals(200, $test->testGetResponseCode());
		$this->assertEquals(200, $test->getResponseCode());
		$test->testSetHttpVersion("1.0");
		$this->assertEquals("1.0", $test->testGetHttpVersion());
		$this->assertEquals("1.0", $test->getHttpVersion());
		$this->assertEquals("HTTP/1.0 200 OK", $test->getInfo());
		$test->setHttpVersion("1.1");
		$this->assertEquals("1.1", $test->testGetHttpVersion());
		$this->assertEquals("1.1", $test->getHttpVersion());
		$this->assertEquals("HTTP/1.1 200 OK", $test->getInfo());
		$test->setInfo("HTTP/1.1 201 Created");
		$this->assertEquals("Created", $test->testGetResponseStatus());
		$this->assertEquals("Created", $test->getResponseStatus());
		$this->assertEquals(201, $test->testGetResponseCode());
		$this->assertEquals(201, $test->getResponseCode());
		$this->assertEquals("1.1", $test->testGetHttpVersion());
		$this->assertEquals("1.1", $test->getHttpVersion());
		$test->testSetHeaders(array("Foo" => "bar"));
		$this->assertEquals(array("Foo" => "bar"), $test->testGetHeaders());
		$this->assertEquals(array("Foo" => "bar"), $test->getHeaders());
		$this->assertEquals("bar", $test->getHeader("foo"));
		$this->assertEquals(null, $test->getHeader("bar"));
		$parent = new message;
		$test->testSetParentMessage($parent);
		$this->assertEquals($parent, $test->testGetParentMessage());
		$this->assertEquals($parent, $test->getParentMessage());
	}
	
	function testAddBody() {
		$m = new http\Message;
		$body = new http\Message\Body;
		$body->append("foo");
		$m->addBody($body);
		$body = new http\Message\Body;
		$body->append("bar");
		$m->addBody($body);
		$this->assertEquals("foobar", (string) $m->getBody());
	}
	
	function testAddHeaders() {
		$m = new http\Message;
		$m->addHeaders(array("foo"=>"bar","bar"=>"foo"));
		$this->assertEquals(array("Foo"=>"bar","Bar"=>"foo"), $m->getHeaders());
		$m->addHeaders(array("key"=>"val","more"=>"Stuff"));
		$this->assertEquals(array("Foo"=>"bar","Bar"=>"foo","Key"=>"val","More"=>"Stuff"), $m->getHeaders());
	}
	
	function testArrayHeaders() {
		$m = new http\Message("GET / HTTP/1.1");
		$m->addHeader("Accept", "text/html");
		$m->addHeader("Accept", "text/xml;q=0");
		$m->addHeader("Accept", "text/plain;q=0.5");
		$this->assertEquals(
			"GET / HTTP/1.1\r\n".
			"Accept: text/html, text/xml;q=0, text/plain;q=0.5\r\n",
			$m->toString()
		);
	}
	
	function testHeaderValueTypes() {
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
					"expires" => date_create("2012-12-31 23:59:59")->format(
						DateTime::COOKIE
					),
					"path" => "/somewhere"
				)
			)
		);
		$m->addHeader("Set-Cookie", "val=0");
		
		$this->assertEquals(
			"HTTP/1.1 200 Ok\r\n".
			"Bool: true\r\n".
			"Int: 123\r\n".
			"Float: 1.23\r\n".
			"Array: 1, 2, 3\r\n".
			"Object: test\r\n".
			"Set-Cookie: foo=bar; path=/somewhere; expires=Mon, 31 Dec 2012 22:59:59 GMT; \r\n".
			"Set-Cookie: val=0\r\n",
			$m->toString()
		);
	}
}

