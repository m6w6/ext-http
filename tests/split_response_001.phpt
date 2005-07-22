--TEST--
http_split_response()
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
$data = "HTTP/1.1 200 Ok\r\nContent-Type: text/plain\r\nContent-Language: de-AT\r\nDate: Sat, 22 Jan 2005 18:10:02 GMT\r\n\r\nHallo Du!";
var_export(http_split_response($data));
echo "\nDone\n";
?>
--EXPECTF--
%sarray (
  0 => 
  array (
    'Response Status' => '200 Ok',
    'Content-Type' => 'text/plain',
    'Content-Language' => 'de-AT',
    'Date' => 'Sat, 22 Jan 2005 18:10:02 GMT',
  ),
  1 => 'Hallo Du!',
)
Done

