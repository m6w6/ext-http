<?php

class UserClient extends http\Client\AbstractClient {
	function send($request = null) {
	}
}
    	

class DataShareTest extends PHPUnit_Framework_TestCase
{
    function testStandard() {
        foreach (http\Client\Factory::getAvailableDrivers() as $driver) {
            $f = new http\Client\Factory(compact("driver"));
            $s = $f->createDataShare();

            $this->assertFalse($s->dns, "dns");
            $this->assertFalse($s->cookie, "cookie");
            $this->assertFalse($s->ssl, "ssl");

            $s->dns = true;
            $s->cookie = true;
            $s->ssl = true;

            $this->assertTrue($s->dns, "dns");
            $this->assertTrue($s->cookie, "cookie");
            $this->assertTrue($s->ssl, "ssl");
        }
    }

    function testAttach() {
        foreach (http\Client\Factory::getAvailableDrivers() as $driver) {
            $f = new http\Client\Factory(compact("driver"));
            $s = $f->createDataShare();
            $s->dns = $s->ssl = $s->cookie = true;
            $c = $f->createClient();
            $s->attach($c);
            $c->setRequest(new http\Client\Request("GET", "https://twitter.com/"));
            $s->attach($c);
            $cc = clone $c;
            $s->attach($cc);

            $this->assertEquals(3, count($s));
        }
    }
    
    function testCurl() {
    	$client = new http\Curl\Client;
    	$client->setRequest(new http\Client\Request("GET", "https://twitter.com/"));
    	$share = new http\Curl\Client\DataShare;
    	$share->ssl = $share->dns = $share->cookie = true;
    	$share->attach($client);
    	$share->attach($client2 = clone $client);
    	$share->attach($client3 = clone $client);
    	
    	$this->assertEquals(3, count($share));
    	$client->send();
    	$client2->send();
    	$client3->send();
    	
    	$share->detach($client);
    	$share->reset();
    }
    
    function testCurlIncompatible() {
    	$client = new UserClient;
    	$client->setRequest(new http\Client\Request("GET", "https://twitter.com"));

    	$share = new http\Curl\Client\DataShare;
		$this->setExpectedException("PHPUnit_Framework_Error_Warning");
    	$share->attach($client);
		$this->setExpectedException("PHPUnit_Framework_Error_Warning");
    	$share->detach($client);
    }
}

