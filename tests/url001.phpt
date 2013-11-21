--TEST--
url from env
--SKIPIF--
<? include "skippif.inc"; ?>
--ENV--
SERVER_PORT=55555
HTTP_HOST=example.com
--GET--
s=b&i=0&e=&a[]=1&a[]=2
--FILE--
<?php
printf("%s\n", new http\Url);
printf("%s\n", new http\Url("other", "index"));
printf("%s\n", new http\Url(array("scheme" => "https", "port" => 443)));
printf("%s\n", new http\Url(array("path" => "/./up/../down/../././//index.php/.", "query" => null), null, http\Url::SANITIZE_PATH|http\Url::FROM_ENV));
printf("%s\n", new http\Url(null, null, 0));
?>
DONE
--EXPECTF--
http://example.com:55555/?s=b&i=0&e=&a[]=1&a[]=2
http://example.com:55555/index?s=b&i=0&e=&a[]=1&a[]=2
https://example.com/?s=b&i=0&e=&a[]=1&a[]=2
http://example.com:55555/index.php/
http://localhost/
DONE
