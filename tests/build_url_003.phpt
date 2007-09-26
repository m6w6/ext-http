--TEST--
http_build_url()
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--ENV--
HTTP_HOST=www.example.com
--FILE--
<?php
$url = '/path/?query#anchor';
echo "-TEST\n";
printf("-%s-\n", http_build_url($url));
printf("-%s-\n", http_build_url($url, array('scheme' => 'https')));
printf("-%s-\n", http_build_url($url, array('scheme' => 'https', 'host' => 'ssl.example.com')));
printf("-%s-\n", http_build_url($url, array('scheme' => 'ftp', 'host' => 'ftp.example.com', 'port' => 21)));
echo "Done\n";
?>
--EXPECTF--
%sTEST
-http://www.example.com/path/?query#anchor-
-https://www.example.com/path/?query#anchor-
-https://ssl.example.com/path/?query#anchor-
-ftp://ftp.example.com/path/?query#anchor-
Done
