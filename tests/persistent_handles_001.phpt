--TEST--
persistent handles
--SKIPIF--
<?php
include 'skip.inc';
skipif(!defined("HTTP_SUPPORT_PERSISTENCE") || !http_support(HTTP_SUPPORT_PERSISTENCE), "need persistent handle support");
?>
--INI--
http.persistent.handles.ident=GLOBAL
--FILE--
<?php
echo "-TEST\n";

echo "No free handles:\n";
var_dump(http_persistent_handles_count());
http_get("http://www.google.com/", null, $info[]);
echo "One free request handle within GLOBAL:\n";
var_dump(http_persistent_handles_count()->http_request["GLOBAL"]);
echo "Reusing request handle:\n";
http_get("http://www.google.com/", null, $info[]);
var_dump($info[0]["pretransfer_time"] > 100 * $info[1]["pretransfer_time"], $info[0]["pretransfer_time"], $info[1]["pretransfer_time"]);
echo "Handles' been cleaned up:\n";
#http_persistent_handles_clean();
var_dump(http_persistent_handles_count());
echo "Done\n";
?>
--EXPECTF--
%sTEST
No free handles:
object(stdClass)#%d (%d) {
  ["http_request_pool"]=>
  array(0) {
  }
  ["http_request"]=>
  array(0) {
  }
  ["http_request_datashare"]=>
  array(0) {
  }
  ["http_request_datashare_lock"]=>
  array(0) {
  }
}
One free request handle within GLOBAL:
int(1)
Reusing request handle:
bool(true)
float(%f)
float(%f)
Handles' been cleaned up:
object(stdClass)#%d (%d) {
  ["http_request_pool"]=>
  array(0) {
  }
  ["http_request"]=>
  array(1) {
    ["GLOBAL"]=>
    int(1)
  }
  ["http_request_datashare"]=>
  array(0) {
  }
  ["http_request_datashare_lock"]=>
  array(0) {
  }
}
Done
