--TEST--
PHPUnit HttpRequestPool
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequestPool');
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

class HttpRequestPoolTest extends PHPUnit2_Framework_TestCase
{
    function test___construct()
    {
    }

    function test___destruct()
    {
    }

    function test_attach()
    {
    }

    function test_detach()
    {
    }

    function test_send()
    {
    }

    function test_reset()
    {
    }

    function test_socketPerform()
    {
    }

    function test_socketSelect()
    {
    }

    function test_valid()
    {
    }

    function test_current()
    {
    }

    function test_key()
    {
    }

    function test_next()
    {
    }

    function test_rewind()
    {
    }

    function test_count()
    {
    }

    function test_getAttachedRequests()
    {
    }

    function test_getFinishedRequests()
    {
    }


}

$s = new PHPUnit2_Framework_TestSuite('HttpRequestPoolTest');
$p = new PHPUnit2_TextUI_ResultPrinter();
$p->printResult($s->run(), 0);

echo "Done\n";
?>
--EXPECTF--
%sTEST


Time: 0

OK (16 tests)
Done
