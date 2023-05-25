--TEST--
client ssl
--SKIPIF--
<?php
include "skipif.inc";
skip_online_test();
skip_client_test();
skip_curl_test("7.34.0");
if (0 === strpos(http\Client\Curl\Versions\CURL, "7.87.0")) {
	die("skip SSL bug in libcurl-7.87\n");
}
if (strpos(http\Client\Curl\Versions\SSL, "SecureTransport") !== false)
	die("skip SecureTransport\n");
?>
--FILE--
<?php
echo "Test\n";

$client = new http\Client;

$client->setSslOptions(array("verifypeer" => true));
$client->addSslOptions(array("verifyhost" => 2));
var_dump(
	array(
		"verifypeer" => true,
		"verifyhost" => 2,
	) === $client->getSslOptions()
);

$client->attach($observer = new class implements SplObserver {
	public $data = [];

	#[ReturnTypeWillChange]
	function update(SplSubject $client, $req = null, $progress = null) {
		$ti = $client->getTransferInfo($req);
		if (isset($ti->tls_session["internals"])) {
			foreach ((array) $ti->tls_session["internals"] as $key => $val) {
				if (!isset($this->data[$key]) || $this->data[$key] < $val) {
					$this->data[$key] = $val;
				}
			}
		}
	}
});

$client->enqueue($req = new http\Client\Request("GET", "https://twitter.com/"));
$client->send();

switch ($client->getTransferInfo($req)->tls_session["backend"]) {
	case "openssl":
	case "gnutls":
		if (count($observer->data) < 1) {
			printf("%s: failed count(ssl.internals) >= 1\n", $client->getTransferInfo($req)->tls_session["backend"]);
			var_dump($observer);
			exit;
		}
		break;
	default:
		break;
}
?>
Done
--EXPECTF--
Test
bool(true)
Done
