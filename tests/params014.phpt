--TEST--
header params rfc5987
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";
$u = urlencode("ü");
$s = urlencode("ß");
$t = "p1*=utf-8''s$u$s,p2*=utf-8''hei$s;a1*=utf-8''a$s;a2*=utf-8''e$s;a3=no,p3=not";
$p = new http\Params($t);
var_dump($p->params);
var_dump((string)$p === $t, (string)$p, $t);
?>
===DONE===
--EXPECT--
Test
array(3) {
  ["p1"]=>
  array(2) {
    ["*rfc5987*"]=>
    array(1) {
      [""]=>
      string(5) "süß"
    }
    ["arguments"]=>
    array(0) {
    }
  }
  ["p2"]=>
  array(2) {
    ["*rfc5987*"]=>
    array(1) {
      [""]=>
      string(5) "heiß"
    }
    ["arguments"]=>
    array(2) {
      ["*rfc5987*"]=>
      array(2) {
        ["a1"]=>
        array(1) {
          [""]=>
          string(3) "aß"
        }
        ["a2"]=>
        array(1) {
          [""]=>
          string(3) "eß"
        }
      }
      ["a3"]=>
      string(2) "no"
    }
  }
  ["p3"]=>
  array(2) {
    ["value"]=>
    string(3) "not"
    ["arguments"]=>
    array(0) {
    }
  }
}
bool(true)
string(96) "p1*=utf-8''s%C3%BC%C3%9F,p2*=utf-8''hei%C3%9F;a1*=utf-8''a%C3%9F;a2*=utf-8''e%C3%9F;a3=no,p3=not"
string(96) "p1*=utf-8''s%C3%BC%C3%9F,p2*=utf-8''hei%C3%9F;a1*=utf-8''a%C3%9F;a2*=utf-8''e%C3%9F;a3=no,p3=not"
===DONE===
