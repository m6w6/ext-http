--TEST--
gh-issue #93: message body add form ignores numeric indices
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$temp = new http\Message\Body;
$temp->addForm(
	array("foo", "bar", "baz"),
	array(
		array(
			"file" => __FILE__,
			"name" => "upload",
			"type" => "text/plain",
		),
		"dir" => array(
			array(
				"file" => __FILE__,
				"name" => 1,
				"type" => "text/plain",
			),
			array(
				"file" => __FILE__,
				"name" => 2,
				"type" => "text/plain",
			),
		),
	)
);

echo $temp;

?>
DONE
--EXPECTF--
Test
--%x.%x
Content-Disposition: form-data; name="0"

foo
--%x.%x
Content-Disposition: form-data; name="1"

bar
--%x.%x
Content-Disposition: form-data; name="2"

baz
--%x.%x
Content-Disposition: form-data; name="upload"; filename="gh-issue92.php"
Content-Transfer-Encoding: binary
Content-Type: text/plain

<?php
echo "Test\n";

$temp = new http\Message\Body;
$temp->addForm(
	array("foo", "bar", "baz"),
	array(
		array(
			"file" => __FILE__,
			"name" => "upload",
			"type" => "text/plain",
		),
		"dir" => array(
			array(
				"file" => __FILE__,
				"name" => 1,
				"type" => "text/plain",
			),
			array(
				"file" => __FILE__,
				"name" => 2,
				"type" => "text/plain",
			),
		),
	)
);

echo $temp;

?>
DONE

--%x.%x
Content-Disposition: form-data; name="dir[1]"; filename="gh-issue92.php"
Content-Transfer-Encoding: binary
Content-Type: text/plain

<?php
echo "Test\n";

$temp = new http\Message\Body;
$temp->addForm(
	array("foo", "bar", "baz"),
	array(
		array(
			"file" => __FILE__,
			"name" => "upload",
			"type" => "text/plain",
		),
		"dir" => array(
			array(
				"file" => __FILE__,
				"name" => 1,
				"type" => "text/plain",
			),
			array(
				"file" => __FILE__,
				"name" => 2,
				"type" => "text/plain",
			),
		),
	)
);

echo $temp;

?>
DONE

--%x.%x
Content-Disposition: form-data; name="dir[2]"; filename="gh-issue92.php"
Content-Transfer-Encoding: binary
Content-Type: text/plain

<?php
echo "Test\n";

$temp = new http\Message\Body;
$temp->addForm(
	array("foo", "bar", "baz"),
	array(
		array(
			"file" => __FILE__,
			"name" => "upload",
			"type" => "text/plain",
		),
		"dir" => array(
			array(
				"file" => __FILE__,
				"name" => 1,
				"type" => "text/plain",
			),
			array(
				"file" => __FILE__,
				"name" => 2,
				"type" => "text/plain",
			),
		),
	)
);

echo $temp;

?>
DONE

--%x.%x--
DONE
