--TEST--
HttpRequestPool
--SKIPIF--
<?php
include 'skip.inc';
(5 > (int) PHP_VERSION) and die('skip PHP5 is required for Http classes');
?>
--FILE--
<?php
$urls = array(
    'http://www.php.net',
    'http://pear.php.net',
    'http://pecl.php.net'
);
$pool = new HttpRequestPool;
foreach ($urls as $url) {
    $pool->attach($reqs[] = new HttpRequest($url, HTTP_HEAD));
}
$pool->send();
foreach ($reqs as $req) {
    echo $req->getResponseInfo('effective_url'), '=', 
        $req->getResponseCode(), ':',
        $req->getResponseMessage()->getResponseCode(), "\n";
    $pool->detach($req);
    $pool->attach($req);
}
$pool->send();
$pool->reset();
echo "Done\n";
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

http://www.php.net/=200:200
http://pear.php.net/=200:200
http://pecl.php.net/=200:200
Done
