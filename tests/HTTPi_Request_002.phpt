--TEST--
HTTPi_Request GET
--SKIPIF--
<?php 
include 'skip.inc';
(5 > (int) PHP_VERSION) and die('skip PHP5 is required for HTTPi');
?>
--FILE--
<?php
$r = new HTTPi_Request('http://localhost', HTTP_GET);
var_dump($r->send());
print_r($r->getResponseInfo());
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

bool(true)
Array
(
    [effective_url] => http://localhost/
    [response_code] => 200
    [http_connectcode] => 0
    [filetime] => -1
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
    [content_type] => text/html%s
    [httpauth_avail] => %d
    [proxyauth_avail] => %d
)
