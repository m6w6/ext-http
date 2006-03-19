--TEST--
PHPUnit HttpMessage
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpMessage');
skipif(!@include 'PHPUnit2/Framework/TestCase.php', 'need PHPUnit2');
?>
--FILE--
<?php
echo "-TEST\n";

error_reporting(E_ALL);
ini_set('html_errors', 0);

require_once 'PHPUnit2/Framework/TestSuite.php';
require_once 'PHPUnit2/Framework/TestCase.php';
require_once 'PHPUnit2/TextUI/ResultPrinter.php';

class HttpMessageTest extends PHPUnit2_Framework_TestCase
{
    function setUp()
    {
        $this->emptyMessage = new HttpMessage;
        $this->responseMessage = new HttpMessage("HTTP/1.1 302 Found\r\nLocation: /foo\r\nHTTP/1.1 200 Ok\r\nServer: Funky/1.0\r\n\r\nHi there!");
        $this->requestMessage = new HttpMessage("GET /foo HTTP/1.1\r\nHost: example.com\r\nContent-type: text/plain\r\nContent-length: 10\r\n\r\nHi there!\n");
    }
    
    function test___construct()
    {
    	$this->assertTrue(new HttpMessage instanceof HttpMessage, "new HttpMessage instanceof HttpMessage");
    }

    function test_getBody()
    {
        $this->assertEquals('', $this->emptyMessage->getBody());
        $this->assertEquals('Hi there!', $this->responseMessage->getBody());
        $this->assertEquals("Hi there!\n", $this->requestMessage->getBody());
    }

    function test_setBody()
    {
        $this->emptyMessage->setBody('New Body 1');
        $this->responseMessage->setBody('New Body 2');
        $this->requestMessage->setBody('New Body 3');
        $this->assertEquals('New Body 2', $this->responseMessage->getBody());
        $this->assertEquals('New Body 1', $this->emptyMessage->getBody());
        $this->assertEquals('New Body 3', $this->requestMessage->getBody());
    }

    function test_getHeaders()
    {
        $this->assertEquals(array(), $this->emptyMessage->getHeaders());
        $this->assertEquals(array('Server' => 'Funky/1.0'), $this->responseMessage->getHeaders());
        $this->assertEquals(array('Host' => 'example.com', 'Content-Type' => 'text/plain', 'Content-Length' => '10'), $this->requestMessage->getHeaders());
    }

    function test_setHeaders()
    {
        $this->emptyMessage->setHeaders(array('Foo' => 'Bar'));
        $this->responseMessage->setHeaders(array());
        $this->requestMessage->setHeaders(array('Host' => 'www.example.com'));
        $this->assertEquals(array('Foo' => 'Bar'), $this->emptyMessage->getHeaders());
        $this->assertEquals(array(), $this->responseMessage->getHeaders());
        $this->assertEquals(array('Host' => 'www.example.com'), $this->requestMessage->getHeaders());
    }

    function test_addHeaders()
    {
        $this->emptyMessage->addHeaders(array('Foo' => 'Bar'));
        $this->responseMessage->addHeaders(array('Date' => 'today'));
        $this->requestMessage->addHeaders(array('Host' => 'www.example.com'));
        $this->assertEquals(array('Foo' => 'Bar'), $this->emptyMessage->getHeaders());
        $this->assertEquals(array('Server' => 'Funky/1.0', 'Date' => 'today'), $this->responseMessage->getHeaders());
        $this->assertEquals(array('Host' => 'www.example.com', 'Content-Type' => 'text/plain', 'Content-Length' => '10'), $this->requestMessage->getHeaders());
        $this->emptyMessage->addHeaders(array('Foo' => 'Baz'), true);
        $this->assertEquals(array('Foo' => array('Bar', 'Baz')), $this->emptyMessage->getHeaders());
    }

    function test_getType()
    {
        $this->assertEquals(HTTP_MSG_NONE, $this->emptyMessage->getType());
        $this->assertEquals(HTTP_MSG_RESPONSE, $this->responseMessage->getType());
        $this->assertEquals(HTTP_MSG_REQUEST, $this->requestMessage->getType());
    }

    function test_setType()
    {
        $this->emptyMessage->setType(HTTP_MSG_RESPONSE);
        $this->responseMessage->setType(HTTP_MSG_REQUEST);
        $this->requestMessage->setType(HTTP_MSG_NONE);
        $this->assertEquals(HTTP_MSG_RESPONSE, $this->emptyMessage->getType());
        $this->assertEquals(HTTP_MSG_REQUEST, $this->responseMessage->getType());
        $this->assertEquals(HTTP_MSG_NONE, $this->requestMessage->getType());
    }

    function test_getResponseCode()
    {
        $this->assertFalse($this->emptyMessage->getResponseCode());
        $this->assertEquals(200, $this->responseMessage->getResponseCode());
        $this->assertFalse($this->requestMessage->getResponseCode());
    }

    function test_setResponseCode()
    {
        $this->assertFalse($this->emptyMessage->setResponseCode(301));
        $this->assertTrue($this->responseMessage->setResponseCode(301));
        $this->assertFalse($this->requestMessage->setResponseCode(301));
        $this->assertFalse($this->emptyMessage->getResponseCode());
        $this->assertEquals(301, $this->responseMessage->getResponseCode());
        $this->assertFalse($this->requestMessage->getResponseCode());
    }

    function test_getRequestMethod()
    {
        $this->assertFalse($this->emptyMessage->getRequestMethod());
        $this->assertFalse($this->responseMessage->getRequestMethod());
        $this->assertEquals('GET', $this->requestMessage->getRequestMethod());
    }

    function test_setRequestMethod()
    {
        $this->assertFalse($this->emptyMessage->setRequestMethod('POST'));
        $this->assertFalse($this->responseMessage->setRequestMethod('POST'));
        $this->assertTrue($this->requestMessage->setRequestMethod('POST'));
        $this->assertFalse($this->emptyMessage->getRequestMethod());
        $this->assertFalse($this->responseMessage->getRequestMethod());
        $this->assertEquals('POST', $this->requestMessage->getRequestMethod());
    }

    function test_getRequestUrl()
    {
        $this->assertFalse($this->emptyMessage->getRequestUrl());
        $this->assertFalse($this->responseMessage->getRequestUrl());
        $this->assertEquals('/foo', $this->requestMessage->getRequestUrl());
    }

    function test_setRequestUrl()
    {
        $this->assertFalse($this->emptyMessage->setRequestUrl('/bla'));
        $this->assertFalse($this->responseMessage->setRequestUrl('/bla'));
        $this->assertTrue($this->requestMessage->setRequestUrl('/bla'));
        $this->assertFalse($this->emptyMessage->getRequestUrl());
        $this->assertFalse($this->responseMessage->getRequestUrl());
        $this->assertEquals('/bla', $this->requestMessage->getRequestUrl());
    }

    function test_getHttpVersion()
    {
        $this->assertEquals('0.0', $this->emptyMessage->getHttpVersion());
        $this->assertEquals('1.1', $this->responseMessage->getHttpVersion());
        $this->assertEquals('1.1', $this->requestMessage->getHttpVersion());
    }

    function test_setHttpVersion()
    {
        $this->assertTrue($this->emptyMessage->setHttpVersion(1.0));
        $this->assertTrue($this->responseMessage->setHttpVersion(1.0));
        $this->assertTrue($this->requestMessage->setHttpVersion(1.0));
        $this->assertEquals('1.0', $this->emptyMessage->getHttpVersion());
        $this->assertEquals('1.0', $this->responseMessage->getHttpVersion());
        $this->assertEquals('1.0', $this->requestMessage->getHttpVersion());
    }

    function test_getParentMessage()
    {
        $this->assertTrue($this->responseMessage->getParentMessage() instanceOf HttpMessage);
        try {
            $this->requestMessage->getParentMessage();
            $this->assertTrue(false, "\$this->requestMessage->getParentMessage() did not throw an exception");
        } catch (HttpRuntimeException $ex) {
        }
    }

    function test_send()
    {
    }

    function test_toString()
    {
        $this->assertEquals('', $this->emptyMessage->toString());
        $this->assertEquals("HTTP/1.1 200 Ok\r\nServer: Funky/1.0\r\n\r\nHi there!\r\n", $this->responseMessage->toString());
        $this->assertEquals("GET /foo HTTP/1.1\r\nHost: example.com\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n\r\nHi there!\n\r\n", $this->requestMessage->toString());
    }

    function test_count()
    {
        if (5.1 <= (float)PHP_VERSION) {
            $this->assertEquals(1, count($this->emptyMessage));
            $this->assertEquals(2, count($this->responseMessage));
            $this->assertEquals(1, count($this->requestMessage));
        } else {
            $this->assertEquals(1, $this->emptyMessage->count());
            $this->assertEquals(2, $this->responseMessage->count());
            $this->assertEquals(1, $this->requestMessage->count());
        }
    }

    function test_serialize()
    {
    }

    function test_unserialize()
    {
    }

    function test___toString()
    {
        ob_start();
        echo $this->responseMessage;
        $this->assertEquals("HTTP/1.1 200 Ok\r\nServer: Funky/1.0\r\n\r\nHi there!\r\n", ob_get_clean());
    }

    function test_fromString()
    {
        $msg = HttpMessage::fromString("HTTP/1.1 200 Ok\r\nServer: Funky/1.0\r\n\r\nHi there!");
        $this->assertTrue($msg instanceOf HttpMessage);
        $this->assertEquals(HTTP_MSG_RESPONSE, $msg->getType());
        $this->assertEquals("Hi there!", $msg->getBody());
    }
}

$s = new PHPUnit2_Framework_TestSuite('HttpMessageTest');
$p = new PHPUnit2_TextUI_ResultPrinter();
$p->printResult($s->run(), 0);

echo "Done\n";
?>
--EXPECTF--
%sTEST

Notice: HttpMessage::getResponseCode(): HttpMessage is not of type HTTP_MSG_RESPONSE in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getResponseCode(): HttpMessage is not of type HTTP_MSG_RESPONSE in %sut_HttpMessage.php on line %d

Notice: HttpMessage::setResponseCode(): HttpMessage is not of type HTTP_MSG_RESPONSE in %sut_HttpMessage.php on line %d

Notice: HttpMessage::setResponseCode(): HttpMessage is not of type HTTP_MSG_RESPONSE in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getResponseCode(): HttpMessage is not of type HTTP_MSG_RESPONSE in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getResponseCode(): HttpMessage is not of type HTTP_MSG_RESPONSE in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestMethod(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestMethod(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::setRequestMethod(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::setRequestMethod(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestMethod(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestMethod(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestUrl(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestUrl(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::setRequestUrl(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::setRequestUrl(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestUrl(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d

Notice: HttpMessage::getRequestUrl(): HttpMessage is not of type HTTP_MSG_REQUEST in %sut_HttpMessage.php on line %d


Time: 0

OK (24 tests)
Done
