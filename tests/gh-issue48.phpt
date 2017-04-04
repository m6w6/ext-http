--TEST--
url errors
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

function test($url, $flags = 0) {
	echo "# DEFAULT\n";
	try {
		echo new http\Url($url, null, $flags), "\n";
	} catch (Exception $e) {
		echo $e->getMessage(), "\n";
	} 
	
	echo "# IGNORE\n";
	echo new http\Url($url, null, $flags|http\Url::IGNORE_ERRORS), "\n";
	
	echo "# SILENT\n";
	echo new http\Url($url, null, $flags|http\Url::SILENT_ERRORS), "\n";
	
	echo "# IGNORE|SILENT\n";
	echo new http\Url($url, null, $flags|http\Url::IGNORE_ERRORS|http\Url::SILENT_ERRORS), "\n";
	echo "==========\n";
}

test("http://.foo.bar/?q=1");
test("http://..foo.bar/i.x");
test("http://foo..bar/i..x");
test("http://-foo.bar");
test("http://--foo.bar");
test("http://f--oo.bar");
test("htto://foo.bar/?q=%");
test("htto://foo.bar/?q=%", http\Url::PARSE_TOPCT);
test("http://a\xc3\xc3b");
test("http://[foobar]:123");
test("#/?foo=&#", http\Url::PARSE_MBUTF8);

?>

===DONE===
--EXPECTF--
Test
# DEFAULT
http\Url::__construct(): Failed to parse host; unexpected '.' at pos 0 in '.foo.bar/?q=1'
# IGNORE

Warning: http\Url::__construct(): Failed to parse host; unexpected '.' at pos 0 in '.foo.bar/?q=1' in %sgh-issue48.php on line %d
http://foo.bar/?q=1
# SILENT

# IGNORE|SILENT
http://foo.bar/?q=1
==========
# DEFAULT
http\Url::__construct(): Failed to parse host; unexpected '.' at pos 0 in '..foo.bar/i.x'
# IGNORE

Warning: http\Url::__construct(): Failed to parse host; unexpected '.' at pos 0 in '..foo.bar/i.x' in %sgh-issue48.php on line %d

Warning: http\Url::__construct(): Failed to parse host; unexpected '.' at pos 1 in '..foo.bar/i.x' in %sgh-issue48.php on line %d
http://foo.bar/i.x
# SILENT

# IGNORE|SILENT
http://foo.bar/i.x
==========
# DEFAULT
http\Url::__construct(): Failed to parse host; unexpected '.' at pos 4 in 'foo..bar/i..x'
# IGNORE

Warning: http\Url::__construct(): Failed to parse host; unexpected '.' at pos 4 in 'foo..bar/i..x' in %sgh-issue48.php on line %d
http://foo.bar/i..x
# SILENT

# IGNORE|SILENT
http://foo.bar/i..x
==========
# DEFAULT
http\Url::__construct(): Failed to parse host; unexpected '-' at pos 0 in '-foo.bar'
# IGNORE

Warning: http\Url::__construct(): Failed to parse host; unexpected '-' at pos 0 in '-foo.bar' in %sgh-issue48.php on line %d
http://foo.bar/
# SILENT

# IGNORE|SILENT
http://foo.bar/
==========
# DEFAULT
http\Url::__construct(): Failed to parse host; unexpected '-' at pos 0 in '--foo.bar'
# IGNORE

Warning: http\Url::__construct(): Failed to parse host; unexpected '-' at pos 0 in '--foo.bar' in %sgh-issue48.php on line %d

Warning: http\Url::__construct(): Failed to parse host; unexpected '-' at pos 1 in '--foo.bar' in %sgh-issue48.php on line %d
http://foo.bar/
# SILENT

# IGNORE|SILENT
http://foo.bar/
==========
# DEFAULT
http://f--oo.bar/
# IGNORE
http://f--oo.bar/
# SILENT
http://f--oo.bar/
# IGNORE|SILENT
http://f--oo.bar/
==========
# DEFAULT
http\Url::__construct(): Failed to parse query; invalid percent encoding at pos 2 in 'q=%'
# IGNORE

Warning: http\Url::__construct(): Failed to parse query; invalid percent encoding at pos 2 in 'q=%' in %sgh-issue48.php on line %d
htto://foo.bar/?q=%
# SILENT

# IGNORE|SILENT
htto://foo.bar/?q=%
==========
# DEFAULT
http\Url::__construct(): Failed to parse query; invalid percent encoding at pos 2 in 'q=%'
# IGNORE

Warning: http\Url::__construct(): Failed to parse query; invalid percent encoding at pos 2 in 'q=%' in %sgh-issue48.php on line %d
htto://foo.bar/?q=%25
# SILENT

# IGNORE|SILENT
htto://foo.bar/?q=%25
==========
# DEFAULT
http\Url::__construct(): Failed to parse hostinfo; unexpected byte 0xc3 at pos 1 in 'a%c%cb'
# IGNORE

Warning: http\Url::__construct(): Failed to parse hostinfo; unexpected byte 0xc3 at pos 1 in 'a%c%cb' in %sgh-issue48.php on line %d

Warning: http\Url::__construct(): Failed to parse hostinfo; unexpected byte 0xc3 at pos 2 in 'a%c%cb' in %sgh-issue48.php on line %d
http://a%r\xc3\xc3%rb/
# SILENT

# IGNORE|SILENT
http://a%r\xc3\xc3%rb/
==========
# DEFAULT
http\Url::__construct(): Failed to parse hostinfo; unexpected '[' at pos 0 in '[foobar]:123'
# IGNORE

Warning: http\Url::__construct(): Failed to parse hostinfo; unexpected '[' at pos 0 in '[foobar]:123' in %sgh-issue48.php on line %d

Warning: http\Url::__construct(): Failed to parse hostinfo; unexpected byte 0x5b at pos 0 in '[foobar]:123' in %sgh-issue48.php on line %d

Warning: http\Url::__construct(): Failed to parse hostinfo; unexpected byte 0x5d at pos 7 in '[foobar]:123' in %sgh-issue48.php on line %d
http://[foobar]:123/
# SILENT

# IGNORE|SILENT
http://[foobar]:123/
==========
# DEFAULT
http\Url::__construct(): Failed to parse fragment; invalid fragment identifier at pos 7 in '/?foo=&#'
# IGNORE

Warning: http\Url::__construct(): Failed to parse fragment; invalid fragment identifier at pos 7 in '/?foo=&#' in %sgh-issue48.php on line %d
#/?foo=&#
# SILENT

# IGNORE|SILENT
#/?foo=&#
==========

===DONE===
