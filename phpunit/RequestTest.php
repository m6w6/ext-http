<?php

class ProgressObserver1 implements SplObserver {
    function update(SplSubject $r) {
        if ($r->getProgress()) $r->pi .= "-";
    }
}
class ProgressObserver2 implements SplObserver {
    function update(SplSubject $r) {
        if ($r->getProgress()) $r->pi .= ".";
    }
}
class CallbackObserver implements SplObserver {
    public $callback;
    function __construct($callback) {
        $this->callback = $callback;
    }
    function update(SplSubject $r) {
        call_user_func($this->callback, $r);
    } 
}

class RequestTest extends PHPUnit_Framework_TestCase
{
    /**
     * @var http\Request
     */
    protected $r;

    function setUp() {
        $this->r = (new http\Request\Factory)->createRequest();
        $this->r->setOptions(
            array(
                "connecttimeout"    => 30,
                "timeout"           => 300,
            )
        );
    }

    function testClone() {
        $this->r->setUrl("http://dev.iworks.at/ext-http/.print_request.php");
        $c = clone $this->r;
        $c->setUrl("http://dev.iworks.at/ext-http/.print_headers.php");
        $this->assertNotEquals($this->r->send(), $c->send());
    }

    function testObserver() {
        $this->r->attach(new ProgressObserver1);
        $this->r->attach(new ProgressObserver2);
        $this->r->attach(
            new CallbackObserver(
                function ($r) {
                    $p = (array) $r->getProgress();
                    $this->assertArrayHasKey("started", $p);
                    $this->assertArrayHasKey("finished", $p);
                    $this->assertArrayHasKey("dlnow", $p);
                    $this->assertArrayHasKey("ulnow", $p);
                    $this->assertArrayHasKey("dltotal", $p);
                    $this->assertArrayHasKey("ultotal", $p);
                    $this->assertArrayHasKey("info", $p);
                }
            )
        );
        $this->r->setUrl("http://dev.iworks.at/ext-http/")->send();
        $this->assertRegexp("/(\.-)+/", $this->r->pi);
        $this->assertCount(3, $this->r->getObservers());
    }

    function testCookies() {
        $this->r->setUrl("http://dev.iworks.at/ext-http/.cookie.php")->send();
        $this->assertNotContains("Cookie", (string) $this->r->getRequestMessage());
        $this->r->send();
        $this->assertNotContains("Cookie", (string) $this->r->getRequestMessage());
        $this->r->enableCookies()->send();
        $this->assertNotContains("Cookie", (string) $this->r->getRequestMessage());
        $this->r->send();
        $this->assertContains("Cookie", (string) $this->r->getRequestMessage());
        $this->assertCount(2, $this->r->getResponseCookies());
    }

    function testResetCookies() {
        $this->r->setUrl("http://dev.iworks.at/ext-http/.cookie.php");

        $this->r->enableCookies();
        $this->r->send();

        $f = function ($a) { return $a->getCookies(); };
        $c = array_map($f, $this->r->getResponseCookies());

        $this->r->send();
        $this->assertEquals($c, array_map($f, $this->r->getResponseCookies()));
        
        $this->r->resetCookies();
        $this->r->send();
        $this->assertNotEquals($c, array_map($f, $this->r->getResponseCookies()));
    }

    function testHistory() {
        $body = new http\Message\Body;
        $body->append("foobar");
        $this->r->setBody($body);

        $this->r->recordHistory = true;

        $this->r->setMethod("POST");
        $this->r->setUrl("http://dev.iworks.at/ext-http/.print_request.php");

        $this->r->send();
        $this->assertStringMatchesFormat(<<<HTTP
POST /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Content-Length: 6
Content-Type: application/x-www-form-urlencoded
X-Original-Content-Length: 6

foobar
HTTP/1.1 200 OK
Date: %s
Server: %s
Vary: %s
Content-Length: 19
Content-Type: text/html
X-Original-Content-Length: 19

string(6) "foobar"

HTTP
        , str_replace("\r", "", $this->r->getHistory()->toString(true))
        );

        $this->r->send();
        $this->assertStringMatchesFormat(<<<HTTP
POST /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Content-Length: 6
Content-Type: application/x-www-form-urlencoded
X-Original-Content-Length: 6

foobar
HTTP/1.1 200 OK
Date: %s
Server: %s
Vary: %s
Content-Length: 19
Content-Type: text/html
X-Original-Content-Length: 19

string(6) "foobar"

POST /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Content-Length: 6
Content-Type: application/x-www-form-urlencoded
X-Original-Content-Length: 6

foobar
HTTP/1.1 200 OK
Date: %s
Server: %s
Vary: %s
Content-Length: 19
Content-Type: text/html
X-Original-Content-Length: 19

string(6) "foobar"

HTTP
        , str_replace("\r", "", $this->r->getHistory()->toString(true))
        );
    }
}

