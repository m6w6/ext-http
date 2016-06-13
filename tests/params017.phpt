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
Link: </TheBook/chapter2>;
         rel="previous"; title*=UTF-8'de'letztes%20Kapitel,
         </TheBook/chapter4>;
         rel="next"; title*=UTF-8'de'n%c3%a4chstes%20Kapitel
EOF;

$p = current(http\Header::parse($link, "http\\Header"))->getParams(
	http\Params::DEF_PARAM_SEP,
	http\Params::DEF_ARG_SEP,
	http\Params::DEF_VAL_SEP,
	http\Params::PARSE_RFC5987 | http\Params::PARSE_RFC5988 | http\Params::PARSE_ESCAPED
);
var_dump($p->params);
var_dump((string)$p);
?>
===DONE===
--EXPECTF--
Test
array(2) {
  ["/TheBook/chapter2"]=>
  array(2) {
    ["value"]=>
    bool(true)
    ["arguments"]=>
    array(2) {
      ["rel"]=>
      string(8) "previous"
      ["*rfc5987*"]=>
      array(1) {
        ["title"]=>
        array(1) {
          ["de"]=>
          string(15) "letztes Kapitel"
        }
      }
    }
  }
  ["/TheBook/chapter4"]=>
  array(2) {
    ["value"]=>
    bool(true)
    ["arguments"]=>
    array(2) {
      ["rel"]=>
      string(4) "next"
      ["*rfc5987*"]=>
      array(1) {
        ["title"]=>
        array(1) {
          ["de"]=>
          string(17) "nächstes Kapitel"
        }
      }
    }
  }
}
string(139) "</TheBook/chapter2>;rel="previous";title*=utf-8'de'letztes%20Kapitel,</TheBook/chapter4>;rel="next";title*=utf-8'de'n%C3%A4chstes%20Kapitel"
===DONE===
