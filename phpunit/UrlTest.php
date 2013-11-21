<?php

class UrlTest extends PHPUnit_Framework_TestCase {
	protected $url; 
	function setUp() {
		$this->url = "http://user:pass@www.example.com:8080/path/file.ext".
			"?foo=bar&more[]=1&more[]=2#hash";
    }

	function testStandard() {
		$this->assertEquals($this->url, (string) new http\Url($this->url));
		
		$url = new http\Url($this->url, 
			array("path" => "changed", "query" => "foo=&added=this"), 
			http\Url::JOIN_PATH |
			http\Url::JOIN_QUERY |
			http\Url::STRIP_AUTH |
			http\Url::STRIP_FRAGMENT
		);

		$this->assertEquals("http", $url->scheme);
		$this->assertEmpty($url->user);
		$this->assertEmpty($url->pass);
		$this->assertEquals("www.example.com", $url->host);
		$this->assertEquals(8080, $url->port);
		$this->assertEquals("/path/changed", $url->path);
		$this->assertEquals("more%5B0%5D=1&more%5B1%5D=2&added=this", $url->query);
        $this->assertEmpty($url->fragment);
    }

    function testMod() {
        $tmp = new http\Url($this->url);
        $mod = $tmp->mod(array("query" => "set=1"), http\Url::REPLACE);
        $this->assertNotEquals($tmp->toArray(), $mod->toArray());
        $this->assertEquals("set=1", $mod->query);
        $this->assertEquals("new_fragment", $tmp->mod("#new_fragment")->fragment);
    }

    function testStrings() {
        $url = new http\Url($this->url);
        $this->assertEquals((string) $url, (string) new http\Url((string) $url));
    }

    function testArrays() {
        $url = new http\Url($this->url);
		$url2 = new http\Url($url->toArray());
        $this->assertEquals($url->toArray(), $url2->toArray());
    }
}
