--TEST--
env response header
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php

http\Env::setResponseCode(201);

http\Env::setResponseHeader("No", "way");
http\Env::setResponseHeader("Foo", "bar");
http\Env::setResponseHeader("No", null);
http\Env::setResponseHeader("More", array("than", "what's", "good"));


print_r(http\Env::getResponseCode()); echo "\n";
print_r(http\Env::getResponseHeader("Foo")); echo "\n";
print_r(http\Env::getResponseHeader());
print_r(http\Env::getResponseStatusForCode(201));

--EXPECTHEADERS--
Foo: bar
--EXPECTF--
201
bar
Array
(
    [X-Powered-By] => %s
    [Foo] => bar
    [More] => Array
        (
            [0] => than
            [1] => what's
            [2] => good
        )

    [Content-Type] => text/html
)
Created
