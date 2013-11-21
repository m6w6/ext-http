--TEST--
header match
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$ae = new http\Header("Accept-encoding", "gzip, deflate");
var_dump($ae->match("gzip", http\Header::MATCH_WORD));
var_dump($ae->match("gzip", http\Header::MATCH_WORD|http\Header::MATCH_CASE));
var_dump($ae->match("gzip", http\Header::MATCH_STRICT));
var_dump($ae->match("deflate", http\Header::MATCH_WORD));
var_dump($ae->match("deflate", http\Header::MATCH_WORD|http\Header::MATCH_CASE));
var_dump($ae->match("deflate", http\Header::MATCH_STRICT));
var_dump($ae->match("zip", http\Header::MATCH_WORD));
var_dump($ae->match("gzip", http\Header::MATCH_FULL));

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
Done
