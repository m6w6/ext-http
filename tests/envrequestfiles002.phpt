--TEST--
env request grabbing $_FILES from array
--SKIPIF--
<?php include "skipif.inc"; ?>
--POST_RAW--
Content-Type: multipart/form-data;boundary=--123

----123
Content-Disposition: form-data;filename=file1;name=file[]

first
----123
Content-Disposition: form-data;filename=file2;name=file[]

second
----123
Content-Disposition: form-data;filename=file3;name=file[]

third
----123--
--FILE--
<?php
echo "TEST\n";

$r = new http\Env\Request;
$f = array();

foreach ($_FILES as $name => $data) {
    foreach ($data["tmp_name"] as $i => $file) {
        $f[$name][$i] = array(
            "file" => $file,
            "name" => $data["name"][$i],
            "size" => $data["size"][$i],
            "type" => $data["type"][$i],
            "error"=> $data["error"][$i]
        );
    }
}

var_dump($f == $r->getFiles());

?>
DONE
--EXPECT--
TEST
bool(true)
DONE
