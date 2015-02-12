<?php 

function serve(callable $cb) {
	foreach (range(8000, 9000) as $port) {
		if (($server = stream_socket_server("tcp://localhost:$port"))) {
			fprintf(STDERR, "%s\n", $port);
			do {
				$R = [$server]; $W = []; $E = [];
				$select = stream_select($R, $E, $E, 0, 10000);
				if ($select && ($client = stream_socket_accept($server, 1))) {
					if (getenv("PHP_HTTP_TEST_SSL")) {
						stream_socket_enable_crypto($client, true, STREAM_CRYPTO_METHOD_SSLv23_SERVER);
					}
					while (!feof($client)) {
						$cb($client);
					}
				}
			} while ($select !== false);
			return;
		}
	}
}

function server($handler, callable $cb) {
	$spec = array(array("pipe","r"), array("pipe","w"), array("pipe","w"));
	$comm = sprintf("%s %s/%s", PHP_BINARY, __DIR__, $handler);
	if (($proc = proc_open($comm, $spec, $pipes, __DIR__))) {
		$port = trim(fgets($pipes[2]));
		
		try {
			$cb($port, $stdin = $pipes[0], $stdout = $pipes[1], $stderr = $pipes[2]);
		} catch (Exception $e) {
			echo $e,"\n";
		}
		
		proc_terminate($proc);
		
		fpassthru($stderr);
		fpassthru($stdout);
	}
}
