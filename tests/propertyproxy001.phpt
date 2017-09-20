--TEST--
property proxy
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

class m extends http\Message { 
    function test1() { 
        $this->headers["bykey"] = 1; 
        var_dump($this->headers); 
	}
	function test2() {
        $h = &$this->headers; 
        $h["by1ref"] = 2; 
        var_dump($this->headers); 
	}
	function test3() {
        $x = &$this->headers["byXref"];

        $h = &$this->headers["by2ref"]; 
        $h = 1; 
        var_dump($this->headers);

        $x = 2;
        var_dump($this->headers);
	}
	function test4() {
        $this->headers["bynext"][] = 1;
        $this->headers["bynext"][] = 2;
        $this->headers["bynext"][] = 3;
        var_dump($this->headers);
    }
} 

$m=new m; 
$m->test1(); 
$m->test2(); 
$m->test3(); 
$m->test4(); 
echo $m,"\n";

?>
DONE
--EXPECTF--
array(1) {
  ["bykey"]=>
  int(1)
}
array(2) {
  ["bykey"]=>
  int(1)
  ["by1ref"]=>
  int(2)
}
array(3) {
  ["bykey"]=>
  int(1)
  ["by1ref"]=>
  int(2)
  ["by2ref"]=>
  int(1)
}
array(4) {
  ["bykey"]=>
  int(1)
  ["by1ref"]=>
  int(2)
  ["by2ref"]=>
  int(1)
  ["byXref"]=>
  int(2)
}
array(5) {
  ["bykey"]=>
  int(1)
  ["by1ref"]=>
  int(2)
  ["by2ref"]=>
  int(1)
  ["byXref"]=>
  int(2)
  ["bynext"]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
  }
}
bykey: 1
by1ref: 2
by2ref: 1
byXref: 2
bynext: 1
bynext: 2
bynext: 3

DONE

