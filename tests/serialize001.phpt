--TEST--
serialization
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php

$ext = new ReflectionExtension("http");
foreach ($ext->getClasses() as $class) {
	if ($class->isInstantiable()) {
		#printf("%s\n", $class->getName());
		$instance = $class->newInstance();
		$serialized = serialize($instance);
		$unserialized = unserialize($serialized);
		
		foreach (array("toString", "toArray") as $m) {
			if ($class->hasMethod($m)) {
				#printf("%s#%s\n", $class->getName(), $m);
				$unserialized->$m();
			}
		}
	}
}
?>
DONE
--EXPECTF--
DONE
