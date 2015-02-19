--TEST--
header parser
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$headers = array(
	"One: ","header\n",
	"Two: header\n\tlines\n",
	"Three",": header\n lines\n here\n",
	"More: than one header\n",
	"More: ", "than: ", "you: ", "expect\n",
	"\n",
);

$states = array(-1=>"FAILURE",0=>"START","KEY","VALUE","VALUE_EX","HEADER_DONE","DONE");
$parser = new http\Header\Parser;
do {
	$state = $parser->parse($part = array_shift($headers), 
		$headers ? 0 : http\Header\Parser::CLEANUP, 
		$result);
	printf("%2\$-32s | %1\$s\n", $states[$state], addcslashes($part, "\r\n\t\0"));
} while ($headers && $state !== http\Header\Parser::STATE_FAILURE);

var_dump($result);

?>
===DONE===
--EXPECT--
Test
One:                             | VALUE
header\n                         | VALUE_EX
Two: header\n\tlines\n           | VALUE_EX
Three                            | KEY
: header\n lines\n here\n        | VALUE_EX
More: than one header\n          | VALUE_EX
More:                            | VALUE
than:                            | VALUE
you:                             | VALUE
expect\n                         | VALUE_EX
\n                               | DONE
array(4) {
  ["One"]=>
  string(6) "header"
  ["Two"]=>
  string(12) "header lines"
  ["Three"]=>
  string(17) "header lines here"
  ["More"]=>
  array(2) {
    [0]=>
    string(15) "than one header"
    [1]=>
    string(17) "than: you: expect"
  }
}
===DONE===
