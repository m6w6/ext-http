--TEST--
env response header
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php

http\Env::setResponseHeader("No", "way");
http\Env::setResponseHeader("Foo", "bar");
http\Env::setResponseHeader("No", null);

print_r(http\Env::getResponseHeader());

--EXPECTHEADERS--
Foo: bar
--EXPECTF--
Array
(
    [X-Powered-By] => %s
    [Foo] => bar
)
