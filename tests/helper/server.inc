<?php 

function serve(callable $cb) {
	foreach (range(8000, 9000) as $port) {
		if (($server = @stream_socket_server("tcp://localhost:$port"))) {
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
	proc(PHP_BINARY, [__DIR__."/$handler"], $cb);
}

function nghttpd(callable $cb) {
	$spec = [["pipe","r"], ["pipe","w"], ["pipe","w"]];
	foreach (range(8000, 9000) as $port) {
		$comm = "exec nghttpd -d html $port http2.key http2.crt";
		if (($proc = proc_open($comm, $spec, $pipes, __DIR__))) {
			$stdin = $pipes[0];
			$stdout = $pipes[1];
			$stderr = $pipes[2];
			
			usleep(50000);
			$status = proc_get_status($proc);
			
			if (!$status["running"]) {
				continue;
			}
			
			try {
				$cb($port, $stdin, $stdout, $stderr);
			} catch (Exception $e) {
				echo $e,"\n";
			}
		
			proc_terminate($proc);
		
			fpassthru($stderr);
			fpassthru($stdout);
			return;
		}
	}
			
}

function proc($bin, $args, callable $cb) {
	$spec = [["pipe","r"], ["pipe","w"], ["pipe","w"]];
	$comm = escapeshellcmd($bin) . " ". implode(" ", array_map("escapeshellarg", $args));
	if (($proc = proc_open($comm, $spec, $pipes, __DIR__))) {
		$stdin = $pipes[0];
		$stdout = $pipes[1];
		$stderr = $pipes[2];
		
		do {
			$port = trim(fgets($stderr));
			$R = [$stderr]; $W = []; $E = [];
		} while (is_numeric($port) && stream_select($R, $W, $E, 0, 10000));
	
		if (is_numeric($port)) {
			try {
				$cb($port, $stdin, $stdout, $stderr);
			} catch (Exception $e) {
				echo $e,"\n";
			}
		}
	
		proc_terminate($proc);
	
		fpassthru($stderr);
		fpassthru($stdout);
	}
}