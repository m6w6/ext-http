--TEST--
GZIP request
--SKIPIF--
<?php
include 'skip.inc';
checkurl('dev.iworks.at');
skipif(!http_support(HTTP_SUPPORT_REQUESTS), 'need curl support');
?>
--FILE--
<?php
echo "-TEST\n";

var_dump(http_parse_message(http_get('http://dev.iworks.at/ext-http/.print_request.php?gzip=1', array('compress' => true))));

echo "Done\n";
--EXPECTF--
%sTEST
object(stdClass)#%d (%d) {
  ["type"]=>
  int(2)
  ["httpVersion"]=>
  float(1.1)
  ["responseCode"]=>
  int(200)
  ["responseStatus"]=>
  string(2) "OK"
  ["headers"]=>
  array(8) {
    %s
    ["Vary"]=>
    string(15) "Accept-Encoding"
    ["Content-Length"]=>
    string(2) "26"
    ["Content-Type"]=>
    string(9) "text/html"
    ["X-Original-Content-Encoding"]=>
    string(4) "gzip"
    ["X-Original-Content-Length"]=>
    string(2) "51"
  }
  ["body"]=>
  string(26) "Array
(
    [gzip] => 1
)
"
  ["parentMessage"]=>
  NULL
}
Done

