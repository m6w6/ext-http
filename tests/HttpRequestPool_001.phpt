--TEST--
HttpRequestPool
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
checkcls('HttpRequestPool');
checkurl('www.php.net');
checkurl('dev.iworks.at');
?>
--FILE--
<?php
echo "-TEST\n";
$post = new HttpRequest('http://dev.iworks.at/ext-http/.print_request.php', HTTP_METH_POST);
$post->addPostFields(array('a'=>1,'b'=>2)) ;

$pool = new HttpRequestPool(
    new HttpRequest('http://www.php.net/', HTTP_METH_HEAD),
    $post
);

$pool->send();

foreach ($pool as $req) {
    echo $req->getUrl(), '=',
        $req->getResponseCode(), ':',
        $req->getResponseMessage()->getResponseCode(), "\n";
}

foreach ($pool as $req) {
	try {
		$pool->attach(new HttpRequest('http://foo.bar'));
	} catch (HttpRequestPoolException $x) {
		echo ".\n";
	}
}

foreach ($pool as $req) {
	$pool->detach($req);
}

echo "Done\n";
?>

--EXPECTF--
%sTEST
http://www.php.net/=200:200
http://dev.iworks.at/ext-http/.print_request.php=200:200
.
.
Done
