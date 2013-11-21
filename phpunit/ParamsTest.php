<?php

class ParamsTest extends PHPUnit_Framework_TestCase {
    function testDefault() {
        $s = "foo, bar;arg=0;bla, gotit=0;now";
        $this->runAssertions(
            new http\Params($s),
            str_replace(" ", "", $s)
        );
    }

    function testCustom() {
        $s = "foo bar.arg:0.bla gotit:0.now";
        $this->runAssertions(
            new http\Params($s, " ", ".", ":"),
            $s
        );
    }

    function testQuoted() {
        $p = new http\Params("multipart/form-data; boundary=\"--123\"");
        $this->assertEquals(
            array(
                "multipart/form-data" => array(
                    "value" => true,
                    "arguments" => array(
                        "boundary" => "--123"
                    )
                )
            ),
            $p->params
        );
        $this->assertEquals("multipart/form-data;boundary=--123", (string) $p);
    }

    function testEscaped() {
        $p = new http\Params("form-data; name=\"upload\"; filename=\"trick\\\"\0\\\"ed\"");
        $this->assertEquals(
            array(
                "form-data" => array(
                    "value" => true,
                    "arguments" => array(
                        "name" => "upload",
                        "filename" => "trick\"\0\"ed"
                    )
                )
            ),
            $p->params
        );
        $this->assertEquals("form-data;name=upload;filename=\"trick\\\"\\0\\\"ed\"", (string) $p);
    }

    function testUrlencoded() {
        $s = "foo=b%22r&bar=b%22z&a%5B%5D%5B%5D=1";
        $p = new http\Params($s, "&", "", "=", http\Params::PARSE_URLENCODED);
        $this->assertEquals(
            array(
                "foo" => array(
                    "value" => "b\"r",
                    "arguments" => array(),
                ),
                "bar" => array(
                    "value" => "b\"z",
                    "arguments" => array(),
                ),
                "a[][]" => array(
                    "value" => "1",
                    "arguments" => array(),
                ),
            ),
            $p->params
        );
        $this->assertEquals("foo=b%22r&bar=b%22z&a%5B%5D%5B%5D=1", (string) $p);
    }

    function testQuery() {
        $s = "foo=b%22r&bar=b%22z&a%5B%5D%5B%5D=1";
        $p = new http\Params($s, "&", "", "=", http\Params::PARSE_QUERY);
        $this->assertEquals(
            array(
                "foo" => array(
                    "value" => "b\"r",
                    "arguments" => array(),
                ),
                "bar" => array(
                    "value" => "b\"z",
                    "arguments" => array(),
                ),
                "a" => array(
                    "value" => array(
                        array("1")
                    ),
                    "arguments" => array(),
                ),
            ),
            $p->params
        );
        $this->assertEquals("foo=b%22r&bar=b%22z&a%5B0%5D%5B0%5D=1", (string) $p);
    }


    function testEmpty() {
        $p = new http\Params(NULL);
        $this->assertEquals(array(), $p->params);
    }

    function testErrorOfToArrayWithArgs() {
        $this->setExpectedException("PHPUnit_Framework_Error_Warning");
        $p = new http\Params();
        $p->toArray("dummy");
    }

    function testIntegerKeys() {
        $p = new http\Params("0=nothing;1=yes");
        $this->assertEquals(array("0" => array("value" => "nothing", "arguments" => array(1=>"yes"))), $p->params);
        $this->assertEquals("0=nothing;1=yes", $p->toString());
    }

    function testBoolParamArguments() {
        $p = new http\Params;
        $container = array("value" => false, "arguments" => array("wrong" => false, "correct" => true));
        $p["container"] = $container;
        $this->assertEquals("container=0;wrong=0;correct", $p->toString());
        $this->assertEquals(array("container" => $container), $p->toArray());
    }

    function testNoArgsForParam() {
        $p = new http\Params;
        $p["param"] = true;
        $this->assertEquals("param", $p->toString());
        $p["param"] = false;
        $this->assertEquals("param=0", $p->toString());
    }

    protected function runAssertions($p, $s) {
        $this->assertCount(3, $p->params);
        $this->assertArrayHasKey("foo", $p->params);
        $this->assertArrayHasKey("bar", $p->params);
        $this->assertArrayHasKEy("gotit", $p->params);

        $this->assertTrue($p["foo"]["value"]);
        $this->assertTrue($p["bar"]["value"]);
        $this->assertEmpty($p["gotit"]["value"]);

        $this->assertEmpty($p["foo"]["arguments"]);
        $this->assertCount(2, $p["bar"]["arguments"]);
        $this->assertCount(1, $p["gotit"]["arguments"]);

        $this->assertEmpty($p["bar"]["arguments"]["arg"]);
        $this->assertTrue($p["bar"]["arguments"]["bla"]);
        $this->assertTrue($p["gotit"]["arguments"]["now"]);

        $this->assertEquals($s, (string) $p);

        $comp = array (
            'foo' => 
            array (
                'value' => true,
                'arguments' => 
                array (
                ),
            ),
            'bar' => 
            array (
                'value' => true,
                'arguments' => 
                array (
                    'arg' => '0',
                    'bla' => true,
                ),
            ),
            'gotit' => 
            array (
                'value' => '0',
                'arguments' => 
                array (
                    'now' => true,
                ),
            ),
        );

        $this->assertEquals($comp, $p->params);
        $a = new http\Params($p->params);
        $this->assertEquals($comp, $a->toArray());
	}
}
