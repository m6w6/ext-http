--TEST--
etags with hash
--SKIPIF--
<?php
include "skipif.inc";
_ext("hash");
?>
--FILE--
<?php
$body = new http\Message\Body;
$body->append("Hello, my old fellow.");
foreach (["md5", "sha1", "sha256"] as $algo) {
    if (strncmp($algo, "sha3-", 5) && strncmp($algo, "sha512/", 7) && strcmp($algo, "crc32c")) {
        ini_set("http.etag.mode", $algo);
        printf("%10s: %s\n",
            $algo,
            $body->etag()
        );
    }
}
?>
DONE
--EXPECT--
       md5: 6ce3cc8f3861fb7fd0d77739f11cd29c
      sha1: ad84012eabe27a61762a97138d9d2623f4f1a7a9
    sha256: ed9ecfe5c76d51179c3c1065916fdb8d94aee05577f187bd763cdc962bba1f42
DONE
