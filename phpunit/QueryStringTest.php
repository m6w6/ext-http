<?php

class QueryStringTest extends PHPUnit_Framework_TestCase {
    protected $q;
    protected $s = "a=b&r[]=0&r[]=1&r[]=2&rr[][]=00&rr[][]=01&1=2";
    protected $e = "a=b&r%5B0%5D=0&r%5B1%5D=1&r%5B2%5D=2&rr%5B0%5D%5B0%5D=00&rr%5B0%5D%5B1%5D=01&1=2";

    function setUp() {
        $this->q = new http\QueryString($this->s);
    }

    function testSimple() {
        $this->assertEquals($this->e, (string) $this->q);
        $this->assertEquals($this->e, $this->q->get());
    }

    function testGetDefval() {
        $this->assertEquals("nonexistant", $this->q->get("unknown", "s", "nonexistant"));
        $this->assertEquals(null, $this->q->get("unknown"));
    }

    function testGetA() {
        $this->assertEquals("b", $this->q->get("a"));
        $this->assertEquals(0, $this->q->get("a", "i"));
        $this->assertEquals(array("b"), $this->q->get("a", "a"));
        $this->assertEquals((object)array("scalar" => "b"), $this->q->get("a", "o"));
    }

    function testGetR() {
        $this->assertEquals(array(0,1,2), $this->q->get("r"));
    }

    function testGetRR() {
        $this->assertEquals(array(array("00","01")), $this->q->get("rr"));
    }

    function testGet1() {
        $this->assertEquals(2, $this->q->get(1));
        $this->assertEquals("2", $this->q->get(1, "s"));
        $this->assertEquals(2.0, $this->q->get(1, "f"));
        $this->assertTrue($this->q->get(1, "b"));
    }

    function testDelA() {
        $this->assertEquals("b", $this->q->get("a", http\QueryString::TYPE_STRING, null, true));
        $this->assertEquals(null, $this->q->get("a"));
    }

    function testDelAll() {
        $this->q->set(array("a" => null, "r" => null, "rr" => null, 1 => null));
        $this->assertEquals("", $this->q->toString());
    }

    function testQSO() {
        $this->assertEquals($this->e, (string) new http\QueryString($this->q));
        $this->assertEquals(http_build_query(array("e"=>$this->q->toArray())), (string) new http\QueryString(array("e" => $this->q)));
    }

    function testIterator() {
        $this->assertEquals($this->q->toArray(), iterator_to_array($this->q));
    }

    function testSerialize() {
        $this->assertEquals($this->e, (string) unserialize(serialize($this->q)));
    }
}
