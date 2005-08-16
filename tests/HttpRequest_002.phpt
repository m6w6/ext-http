--TEST--
HttpRequest GET/POST
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
checkcls('HttpRequest');
checkurl('www.google.com');
?>
--FILE--
<?php
echo "-TEST\n";
$r = new HttpRequest('http://www.google.com', HTTP_GET);
$r->send();
print_r($r->getResponseInfo());
$r->setMethod(HTTP_POST);
$r->addPostFields(array('q'=>'foobar','start'=>10));
$r->send();
var_dump($r->getResponseCode());
var_dump($r->getResponseMessage()->getResponseCode());
var_dump(false != strstr($r->getResponseBody(), "Not Implemented"));
?>
--EXPECTF--
%sTEST
Array
(
    [effective_url] => http://www.google.com/
    [response_code] => %d
    [http_connectcode] => %d
    [filetime] => %s
    [total_time] => %f
    [namelookup_time] => %f
    [connect_time] => %f
    [pretransfer_time] => %f
    [starttransfer_time] => %f
    [redirect_time] => %f
    [redirect_count] => %f
    [size_upload] => %f
    [size_download] => %d
    [speed_download] => %d
    [speed_upload] => %d
    [header_size] => %d
    [request_size] => %d
    [ssl_verifyresult] => %d
    [content_length_download] => %d
    [content_length_upload] => %d
    [content_type] => %s
    [httpauth_avail] => %d
    [proxyauth_avail] => %s
)
int(501)
int(501)
bool(true)

