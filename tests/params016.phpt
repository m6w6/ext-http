--TEST--
header params rfc5988
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$link = <<<EOF
<https://api.github.com/search/code?q=addClass+user%3Amozilla&page=2>; rel="next", <https://api.github.com/search/code?q=addClass+user%3Amozilla&page=34>; rel="last"
EOF;

$p = new http\Params($link, ",", ";", "=",
	http\Params::PARSE_RFC5988 | http\Params::PARSE_ESCAPED);
var_dump($p->params);
var_dump((string)$p);
?>
===DONE===
--EXPECT--
Test
array(2) {
  ["https://api.github.com/search/code?q=addClass+user%3Amozilla&page=2"]=>
  array(2) {
    ["value"]=>
    bool(true)
    ["arguments"]=>
    array(1) {
      ["rel"]=>
      string(4) "next"
    }
  }
  ["https://api.github.com/search/code?q=addClass+user%3Amozilla&page=34"]=>
  array(2) {
    ["value"]=>
    bool(true)
    ["arguments"]=>
    array(1) {
      ["rel"]=>
      string(4) "last"
    }
  }
}
string(162) "<https://api.github.com/search/code?q=addClass+user%3Amozilla&page=2>;rel="next",<https://api.github.com/search/code?q=addClass+user%3Amozilla&page=34>;rel="last""
===DONE===
