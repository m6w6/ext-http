--TEST--
header parser errors
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 
echo "Test\n";

$headers = array(
	"Na\0me: value",
	"Na\nme: value",
	"Name:\0value",
	"Name:\nvalue",
	"Name: val\0ue",
	"Name: value\0",
);

foreach ($headers as $header) {
	$parsed = null;
	$parser = new http\Header\Parser;
	var_dump($parser->parse($header, http\Header\Parser::CLEANUP, $parsed), $parsed);
}
?>
===DONE===
--EXPECTF--
Test

Warning: http\Header\Parser::parse(): Failed to parse headers: unexpected character '\000' at pos 2 of 'Na\000me' in %sheaderparser002.php on line %d
int(-1)
array(0) {
}

Warning: http\Header\Parser::parse(): Failed to parse headers: unexpected end of line at pos 2 of 'Na\nme: value' in %sheaderparser002.php on line %d
int(-1)
array(0) {
}

Warning: http\Header\Parser::parse(): Failed to parse headers: unexpected character '\000' at pos 0 of '\000value' in %sheaderparser002.php on line %d
int(-1)
array(0) {
}

Warning: http\Header\Parser::parse(): Failed to parse headers: unexpected end of input at pos 5 of 'value' in %sheaderparser002.php on line %d
int(-1)
array(0) {
}

Warning: http\Header\Parser::parse(): Failed to parse headers: unexpected character '\000' at pos 3 of 'val\000ue' in %sheaderparser002.php on line %d
int(-1)
array(0) {
}

Warning: http\Header\Parser::parse(): Failed to parse headers: unexpected character '\000' at pos 5 of 'value\000' in %sheaderparser002.php on line %d
int(-1)
array(0) {
}
===DONE===