--TEST--
header params rfc5987
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$p = new http\Params("attachment; filename*=IsO-8859-1''d%f6ner.pdf");
var_dump($p->params, (string) $p);
$p = new http\Params("bar; title*=iso-8859-1'en'%A3%20rates");
var_dump($p->params, (string) $p);
$p = new http\Params("bar; title*=UTF-8''%c2%a3%20and%20%e2%82%ac%20rates");
var_dump($p->params, (string) $p);

?>
===DONE===
--EXPECT--
Test
array(1) {
  ["attachment"]=>
  array(2) {
    ["value"]=>
    bool(true)
    ["arguments"]=>
    array(1) {
      ["*rfc5987*"]=>
      array(1) {
        ["filename"]=>
        array(1) {
          [""]=>
          string(10) "döner.pdf"
        }
      }
    }
  }
}
string(42) "attachment;filename*=utf-8''d%C3%B6ner.pdf"
array(1) {
  ["bar"]=>
  array(2) {
    ["value"]=>
    bool(true)
    ["arguments"]=>
    array(1) {
      ["*rfc5987*"]=>
      array(1) {
        ["title"]=>
        array(1) {
          ["en"]=>
          string(8) "£ rates"
        }
      }
    }
  }
}
string(34) "bar;title*=utf-8'en'%C2%A3%20rates"
array(1) {
  ["bar"]=>
  array(2) {
    ["value"]=>
    bool(true)
    ["arguments"]=>
    array(1) {
      ["*rfc5987*"]=>
      array(1) {
        ["title"]=>
        array(1) {
          [""]=>
          string(16) "£ and € rates"
        }
      }
    }
  }
}
string(50) "bar;title*=utf-8''%C2%A3%20and%20%E2%82%AC%20rates"
===DONE===
