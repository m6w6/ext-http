--TEST--
HttpRequest XMLRPC
--SKIPIF--
<?php
include 'skip.inc';
checkext('xmlrpc');
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";

$r = new HttpRequest('http://dev.iworks.at/.print_request.php', HTTP_METH_POST);
$r->setContentType('text/xml');
$r->setBody(xmlrpc_encode_request('testMethod', array('foo' => 'bar')));
var_dump($r->send());
var_dump($r->send());
var_dump($r->send());

echo "Done\n";
?>
--EXPECTF--
%sTEST
object(HttpMessage)#%d (%d) {
  ["type:protected"]=>
  int(2)
  ["body:protected"]=>
  string(309) "string(294) "<?xml version="1.0" encoding="iso-8859-1"?>
<methodCall>
<methodName>testMethod</methodName>
<params>
 <param>
  <value>
   <struct>
    <member>
     <name>foo</name>
     <value>
      <string>bar</string>
     </value>
    </member>
   </struct>
  </value>
 </param>
</params>
</methodCall>
"
"
  ["requestMethod:protected"]=>
  string(0) ""
  ["requestUrl:protected"]=>
  string(0) ""
  ["responseStatus:protected"]=>
  string(2) "OK"
  ["responseCode:protected"]=>
  int(200)
  ["httpVersion:protected"]=>
  float(1.1)
  ["headers:protected"]=>
  array(6) {
    %s
  }
  ["parentMessage:protected"]=>
  NULL
}
object(HttpMessage)#%d (%d) {
  ["type:protected"]=>
  int(2)
  ["body:protected"]=>
  string(309) "string(294) "<?xml version="1.0" encoding="iso-8859-1"?>
<methodCall>
<methodName>testMethod</methodName>
<params>
 <param>
  <value>
   <struct>
    <member>
     <name>foo</name>
     <value>
      <string>bar</string>
     </value>
    </member>
   </struct>
  </value>
 </param>
</params>
</methodCall>
"
"
  ["requestMethod:protected"]=>
  string(0) ""
  ["requestUrl:protected"]=>
  string(0) ""
  ["responseStatus:protected"]=>
  string(2) "OK"
  ["responseCode:protected"]=>
  int(200)
  ["httpVersion:protected"]=>
  float(1.1)
  ["headers:protected"]=>
  array(6) {
    %s
  }
  ["parentMessage:protected"]=>
  NULL
}
object(HttpMessage)#%d (%d) {
  ["type:protected"]=>
  int(2)
  ["body:protected"]=>
  string(309) "string(294) "<?xml version="1.0" encoding="iso-8859-1"?>
<methodCall>
<methodName>testMethod</methodName>
<params>
 <param>
  <value>
   <struct>
    <member>
     <name>foo</name>
     <value>
      <string>bar</string>
     </value>
    </member>
   </struct>
  </value>
 </param>
</params>
</methodCall>
"
"
  ["requestMethod:protected"]=>
  string(0) ""
  ["requestUrl:protected"]=>
  string(0) ""
  ["responseStatus:protected"]=>
  string(2) "OK"
  ["responseCode:protected"]=>
  int(200)
  ["httpVersion:protected"]=>
  float(1.1)
  ["headers:protected"]=>
  array(6) {
    %s
  }
  ["parentMessage:protected"]=>
  NULL
}
Done
