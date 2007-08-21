--TEST--
persistent handles
--SKIPIF--
<?php
include 'skip.inc';
skipif(!http_support(HTTP_SUPPORT_REQUESTS), "need request support");
skipif(function_exists('zend_thread_id'), "need non-ZTS build");
?>
--INI--
http.persistent.handles.limit=-1
http.persistent.handles.ident=GLOBAL
--FILE--
<?php
echo "-TEST\n";

echo "No free handles!\n";
foreach (http_persistent_handles_count() as $provider => $idents) {
	foreach ((array)$idents as $ident => $counts) {
		if (!empty($counts["free"])) {
			printf("%s, %s, %s\n", $provider, $ident, $counts["free"]);
		}
	}
}

http_get("http://www.google.com/", null, $info[]);

echo "One free request handle within GLOBAL: ";
var_dump(http_persistent_handles_count()->http_request["GLOBAL"]["free"]);

echo "Reusing request handle: ";
http_get("http://www.google.com/", null, $info[]);
var_dump($info[0]["pretransfer_time"] > 10 * $info[1]["pretransfer_time"], $info[0]["pretransfer_time"], $info[1]["pretransfer_time"]);

echo "Handles' been cleaned up:\n";
http_persistent_handles_clean();
print_r(http_persistent_handles_count());

echo "Done\n";
?>
--EXPECTF--
%sTEST
No free handles!
One free request handle within GLOBAL: int(1)
Reusing request handle: bool(true)
float(%f)
float(%f)
Handles' been cleaned up:
stdClass Object
(
    [http_request] => Array
        (
            [GLOBAL] => Array
                (
                    [used] => 0
                    [free] => 0
                )

        )

    [http_request_datashare] => Array
        (
            [GLOBAL] => Array
                (
                    [used] => 0
                    [free] => 0
                )

        )

    [http_request_pool] => Array
        (
            [GLOBAL] => Array
                (
                    [used] => 0
                    [free] => 0
                )

        )

)
Done
