--TEST--
Bug #67932 (php://input always empty)
--SKIPIF--
<?php 
include "skipif.inc";
?>
--PUT--
Content-Type: text/xml

<?xml version="1.0" encoding="utf-8" ?>
<body>test</body>
--FILE--
<?php
readfile("php://input");
?>
--EXPECT--
<?xml version="1.0" encoding="utf-8" ?>
<body>test</body>