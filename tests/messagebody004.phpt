--TEST--
message body add form
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$temp = new http\Message\Body;
$temp->addForm(
	array(
		"foo" => "bar",
		"more" => array(
			"bah", "baz", "fuz"
		),
	),
	array(
		array(
			"file" => __FILE__,
			"name" => "upload",
			"type" => "text/plain",
		)
	)
);

echo $temp;

?>
DONE
--EXPECTF--
Test
--%x.%x
Content-Disposition: form-data; name="foo"

bar
--%x.%x
Content-Disposition: form-data; name="more[0]"

bah
--%x.%x
Content-Disposition: form-data; name="more[1]"

baz
--%x.%x
Content-Disposition: form-data; name="more[2]"

fuz
--%x.%x
Content-Disposition: form-data; name="upload"; filename="%s"
Content-Transfer-Encoding: binary
Content-Type: text/plain

<?php
echo "Test\n";

$temp = new http\Message\Body;
$temp->addForm(
	array(
		"foo" => "bar",
		"more" => array(
			"bah", "baz", "fuz"
		),
	),
	array(
		array(
			"file" => __FILE__,
			"name" => "upload",
			"type" => "text/plain",
		)
	)
);

echo $temp;

?>
DONE

--%x.%x--
DONE
