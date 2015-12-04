--TEST--
url from env
--SKIPIF--
<?php
include "skipif.inc";
?>
--ENV--
SERVER_PORT=55555
HTTP_HOST=example.com
--GET--
s=b&i=0&e=&a[]=1&a[]=2
--FILE--
<?php
printf("%s\n", new http\Env\Url);
printf("%s\n", new http\Env\Url("other", "index"));
printf("%s\n", new http\Env\Url(array("scheme" => "https", "port" => 443)));
printf("%s\n", new http\Env\Url(array("path" => "/./up/../down/../././//index.php/.", "query" => null), null, http\Url::SANITIZE_PATH|http\Url::FROM_ENV));
printf("%s\n", new http\Env\Url(null, null, 0));
printf("%s\n", new http\Url(null, null, http\Url::FROM_ENV));
?>
DONE
--EXPECTF--
http://example.com:55555/?s=b&i=0&e=&a[]=1&a[]=2
http://example.com:55555/index?s=b&i=0&e=&a[]=1&a[]=2
https://example.com/?s=b&i=0&e=&a[]=1&a[]=2
http://example.com:55555/index.php/
http://example.com:55555/?s=b&i=0&e=&a[]=1&a[]=2
http://example.com:55555/?s=b&i=0&e=&a[]=1&a[]=2
DONE
