--TEST--
env request grabbing $_FILES
--SKIPIF--
<?php include "skipif.inc"; ?>
--POST_RAW--
Content-Type: multipart/form-data;boundary=--123

----123
Content-Disposition: form-data;filename="foo.bar"

foo
bar

----123
Content-Disposition: form-data;filename="bar.foo"

bar
foo

----123--
--FILE--
<?php
echo "TEST\n";

$r = new http\Env\Request;
$f = $r->getFiles();

foreach ($_FILES as $i => $file) {
    foreach (array("name","type","size","error","file") as $key) {
        if ($file[$key == "file" ? "tmp_name" : $key] != $f[$i][$key]) {
            printf("%d.%s differs: '%s' != '%s'\n", $i, $key, $f[$i][$key], $file[$key]);
        }
    }
}

?>
DONE
--EXPECT--
TEST
DONE
