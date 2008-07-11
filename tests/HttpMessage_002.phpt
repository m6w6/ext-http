--TEST--
HttpMessage properties
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
checkcls('HttpMessage');
?>
--FILE--
<?php
class Message extends HttpMessage
{
	var $var_property = 'var';
	public $public_property = 'public';
	protected $protected_property = 'protected';
	private $private_property = 'private';

	public function test()
	{
		var_dump($this->var_property);
		var_dump($this->public_property);
		var_dump($this->protected_property);
		var_dump($this->private_property);
		var_dump($this->non_ex_property);
		$this->var_property.='_property';
		$this->public_property.='_property';
		$this->protected_property.='_property';
		$this->private_property.='_property';
		$this->non_ex_property = 'non_ex';
		var_dump($this->var_property);
		var_dump($this->public_property);
		var_dump($this->protected_property);
		var_dump($this->private_property);
		var_dump($this->non_ex_property);

		print_r($this->headers);
		$this->headers['Foo'] = 'Bar';
	}
}

error_reporting(E_ALL|E_STRICT);

echo "-TEST\n";
$m = new Message;
$m->test();
echo "Done\n";
?>
--EXPECTF--
%aTEST
string(3) "var"
string(6) "public"
string(9) "protected"
string(7) "private"

Notice: Undefined property: Message::$non_ex_property in %s
NULL
string(12) "var_property"
string(15) "public_property"
string(18) "protected_property"
string(16) "private_property"
string(6) "non_ex"
Array
(
)
%aFatal error%sCannot access HttpMessage properties by reference or array key/index in%s
