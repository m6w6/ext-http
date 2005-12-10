--TEST--
PHPUnit HttpResponse
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpResponse');
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

class HttpResponseTest extends PHPUnit2_Framework_TestCase
{
    function test_setHeader()
    {
    }

    function test_getHeader()
    {
    }

    function test_setETag()
    {
    }

    function test_getETag()
    {
    }

    function test_setLastModified()
    {
    }

    function test_getLastModified()
    {
    }

    function test_setContentDisposition()
    {
    }

    function test_getContentDisposition()
    {
    }

    function test_setContentType()
    {
    }

    function test_getContentType()
    {
    }

    function test_guessContentType()
    {
    }

    function test_setCache()
    {
    }

    function test_getCache()
    {
    }

    function test_setCacheControl()
    {
    }

    function test_getCacheControl()
    {
    }

    function test_setGzip()
    {
    }

    function test_getGzip()
    {
    }

    function test_setThrottleDelay()
    {
    }

    function test_getThrottleDelay()
    {
    }

    function test_setBufferSize()
    {
    }

    function test_getBufferSize()
    {
    }

    function test_setData()
    {
    }

    function test_getData()
    {
    }

    function test_setFile()
    {
    }

    function test_getFile()
    {
    }

    function test_setStream()
    {
    }

    function test_getStream()
    {
    }

    function test_send()
    {
    }

    function test_capture()
    {
    }

    function test_redirect()
    {
    }

    function test_status()
    {
    }

    function test_getRequestHeaders()
    {
    }

    function test_getRequestBody()
    {
    }


}

$s = new PHPUnit2_Framework_TestSuite('HttpResponseTest');
$p = new PHPUnit2_TextUI_ResultPrinter();
$p->printResult($s->run(), 0);

echo "Done\n";
?>
--EXPECTF--
%sTEST


Time: 0

OK (33 tests)
Done
