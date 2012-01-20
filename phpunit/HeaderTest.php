<?php

class HeaderTest extends PHPUnit_Framework_TestCase {
    function setUp() {
        $this->h = new http\Header("foo", "bar");
    }
    function testString() {
        $this->assertEquals("Foo: bar", (string) $this->h);
    }

    function testSerialize() {
        $this->assertEquals("Foo: bar", (string) unserialize(serialize($this->h)));
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
}
