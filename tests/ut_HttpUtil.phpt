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

error_reporting(E_ALL);
ini_set('html_errors', 0);

require_once 'PHPUnit2/Framework/TestSuite.php';
require_once 'PHPUnit2/Framework/TestCase.php';
require_once 'PHPUnit2/TextUI/ResultPrinter.php';

class HttpUtilTest extends PHPUnit2_Framework_TestCase
{
    function test_date()
    {
    	$this->assertEquals('Thu, 01 Jan 1970 00:00:01 GMT', HttpUtil::date(1));
    }

    function test_buildUrl()
    {
    	$_SERVER['SERVER_NAME'] = 'www.example.com';
    	$this->assertEquals('http://www.example.com/test.php?foo=bar', HttpUtil::buildUrl('/test.php?foo=bar', array('port' => 80)));
    	$this->assertEquals('https://www.example.com/', HttpUtil::buildUrl('/', array('scheme' => 'https')));
    	$this->assertEquals('ftp://ftp.example.com/pub', HttpUtil::buildUrl('/pub', array('host' => 'ftp.example.com', 'port' => 21)));
    }

    function test_negotiateLanguage()
    {
    	$_SERVER['HTTP_ACCEPT_LANGUAGE'] = 'en, de;q=0.5, it-IT; q = 0.5   ';
    	$this->assertEquals('de', HttpUtil::negotiateLanguage(array('de','it'), $r));
    	$this->assertEquals(array('de'=>0.5,'it'=>0.45), $r);
    }

    function test_negotiateCharset()
    {
    	$_SERVER['HTTP_ACCEPT_CHARSET'] = '  iso-8859-1, Unicode ;q=0 , utf-8  ';
    	$this->assertEquals('iso-8859-1', HttpUtil::negotiateCharset(array('utf-8','iso-8859-1'), $r));
    	$this->assertEquals(array('iso-8859-1'=>1000.0,'utf-8'=>999.0), $r);
    }

    function test_negotiateContentType()
    {
    	$_SERVER['HTTP_ACCEPT'] = ' text/xml+xhtml, text/html;q = .9,  *';
    	$this->assertEquals('text/xml+xhtml', HttpUtil::negotiateContentType(array('text/xml+xhtml', 'text/html'), $r));
    	$this->assertEquals(array('text/xml+xhtml'=>1000.0,'text/html'=>0.9), $r);
    }

    function test_matchModified()
    {
    	$_SERVER['HTTP_IF_MODIFIED_SINCE'] = 'Fri, 02 Jan 1970 00:00:01 GMT';
    	$this->assertTrue(HttpUtil::matchModified(1));
    	$this->assertFalse(HttpUtil::matchModified(2*24*60*60+1));
    	unset($_SERVER['HTTP_IF_MODIFIED_SINCE']);
    	
    	$_SERVER['HTTP_IF_UNMODIFIED_SINCE'] = 'Fri, 02 Jan 1970 00:00:01 GMT';
    	$this->assertTrue(HttpUtil::matchModified(1, true));
    	$this->assertFalse(HttpUtil::matchModified(2*24*60*60+1, true));
    	unset($_SERVER['HTTP_IF_UNMODIFIED_SINCE']);
    }

    function test_matchEtag()
    {
    	$_SERVER['HTTP_IF_NONE_MATCH'] = '"abc"';
    	$this->assertTrue(HttpUtil::matchEtag('abc'));
    	$this->assertFalse(HttpUtil::matchEtag('ABC'));
    	unset($_SERVER['HTTP_IF_NONE_MATCH']);
    	
    	$_SERVER['HTTP_IF_MATCH'] = '"abc"';
    	$this->assertTrue(HttpUtil::matchEtag('abc', true));
    	$this->assertFalse(HttpUtil::matchEtag('ABC', true));
    	unset($_SERVER['HTTP_IF_MATCH']);
    	
    	$_SERVER['HTTP_IF_NONE_MATCH'] = '*';
    	$this->assertTrue(HttpUtil::matchEtag('abc'));
    	$this->assertTrue(HttpUtil::matchEtag('ABC'));
    	unset($_SERVER['HTTP_IF_NONE_MATCH']);
    	
    	$_SERVER['HTTP_IF_MATCH'] = '*';
    	$this->assertTrue(HttpUtil::matchEtag('abc', true));
    	$this->assertTrue(HttpUtil::matchEtag('ABC', true));
    	unset($_SERVER['HTTP_IF_MATCH']);
    }

    function test_matchRequestHeader()
    {
    	$_SERVER['HTTP_FOO'] = 'FoObAr';
    	$this->assertTrue(HttpUtil::matchRequestHeader('foo', 'foobar', false));
    	$this->assertTrue(HttpUtil::matchRequestHeader('foo', 'FoObAr', true));
    	$this->assertFalse(HttpUtil::matchRequestHeader('foo', 'foobar', true));
    }
    
    function test_zlib()
    {
    	if ($support = http_support(HTTP_SUPPORT_ENCODINGS)) {
    		$this->assertEquals(file_get_contents(__FILE__), http_inflate(http_deflate(file_get_contents(__FILE__), HTTP_DEFLATE_TYPE_GZIP)));
    		$this->assertEquals(file_get_contents(__FILE__), http_inflate(http_deflate(file_get_contents(__FILE__))));
		} else {
			$this->assertFalse($support);
		}
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

OK (9 tests)
Done
