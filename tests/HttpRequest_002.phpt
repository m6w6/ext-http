--TEST--
HttpRequest GET/POST
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
checkcls('HttpRequest');
checkurl('www.google.com');
checkurl('dev.iworks.at');
?>
--FILE--
<?php
echo "-TEST\n";

$r = new HttpRequest('http://www.google.com', HttpRequest::METH_GET);
$r->send();
print_r($r->getResponseInfo());

$r = new HttpRequest('http://dev.iworks.at/ext-http/.print_request.php', HTTP_METH_POST);
$r->addCookies(array('MyCookie' => 'foobar'));
$r->addQueryData(array('gq'=>'foobar','gi'=>10));
$r->addPostFields(array('pq'=>'foobar','pi'=>10));
$r->addPostFile('upload', dirname(__FILE__).'/data.txt', 'text/plain');
$r->send();
echo $r->getResponseBody();
var_dump($r->getResponseMessage()->getResponseCode());

echo "Done";
?>
--EXPECTF--
%sTEST
Array
(
    [effective_url] => http://www.google.com/
    [response_code] => 302
    [total_time] => %f
    [namelookup_time] => %f
    [connect_time] => %f
    [pretransfer_time] => %f
    [size_upload] => %d
    [size_download] => %d
    [speed_download] => %d
    [speed_upload] => %d
    [header_size] => %d
    [request_size] => %d
    [ssl_verifyresult] => %d
    [filetime] => -1
    [content_length_download] => %d
    [content_length_upload] => %d
    [starttransfer_time] => %f
    [content_type] => text/html
    [redirect_time] => %d
    [redirect_count] => %d
    [connect_code] => %d
    [httpauth_avail] => %d
    [proxyauth_avail] => %d
    [os_errno] => %d
    [num_connects] => %d
    [ssl_engines] => Array%s

    [cookies] => Array%s

    [error] => 
)
Array
(
    [gq] => foobar
    [gi] => 10
    [pq] => foobar
    [pi] => 10
    [MyCookie] => foobar
)
Array
(
    [upload] => Array
        (
            [name] => data.txt
            [type] => text/plain
            [tmp_name] => %s
            [error] => 0
            [size] => 1010
        )

)
int(200)
Done
