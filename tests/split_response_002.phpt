--TEST--
http_split_response() list bug (mem-leaks)
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "Try\n";
$data = "HTTP/1.1 200 Ok\r\nContent-Type: text/plain\r\nContent-Language: de-AT\r\nDate: Sat, 22 Jan 2005 18:10:02 GMT\r\n\r\nHallo Du!";
class t { 
	var $r = array(); 
	function fail($data) {
		list($this->r['headers'], $this->r['body']) = http_split_response($data);
	}
}

$t = new t;
$t->fail($data);
echo "Done\n";
?>
--EXPECTF--
%sTry
Done
