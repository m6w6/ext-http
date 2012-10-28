<?php

class PoolTest extends PHPUnit_Framework_TestCase
{
    /*function testStandard() {
        foreach (http\Client\Factory::getAvailableDrivers() as $driver) {
            $f = new http\Client\Factory(compact("driver"));
            $p = $f->createPool();

            $this->assertFalse($p->dns, "dns");
            $this->assertFalse($p->cookie, "cookie");
            $this->assertFalse($p->ssl, "ssl");

            $p->dns = true;
            $p->cookie = true;
            $p->ssl = true;

            $this->assertTrue($p->dns, "dns");
            $this->assertTrue($p->cookie, "cookie");
            $this->assertTrue($p->ssl, "ssl");
        }
    }*/

    function testAttach() {
        foreach (http\Client\Factory::getAvailableDrivers() as $driver) {
            $f = new http\Client\Factory(compact("driver"));
            
            $p = $f->createPool();
            $c = $f->createClient();
            
            $c->setRequest(new http\Client\Request("GET", "https://twitter.com/"));
            $p->attach($c);
            
            try {
            	$p->attach($c);
            } catch (http\Exception $e) {
            	$this->assertEquals("Could not attach request to pool: Invalid easy handle", $e->getMessage());
            }
            
            $cc = clone $c;
            $p->attach($cc);

            $this->assertEquals(2, count($p));
        }
    }
    
    function testCurl() {
    	$client = new http\Curl\Client;
    	$client->setRequest(new http\Client\Request("GET", "https://twitter.com/"));
    	$pool = new http\Curl\Client\Pool;
    	$pool->attach($client);
    	$pool->attach($client2 = clone $client);
    	$pool->attach($client3 = clone $client);
    	
    	$this->assertEquals(3, count($pool));
    	$pool->send();
    	
    	$pool->detach($client);
    	$this->assertEquals(2, count($pool));
    	
    	$pool->reset();
    	$this->assertEquals(0, count($pool));
    }
    
    function testCurlEvents() {
    	$client = new http\Curl\Client;
    	$pool = new http\Curl\Client\Pool;
    	
    	$client->setRequest(new http\Client\Request("GET", "https://twitter.com/"));
    	
    	$pool->attach($client);
    	$pool->attach(clone $client);
    	$pool->attach(clone $client);
    	
    	$pool->enableEvents();
    	$pool->send();
    }
}

