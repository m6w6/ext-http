<?php

class ProgressObserver1 implements SplObserver {
    function update(SplSubject $c, $r = null) {
        if ($c->getProgressInfo($r)) $c->pi .= "-";
    }
}
class ProgressObserver2 implements SplObserver {
    function update(SplSubject $c, $r = null) {
        if ($c->getProgressInfo($r)) $c->pi .= ".";
    }
}
class CallbackObserver implements SplObserver {
    public $callback;
    function __construct($callback) {
        $this->callback = $callback;
    }
    function update(SplSubject $c, $r = null) {
        call_user_func($this->callback, $c, $r);
    } 
}

class RequestTest extends PHPUnit_Framework_TestCase
{
    /**
     * @var http\Client
     */
    protected $r;

    function setUp() {
        $this->r = new http\Client;
        $this->r->pi = "";
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

    function testObserver() {
		$test = $this;
        $this->r->attach($o1 = new ProgressObserver1);
        $this->r->attach($o2 = new ProgressObserver2);
        $this->r->attach(
            $o3 = new CallbackObserver(
                function ($c, $r) use ($test) {
                    $p = (array) $c->getProgressInfo($r);
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
        $this->r->enqueue(new http\Client\Request("GET", "http://dev.iworks.at/ext-http/"))->send();
        $this->assertRegexp("/(\.-)+/", $this->r->pi);
        $this->assertCount(3, $this->r->getObservers());
        $this->r->detach($o1);
        $this->assertCount(2, $this->r->getObservers());
        $this->r->detach($o2);
        $this->assertCount(1, $this->r->getObservers());
        $this->r->detach($o3);
        $this->assertCount(0, $this->r->getObservers());
    }

    function testSsl() {
    	$this->r->setSslOptions(array("verify_peer" => true));
    	$this->r->addSslOptions(array("verify_host" => 2));
    	$this->assertEquals(
    		array(
    			"verify_peer" => true,
    			"verify_host" => 2,
    		),
    		$this->r->getSslOptions()
    	);
    	$this->r->enqueue($req = new http\Client\Request("GET", "https://twitter.com/"));
    	$this->r->send();
    	$ti = $this->r->getTransferInfo($req);
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
        $this->r->enqueue($request)->send();

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


        $this->r->requeue($request)->send();

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

