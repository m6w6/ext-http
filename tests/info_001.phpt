--TEST--
invalid HTTP info
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php

try { 
    var_dump(new http\Message("GET HTTP/1.1"));
} catch (Exception $e) {
    echo $e, "\n";
}
try { 
    var_dump(new http\Message("GET HTTP/1.123"));
} catch (Exception $e) {
    echo $e, "\n";
}
try { 
    var_dump(new http\Message("GETHTTP/1.1"));
} catch (Exception $e) {
    echo $e, "\n";
}
var_dump(new http\Message("GET / HTTP/1.1"));
?>
DONE
--EXPECTF--
exception 'http\Exception\BadMessageException' with message 'Could not parse message: GET HTTP/1.1' in %s
Stack trace:
#0 %s: http\Message->__construct('GET HTTP/1.1')
#1 {main}
exception 'http\Exception\BadMessageException' with message 'Could not parse message: GET HTTP/1.123' in %s
Stack trace:
#0 %s: http\Message->__construct('GET HTTP/1.123')
#1 {main}
exception 'http\Exception\BadMessageException' with message 'Could not parse message: GETHTTP/1.1' in %s
Stack trace:
#0 %s: http\Message->__construct('GETHTTP/1.1')
#1 {main}
object(http\Message)#%d (9) {
  ["type":protected]=>
  int(1)
  ["body":protected]=>
  object(http\Message\Body)#%d (0) {
  }
  ["requestMethod":protected]=>
  string(3) "GET"
  ["requestUrl":protected]=>
  string(1) "/"
  ["responseStatus":protected]=>
  string(0) ""
  ["responseCode":protected]=>
  int(0)
  ["httpVersion":protected]=>
  string(3) "1.1"
  ["headers":protected]=>
  array(0) {
  }
  ["parentMessage":protected]=>
  NULL
}
DONE
