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

        $this->assertEquals(
            array (
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
            ),
            $p->params
        );
	}
}
