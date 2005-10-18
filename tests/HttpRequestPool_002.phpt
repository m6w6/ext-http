--TEST--
extending HttpRequestPool
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequestPool');
?>
--FILE--
<?php
echo "-TEST\n";

class MyPool extends HttpRequestPool
{
	public function send()
	{
		while ($this->socketPerform()) {
			$this->handleRequests();
			if (!$this->socketSelect()) {
				throw new HttpSocketException;
			}
		}
		$this->handleRequests();
	}
	
	private function handleRequests()
	{
		echo ".";
		foreach ($this->getFinishedRequests() as $r) {
			echo "=", $r->getResponseCode(), "=";
			$this->detach($r);
		}
	}
}

$pool = new MyPool(
    new HttpRequest('http://www.php.net/', HTTP_METH_HEAD),
    new HttpRequest('http://at.php.net/', HTTP_METH_HEAD),
    new HttpRequest('http://de.php.net/', HTTP_METH_HEAD),
    new HttpRequest('http://ch.php.net/', HTTP_METH_HEAD)
);

$pool->send();

echo "\nDone\n";
?>
--EXPECTREGEX--
.+TEST
\.+=200=\.+=200=\.+=200=\.+=200=
Done
