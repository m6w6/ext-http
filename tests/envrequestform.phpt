--TEST--
env request form
--SKIPIF--
<?php include "skipif.inc"; ?>
--POST--
a=b&b=c&r[]=1&r[]=2
--FILE--
<?php
echo "TEST\n";

$r = new http\Env\Request;
printf("%s\n", $r->getForm());
printf("%s\n", $r->getForm("b", "s", null, true));
printf("%s\n", $r->getForm("x", "s", "nooo"));
printf("%s\n", $r->getForm());
?>
DONE
--EXPECT--
TEST
a=b&b=c&r%5B0%5D=1&r%5B1%5D=2
c
nooo
a=b&r%5B0%5D=1&r%5B1%5D=2
DONE
