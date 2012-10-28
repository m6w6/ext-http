<?php

use http\Client\Request;
use http\Message\Body;

class ClientRequestTest extends PHPUnit_Framework_TestCase
{
    function testStandard() {
        $r = new Request($m = "POST", $u = "http://localhost/foo", $h = array("header", "value"),
            $b = new Body(fopen(__FILE__, "r+b"))
        );

        $this->assertEquals($b, $r->getBody());
        $this->assertEquals($h, $r->getHeaders());
        $this->assertEquals($u, $r->getRequestUrl());
        $this->assertEquals($m, $r->getRequestMethod());
    }

    function testContentType() {
        $r = new Request("POST", "http://localhost/");
        $this->assertEquals($r, $r->setContentType($ct = "text/plain; charset=utf-8"));
        $this->assertEquals($ct, $r->getContentType());
    }

    function testQuery() {
        $r = new Request("GET", "http://localhost/");
        $this->assertEquals(null, $r->getQuery());
        $this->assertEquals($r, $r->setQuery($q = "foo=bar"));
        $this->assertEquals($q, $r->getQuery());
        $this->assertEquals($r, $r->addQuery("a[]=1&a[]=2"));
        $this->assertEquals("foo=bar&a%5B0%5D=1&a%5B1%5D=2", $r->getQuery());
        $this->assertEquals(null, $r->setQuery(null)->getQuery());
    }

    function testOptions() {
        $r = new Request("GET", "http://localhost");
        $this->assertEquals($r, $r->setOptions($o = array("redirect"=>5, "timeout"=>5)));
        $this->assertEquals($o, $r->getOptions());
        $this->assertEquals($r, $r->setSslOptions($o = array("verify_peer"=>false)));
        $this->assertEquals($o, $r->getSslOptions());
    }
}

