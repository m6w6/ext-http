<?php 

function serve(callable $cb) {
	foreach (range(8000, 9000) as $port) {
		if (($server = stream_socket_server("tcp://localhost:$port"))) {
			fprintf(STDERR, "%s\n", $port);
			if (($client = stream_socket_accept($server))) {
				$cb($client);
			}
			return;
		}
	}
}

function server($file, callable $cb) {
	$spec = array(array("pipe","r"), array("pipe","w"), array("pipe","w"));
	if (($proc = proc_open(PHP_BINARY . " $file", $spec, $pipes, __DIR__))) {
		$port = trim(fgets($pipes[2]));
		
		$cb($port, 
				$stdin = $pipes[0], 
				$stdout = $pipes[1], 
				$stderr = $pipes[2]);
		
		fpassthru($stderr);
		fpassthru($stdout);
	}
}
