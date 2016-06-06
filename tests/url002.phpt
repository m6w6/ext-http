--TEST--
url properties
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";
$u = "http://user:pass@www.example.com:8080/path/file.ext".
			"?foo=bar&more[]=1&more[]=2#hash";

var_dump($u === (string) new http\Url($u));

$url = new http\Url($u, 
	array("path" => "changed", "query" => "bar&foo=&added=this"), 
	http\Url::JOIN_PATH |
	http\Url::JOIN_QUERY |
	http\Url::STRIP_AUTH |
	http\Url::STRIP_FRAGMENT
);

var_dump($url->scheme);
var_dump($url->user);
var_dump($url->pass);
var_dump($url->host);
var_dump($url->port);
var_dump($url->path);
var_dump($url->query);
var_dump($url->fragment);

?>
DONE
--EXPECT--
Test
bool(true)
string(4) "http"
NULL
NULL
string(15) "www.example.com"
int(8080)
string(13) "/path/changed"
string(47) "foo=&more%5B0%5D=1&more%5B1%5D=2&bar&added=this"
NULL
DONE
