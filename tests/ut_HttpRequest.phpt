--TEST--
PHPUnit HttpRequest
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequest');
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

class HttpRequestTest extends PHPUnit2_Framework_TestCase
{
    function test___construct()
    {
    }

    function test___destruct()
    {
    }

    function test_setOptions()
    {
    }

    function test_getOptions()
    {
    }

    function test_setSslOptions()
    {
    }

    function test_getSslOptions()
    {
    }

    function test_addHeaders()
    {
    }

    function test_getHeaders()
    {
    }

    function test_setHeaders()
    {
    }

    function test_addCookies()
    {
    }

    function test_getCookies()
    {
    }

    function test_setCookies()
    {
    }

    function test_setMethod()
    {
    }

    function test_getMethod()
    {
    }

    function test_setUrl()
    {
    }

    function test_getUrl()
    {
    }

    function test_setContentType()
    {
    }

    function test_getContentType()
    {
    }

    function test_setQueryData()
    {
    }

    function test_getQueryData()
    {
    }

    function test_addQueryData()
    {
    }

    function test_setPostFields()
    {
    }

    function test_getPostFields()
    {
    }

    function test_addPostFields()
    {
    }

    function test_setRawPostData()
    {
    }

    function test_getRawPostData()
    {
    }

    function test_addRawPostData()
    {
    }

    function test_setPostFiles()
    {
    }

    function test_addPostFile()
    {
    }

    function test_getPostFiles()
    {
    }

    function test_setPutFile()
    {
    }

    function test_getPutFile()
    {
    }

    function test_send()
    {
    }

    function test_getResponseData()
    {
    }

    function test_getResponseHeader()
    {
    }

    function test_getResponseCookie()
    {
    }

    function test_getResponseCode()
    {
    }

    function test_getResponseBody()
    {
    }

    function test_getResponseInfo()
    {
    }

    function test_getResponseMessage()
    {
    }

    function test_getRequestMessage()
    {
    }

    function test_getHistory()
    {
    }

    function test_clearHistory()
    {
    }

    function test_get()
    {
    }

    function test_head()
    {
    }

    function test_postData()
    {
    }

    function test_postFields()
    {
    }

    function test_putFile()
    {
    }

    function test_putStream()
    {
    }

    function test_methodRegister()
    {
    }

    function test_methodUnregister()
    {
    }

    function test_methodName()
    {
    }

    function test_methodExists()
    {
    }


}

$s = new PHPUnit2_Framework_TestSuite('HttpRequestTest');
$p = new PHPUnit2_TextUI_ResultPrinter();
$p->printResult($s->run(), 0);

echo "Done\n";
?>
--EXPECTF--
%sTEST


Time: 0

OK (53 tests)
Done
