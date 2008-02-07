--TEST--
http_parse_message() content range header w/(o) =
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
print_r(http_parse_message(
"
HTTP/1.1 206
Server: Funky/1.0
Content-Range: bytes: 0-0/100

1

HTTP/1.1 206
Server: Funky/1.0
Content-Range: bytes 0-0/100

1

"
));
echo "Done\n";
?>
--EXPECTF--
%aTEST
stdClass Object
(
    [type] => 2
    [httpVersion] => 1.1
    [responseCode] => 206
    [responseStatus] => 
    [headers] => Array
        (
            [Server] => Funky/1.0
            [Content-Range] => bytes 0-0/100
        )

    [body] => 1
    [parentMessage] => stdClass Object
        (
            [type] => 2
            [httpVersion] => 1.1
            [responseCode] => 206
            [responseStatus] => 
            [headers] => Array
                (
                    [Server] => Funky/1.0
                    [Content-Range] => bytes: 0-0/100
                )

            [body] => 1
            [parentMessage] => 
        )

)
Done
