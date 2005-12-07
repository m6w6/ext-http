--TEST--
PHPUnit HttpUtil
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpUtil');
skipif(!@include 'PHPUnit2/Framework/TestCase.php', 'need PHPUnit2');
?>
--FILE--
<?php
echo "-TEST\n";

require_once 'PHPUnit2/Framework/TestSuite.php';
require_once 'PHPUnit2/Framework/TestCase.php';
require_once 'PHPUnit2/TextUI/ResultPrinter.php';

class HttpUtilTest extends PHPUnit2_Framework_TestCase
{
    function test_date()
    {
    }

    function test_buildUri()
    {
    }

    function test_negotiateLanguage()
    {
    }

    function test_negotiateCharset()
    {
    }

    function test_negotiateContentType()
    {
    }

    function test_matchModified()
    {
    }

    function test_matchEtag()
    {
    }

    function test_matchRequestHeader()
    {
    }

    function test_parseMessage()
    {
    }

    function test_parseHeaders()
    {
    }

    function test_chunkedDecode()
    {
    }

    function test_gzEncode()
    {
    }

    function test_gzDecode()
    {
    }

    function test_deflate()
    {
    }

    function test_inflate()
    {
    }

    function test_compress()
    {
    }

    function test_uncompress()
    {
    }

    function test_support()
    {
    }


}

$s = new PHPUnit2_Framework_TestSuite('HttpUtilTest');
$p = new PHPUnit2_TextUI_ResultPrinter();
$p->printResult($s->run(), 0);

echo "Done\n";
?>
--EXPECTF--
%sTEST

Time: 0

OK (18 tests)
Done
