--TEST--
HttpRequestPool detaching in callbacks
--SKIPIF--
<?php
include 'skip.inc';
checkcls("HttpRequestPool");
checkurl("at.php.net");
checkurl("de.php.net");
?>
--FILE--
<?php
echo "-TEST\n";
class r extends HttpRequest {
	function onProgress() {
		static $i = array();
		if (empty($i[$this->getUrl()])) {
			$i[$this->getUrl()] = true;
			try {
				$GLOBALS['p']->detach($this);
			} catch (Exception $ex) {
				echo $ex, "\n";
			}
		}
	}
	function onFinish() {
		$GLOBALS['p']->detach($this);
	}
}
$p = new HttpRequestPool(new r("http://at.php.net"), new r("http://de.php.net"));
$p->send();
var_dump($p->getAttachedRequests());
echo "Done\n";
?>
--EXPECTF--
%sTEST
exception 'HttpRequestPoolException' with message 'HttpRequest object(#%d) cannot be detached from the HttpRequestPool while executing the progress callback' in %sHttpRequestPool_006.php:%d
Stack trace:
#0 %sHttpRequestPool_006.php(%d): HttpRequestPool->detach(Object(r))
#1 [internal function]: r->onProgress(Array)
#2 %sHttpRequestPool_006.php(%d): HttpRequestPool->send()
#3 {main}
exception 'HttpRequestPoolException' with message 'HttpRequest object(#%d) cannot be detached from the HttpRequestPool while executing the progress callback' in %sHttpRequestPool_006.php:%d
Stack trace:
#0 %sHttpRequestPool_006.php(%d): HttpRequestPool->detach(Object(r))
#1 [internal function]: r->onProgress(Array)
#2 %sHttpRequestPool_006.php(%d): HttpRequestPool->send()
#3 {main}
array(0) {
}
Done
