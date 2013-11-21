<?php
function _ext($ext) { extension_loaded($ext) or die("skip $ext extension needed\n"); }
_ext("http");
