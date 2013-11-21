--TEST--
. become _ in query strings due to php_default_treat_data()
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
$url = 'http://www.example.com/foobar?bar.baz=blah&utm_source=google&utm_campaign=somethingelse&blat';

// Original
echo $url, PHP_EOL, PHP_EOL;

// Changing a parameter in the string
echo new http\Url($url, array('query' => 'utm_source=changed'), http\Url::JOIN_QUERY), PHP_EOL, PHP_EOL;

// Replacing the host
echo new http\Url($url, array('host' => 'www.google.com')), PHP_EOL, PHP_EOL;

// Generating a query string from scratch
echo new http\QueryString(array(
            'bar.baz' => 'blah',
            'utm_source' => 'google',
            'utm_campaign' => 'somethingelse',
            'blat' => null,
            )), PHP_EOL, PHP_EOL;
?>
DONE
--EXPECT--
http://www.example.com/foobar?bar.baz=blah&utm_source=google&utm_campaign=somethingelse&blat

http://www.example.com/foobar?bar.baz=blah&utm_source=changed&utm_campaign=somethingelse

http://www.google.com/foobar?bar.baz=blah&utm_source=google&utm_campaign=somethingelse&blat

bar.baz=blah&utm_source=google&utm_campaign=somethingelse

DONE
