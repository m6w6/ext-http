--TEST--
http_build_url() with parse_url()
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
echo http_build_url(parse_url("http://example.org/orig?q=1#f"), 
	parse_url("https://www.example.com:9999/replaced#n")), "\n";
echo http_build_url(("http://example.org/orig?q=1#f"), 
	("https://www.example.com:9999/replaced#n"), 0, $u), "\n";
print_r($u);
echo "Done\n";
?>
--EXPECTF--
%aTEST
https://www.example.com:9999/replaced?q=1#n
https://www.example.com:9999/replaced?q=1#n
Array
(
    [scheme] => https
    [host] => www.example.com
    [port] => 9999
    [path] => /replaced
    [query] => q=1
    [fragment] => n
)
Done
