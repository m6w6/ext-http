--TEST--
query string
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$s = "a=b&r[]=0&r[]=1&r[]=2&rr[][]=00&rr[][]=01&1=2";
$e = "a=b&r%5B0%5D=0&r%5B1%5D=1&r%5B2%5D=2&rr%5B0%5D%5B0%5D=00&rr%5B0%5D%5B1%5D=01&1=2";
$q = new http\QueryString($s);

var_dump($e === (string) $q);
var_dump($e === $q->get());

printf("Get defval\n");
var_dump("nonexistant" === $q->get("unknown", "s", "nonexistant"));
var_dump(null === $q->get("unknown"));

printf("Get 'a'\n");
var_dump("b" === $q->get("a"));
var_dump(0 === $q->get("a", "i"));
var_dump(array("b") === $q->get("a", "a"));
var_dump((object)array("scalar" => "b") == $q->get("a", "o"));

printf("Get 'r'\n");
var_dump(array("0","1","2") === $q->get("r"));

printf("Get 'rr'\n");
var_dump(array(array("00","01")) === $q->get("rr"));

printf("Get 1\n");
var_dump(2 == $q->get(1));
var_dump("2" === $q->get(1, "s"));
var_dump(2.0 === $q->get(1, "f"));
var_dump($q->get(1, "b"));

printf("Del 'a'\n");
var_dump("b" === $q->get("a", http\QueryString::TYPE_STRING, null, true));
var_dump(null === $q->get("a"));

printf("Del all\n");
$q->set(array("a" => null, "r" => null, "rr" => null, 1 => null));
var_dump("" === $q->toString());

$q = new http\QueryString($s);

printf("QSO\n");
var_dump($e === (string) new http\QueryString($q));
var_dump(http_build_query(array("e"=>$q->toArray())) === (string) new http\QueryString(array("e" => $q)));

printf("Iterator\n");
var_dump($q->toArray() === iterator_to_array($q));

printf("Serialize\n");
var_dump($e === (string) unserialize(serialize($q)));

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
Get defval
bool(true)
bool(true)
Get 'a'
bool(true)
bool(true)
bool(true)
bool(true)
Get 'r'
bool(true)
Get 'rr'
bool(true)
Get 1
bool(true)
bool(true)
bool(true)
bool(true)
Del 'a'
bool(true)
bool(true)
Del all
bool(true)
QSO
bool(true)
bool(true)
Iterator
bool(true)
Serialize
bool(true)
DONE
