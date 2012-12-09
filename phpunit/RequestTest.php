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
		$f = new http\Client\Factory;
        $this->r = $f->createClient();
        $this->r->setOptions(
            array(
                "connecttimeout"    => 30,
                "timeout"           => 300,
            )
        );
        $this->r->setOptions(
        	array(
        		"timeout" => 600
        	)
        );
        $this->assertEquals(
        	array(
        		"connecttimeout" => 30,
        		"timeout" => 600,
        	),
        	$this->r->getOptions()
        );
    }

    function testClone() {
        $c = clone $this->r;
        $this->assertNotSame($this->r, $c);
    }

    function testObserver() {
		$test = $this;
        $this->r->attach($o1 = new ProgressObserver1);
        $this->r->attach($o2 = new ProgressObserver2);
        $this->r->attach(
            $o3 = new CallbackObserver(
                function ($r) use ($test) {
                    $p = (array) $r->getProgress();
                    $test->assertArrayHasKey("started", $p);
                    $test->assertArrayHasKey("finished", $p);
                    $test->assertArrayHasKey("dlnow", $p);
                    $test->assertArrayHasKey("ulnow", $p);
                    $test->assertArrayHasKey("dltotal", $p);
                    $test->assertArrayHasKey("ultotal", $p);
                    $test->assertArrayHasKey("info", $p);
                }
            )
        );
        $this->r->setRequest(new http\Client\Request("GET", "http://dev.iworks.at/ext-http/"))->send(null);
        $this->assertRegexp("/(\.-)+/", $this->r->pi);
        $this->assertCount(3, $this->r->getObservers());
        $this->r->detach($o1);
        $this->assertCount(2, $this->r->getObservers());
        $this->r->detach($o2);
        $this->assertCount(1, $this->r->getObservers());
        $this->r->detach($o3);
        $this->assertCount(0, $this->r->getObservers());
    }

    function testCookies() {
        $this->r->setRequest(new http\Client\Request("GET", "http://dev.iworks.at/ext-http/.cookie.php"))->send(null);
        $this->assertNotContains("Cookie", (string) $this->r->getRequestMessage());
        $this->r->send(null);
        $this->assertNotContains("Cookie", (string) $this->r->getRequestMessage());
        $this->r->enableCookies()->send(null);
        $this->assertNotContains("Cookie", (string) $this->r->getRequestMessage());
        $this->r->send(null);
        $this->assertContains("Cookie", (string) $this->r->getRequestMessage());
        $cookies = $this->r->getResponseMessage()->getCookies(0, array("extra"));
        $this->assertCount(2, $cookies);
        foreach ($cookies as $cookie) {
        	if ($cookie->getCookie("perm")) {
        		$this->assertTrue(0 < $cookie->getExpires());
        	}
        	if ($cookie->getCookie("temp")) {
        		$this->assertEquals(-1, $cookie->getExpires());
        	}
        }
        $this->r->send(new http\Client\Request("GET", "http://dev.iworks.at/ext-http/.cookie1.php"));
        $cookies = $this->r->getResponseMessage()->getCookies(0, array("bar"));
        $this->assertCount(1, $cookies);
        $cookies = $cookies[0];
        $this->assertEquals(array("bar"=>"foo"), $cookies->getExtras());
        $this->assertEquals(array("foo"=>"bar"), $cookies->getCookies());
        $cookies = $this->r->getResponseMessage()->getCookies(0, array("foo"));
        $this->assertCount(1, $cookies);
        $cookies = $cookies[0];
        $this->assertEquals(array("foo"=>"bar","bar"=>"foo"), $cookies->getCookies());
        $this->assertEquals(array(), $cookies->getExtras());
    }

    function testResetCookies() {
        $this->r->setRequest(new http\Client\Request("GET", "http://dev.iworks.at/ext-http/.cookie.php"));

        $this->r->enableCookies();
        $this->r->send(null);

        $f = function ($a) { return $a->getCookies(); };
        $c = array_map($f, $this->r->getResponseMessage()->getCookies());

        $this->r->send(null);
        $this->assertEquals($c, array_map($f, $this->r->getResponseMessage()->getCookies()));
        
        $this->r->resetCookies();
        $this->r->send(null);
        $this->assertNotEquals($c, array_map($f, $this->r->getResponseMessage()->getCookies()));
    }
    
    function testSsl() {
    	$this->r->setRequest(new http\Client\Request("GET", "https://twitter.com/"));
    	$this->r->setSslOptions(array("verify_peer" => true));
    	$this->r->addSslOptions(array("verify_host" => 2));
    	$this->assertEquals(
    		array(
    			"verify_peer" => true,
    			"verify_host" => 2,
    		),
    		$this->r->getSslOptions()
    	);
    	$this->r->send();
    	$ti = $this->r->getTransferInfo();
    	$this->assertArrayHasKey("ssl_engines", $ti);
    	$this->assertGreaterThan(0, count($ti["ssl_engines"]));	
    }

    function testHistory() {
        $body = new http\Message\Body;
        $body->append("foobar");

        $request = new http\Client\Request;
        $request->setBody($body);
        $request->setRequestMethod("POST");
        $request->setRequestUrl("http://dev.iworks.at/ext-http/.print_request.php");

        $this->r->recordHistory = true;
        $this->r->send($request);

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


        $this->r->send($request);

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

