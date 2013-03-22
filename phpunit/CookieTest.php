<?php

class CookieTest extends PHPUnit_Framework_TestCase {
    function testEmpty() {
        $c = new http\Cookie;
        $o = clone $c;
        $a = array(
            "cookies" => array(),
            "extras" => array(),
            "flags" => 0,
            "expires" => -1,
            "path" => "",
            "domain" => "",
        	"max-age" => -1,
        );
        $this->assertEquals($a, $c->toArray());
        $this->assertEquals($a, $o->toArray());
    }

    function testExpiresAsDate() {
        $d = new DateTime;
        $c = new http\Cookie(array("expires" => $d->format(DateTime::RFC1123)));
        $this->assertEquals($d->format("U"), $c->getExpires());
    }

    function testNumeric() {
        $c = new http\Cookie("1=%20; 2=%22; 3=%5D", 0, array(2));
        $this->assertEquals("1=%20; 3=%5D; 2=%22; ", (string) $c);
    }

    function testRaw() {
        $c = new http\Cookie("1=%20; 2=%22; e3=%5D", http\Cookie::PARSE_RAW, array(2));
        $this->assertEquals("1=%2520; e3=%255D; 2=%2522; ", (string) $c);
    }

    function testSimple() {
        $orig = new http\Cookie("key=value");
        $copy = clone $orig;
        $same = new http\Cookie($copy);
        $even = new http\Cookie($same->toArray());
        foreach (array($orig, $copy) as $c) {
            $this->assertEquals("value", $c->getCookie("key"));
            $this->assertEquals(-1, $c->getExpires());
            $this->assertEquals(-1, $c->getMaxAge());
            $this->assertEquals(0, $c->getFlags());
            $this->assertEquals(null, $c->getPath());
            $this->assertEquals(null, $c->getDomain());
            $this->assertEquals(array(), $c->getExtras());
            $this->assertEquals(array("key" => "value"), $c->getCookies());
            $this->assertEquals("key=value; ", $c->toString());
            $this->assertEquals(
                array (
                    "cookies" => 
                        array (
                            "key" => "value",
                        ),
                    "extras" => 
                        array (
                        ),
                    "flags" => 0,
                    "expires" => -1,
                    "path" => "",
                    "domain" => "",
                	"max-age" => -1,
                ),
                $c->toArray()
            );
        }
    }

    function testExpires() {
        $c = new http\Cookie("this=expires; expires=Tue, 24 Jan 2012 10:35:32 +0100");
        $this->assertEquals("expires", $c->getCookie("this"));
        $this->assertEquals(1327397732, $c->getExpires());
        $o = clone $c;
        $t = time();
        $o->setExpires();
        $this->assertEquals(-1, $o->getExpires());
        $this->assertNotEquals(-1, $c->getExpires());
        $o->setExpires($t);
        $this->assertEquals($t, $o->getExpires());
        $this->assertNotEquals($t, $c->getExpires());
        $this->assertEquals(
            sprintf(
                "this=expires; expires=%s; ", 
                date_create("@$t")
            		->setTimezone(new DateTimezone("UTC"))
            		->format("D, d M Y H:i:s \\G\\M\\T")
            ), 
            $o->toString()
        );
    }

    function testMaxAge() {
        $c = new http\Cookie("this=max-age; max-age=12345");
        $this->assertEquals("max-age", $c->getCookie("this"));
        $this->assertEquals(12345, $c->getMaxAge());
        $o = clone $c;
        $t = 54321;
        $o->setMaxAge();
        $this->assertEquals(-1, $o->getMaxAge());
        $this->assertNotEquals(-1, $c->getMaxAge());
        $o->setMaxAge($t);
        $this->assertEquals($t, $o->getMaxAge());
        $this->assertNotEquals($t, $c->getMaxAge());
        $this->assertEquals(
        	"this=max-age; max-age=$t; ",
            $o->toString()
        );
    }

    function testPath() {
        $c = new http\Cookie("this=has a path; path=/down; ");
        $this->assertEquals("has a path", $c->getCookie("this"));
        $this->assertEquals("this=has%20a%20path; path=/down; ", (string)$c);
        $this->assertEquals("/down", $c->getPath());
        $o = clone $c;
        $p = "/up";
        $o->setPath();
        $this->assertEquals(null, $o->getPath());
        $this->assertNotEquals(null, $c->getPath());
        $o->setPath($p);
        $this->assertEquals($p, $o->getPath());
        $this->assertNotEquals($p, $c->getPath());
        $this->assertEquals("this=has%20a%20path; path=$p; ", $o->toString());
    }

    function testDomain() {
        $c = new http\Cookie("this=has a domain; domain=.example.com; ");
        $this->assertEquals("has a domain", $c->getCookie("this"));
        $this->assertEquals("this=has%20a%20domain; domain=.example.com; ", (string)$c);
        $this->assertEquals(".example.com", $c->getDomain());
        $o = clone $c;
        $d = "sub.example.com";
        $o->setDomain();
        $this->assertEquals(null, $o->getDomain());
        $this->assertNotEquals(null, $c->getDomain());
        $o->setDomain($d);
        $this->assertEquals($d, $o->getDomain());
        $this->assertNotEquals($d, $c->getDomain());
        $this->assertEquals("this=has%20a%20domain; domain=$d; ", $o->toString());
    }

    function testFlags() {
        $c = new http\Cookie("icanhas=flags; secure; httpOnly");
        $this->assertEquals(http\Cookie::SECURE, $c->getFlags() & http\Cookie::SECURE, "secure");
        $this->assertEquals(http\Cookie::HTTPONLY, $c->getFlags() & http\Cookie::HTTPONLY, "httpOnly");
        $c->setFlags($c->getFlags() ^ http\Cookie::SECURE);
        $this->assertEquals(0, $c->getFlags() & http\Cookie::SECURE, "secure");
        $this->assertEquals(http\Cookie::HTTPONLY, $c->getFlags() & http\Cookie::HTTPONLY, "httpOnly");
        $c->setFlags($c->getFlags() ^ http\Cookie::HTTPONLY);
        $this->assertEquals(0, $c->getFlags() & http\Cookie::SECURE, "secure");
        $this->assertEquals(0, $c->getFlags() & http\Cookie::HTTPONLY, "httpOnly");
        $this->assertEquals("icanhas=flags; ", $c->toString());
        $c->setFlags(http\Cookie::SECURE|http\Cookie::HTTPONLY);
        $this->assertEquals("icanhas=flags; secure; httpOnly; ", $c->toString());
    }

    function testExtras() {
        $c = new http\Cookie("c1=v1; e0=1; e2=2; c2=v2", 0, array("e0", "e1", "e2"));
        $this->assertEquals(array("c1"=>"v1", "c2"=>"v2"), $c->getCookies());
        $this->assertEquals(array("e0"=>"1", "e2"=>"2"), $c->getExtras());
        $c->addExtra("e1", 1);
        $c->setExtra("e0");
        $c->setExtra("e3", 123);
        $this->assertEquals(123, $c->getExtra("e3"));
        $c->setExtra("e3");
        $this->assertEquals(array("e2"=>"2", "e1"=>1), $c->getExtras());
        $this->assertEquals("c1=v1; c2=v2; e2=2; e1=1; ", $c->toString());
        $c->addExtras(array("e3"=>3, "e4"=>4));
        $this->assertEquals(array("e2"=>"2", "e1"=>1, "e3"=>3, "e4"=>4), $c->getExtras());
        $this->assertEquals("c1=v1; c2=v2; e2=2; e1=1; e3=3; e4=4; ", $c->toString());
        $c->setExtras(array("e"=>"x"));
        $this->assertEquals(array("e"=>"x"), $c->getExtras());
        $this->assertEquals("c1=v1; c2=v2; e=x; ", $c->toString());
        $c->setExtras();
        $this->assertEquals(array(), $c->getExtras());
        $this->assertEquals("c1=v1; c2=v2; ", $c->toString());
    }

    function testCookies() {
        $c = new http\Cookie("e0=1; c1=v1; e2=2; c2=v2", 0, array("c0", "c1", "c2"));
        $this->assertEquals(array("c1"=>"v1", "c2"=>"v2"), $c->getExtras());
        $this->assertEquals(array("e0"=>"1", "e2"=>"2"), $c->getCookies());
        $c->addCookie("e1", 1);
        $c->setCookie("e0");
        $c->setCookie("e3", 123);
        $this->assertEquals(123, $c->getCookie("e3"));
        $c->setCookie("e3");
        $this->assertEquals(array("e2"=>"2", "e1"=>1), $c->getCookies());
        $this->assertEquals("e2=2; e1=1; c1=v1; c2=v2; ", $c->toString());
        $c->addCookies(array("e3"=>3, "e4"=>4));
        $this->assertEquals(array("e2"=>"2", "e1"=>1, "e3"=>3, "e4"=>4), $c->getCookies());
        $this->assertEquals("e2=2; e1=1; e3=3; e4=4; c1=v1; c2=v2; ", $c->toString());
        $c->setCookies(array("e"=>"x"));
        $this->assertEquals(array("e"=>"x"), $c->getCookies());
        $this->assertEquals("e=x; c1=v1; c2=v2; ", $c->toString());
        $c->setCookies();
        $this->assertEquals(array(), $c->getCookies());
        $this->assertEquals("c1=v1; c2=v2; ", $c->toString());
    }
}
