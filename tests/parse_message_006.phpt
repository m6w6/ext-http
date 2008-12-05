--TEST--
mixed EOL trap
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";

$message =
"HTTP/1.1 200 Ok\n".
"Header: Value\r\n".
"Connection: close\r\n".
"\n".
"Bug!";

print_r(http_parse_message($message));

echo "Done\n";
--EXPECTF--
%aTEST
stdClass Object
(
    [type] => 2
    [httpVersion] => 1.1
    [responseCode] => 200
    [responseStatus] => Ok
    [headers] => Array
        (
            [Header] => Value
            [Connection] => close
        )

    [body] => Bug!
    [parentMessage] => 
)
Done
