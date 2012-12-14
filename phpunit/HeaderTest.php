<?php

class HeaderTest extends PHPUnit_Framework_TestCase {
    function setUp() {
        
    }
    function testString() {
    	$h = new http\Header("foo", "bar");
        $this->assertEquals("Foo: bar", (string) $h);
    }

    function testSerialize() {
		$h = new http\Header("foo", "bar");
		$this->assertEquals("Foo: bar", (string) unserialize(serialize($h)));
    }

	function testNumeric() {
		$h = new http\Header(123, 456);
        $this->assertEquals("123: 456", (string) $h);
		$this->assertEquals("123: 456", (string) unserialize(serialize($h)));
	}
	
    function testMatch() {
        $ae = new http\Header("Accept-encoding", "gzip, deflate");
        $this->assertTrue($ae->match("gzip", http\Header::MATCH_WORD));
        $this->assertTrue($ae->match("gzip", http\Header::MATCH_WORD|http\Header::MATCH_CASE));
        $this->assertFalse($ae->match("gzip", http\Header::MATCH_STRICT));
        $this->assertTrue($ae->match("deflate", http\Header::MATCH_WORD));
        $this->assertTrue($ae->match("deflate", http\Header::MATCH_WORD|http\Header::MATCH_CASE));
        $this->assertFalse($ae->match("deflate", http\Header::MATCH_STRICT));

        $this->assertFalse($ae->match("zip", http\Header::MATCH_WORD));
        $this->assertFalse($ae->match("gzip", http\Header::MATCH_FULL));
    }
    
    function testNegotiate() {
    	$a = new http\Header("Accept", "text/html, text/plain;q=0.5, */*;q=0");
    	$this->assertEquals("text/html", $a->negotiate(array("text/plain","text/html")));
    	$this->assertEquals("text/html", $a->negotiate(array("text/plain","text/html"), $rs));
    	$this->assertEquals(array("text/html"=>0.99, "text/plain"=>0.5), $rs);
    	$this->assertEquals("text/plain", $a->negotiate(array("foo/bar", "text/plain"), $rs));
    	$this->assertEquals(array("text/plain"=>0.5), $rs);
    	$this->assertEquals("foo/bar", $a->negotiate(array("foo/bar"), $rs));
    	$this->assertEquals(array(), $rs);
    }
    
    function testParse() {
    	$header = "Foo: bar\nBar: foo\n";
    	$this->assertEquals(array("Foo"=>"bar","Bar"=>"foo"), http\Header::parse($header)); 
    	$header = http\Header::parse($header, "http\\Header");
    	$this->assertCount(2, $header);
    	$this->assertContainsOnlyInstancesOf("http\\Header", $header); 
    }
    
    function testParseError() {
    	$header = "wass\nup";
    	$this->setExpectedException("PHPUnit_Framework_Error_Warning", "Could not parse headers");
    	$this->assertFalse(http\Header::parse($header));
    }
	
	function testParams() {
		$header = new http\Header("Cache-control", "public, must-revalidate, max-age=0");
		$this->assertEquals(
			array(
				"public" => array("value" => true, "arguments" => array()),
				"must-revalidate" => array("value" => true, "arguments" => array()),
				"max-age" => array("value" => 0, "arguments" => array()),
			),
			$header->getParams()->params
		);
	}
	
	function testParamsWithArgs() {
		$header = new http\Header("Custom", '"foo" is "bar". "bar" is "bis" where "bis" is 3.');
		$this->assertEquals(
			array(
				"foo" => array("value" => "bar", "arguments" => array()),
				"bar" => array("value" => "baz", "arguments" => array("baz" => "3"))
			),
			$header->getParams(".", "where", "is")->params
		);
	}
}
