<?php

$php = getenv('TEST_PHP_EXECUTABLE');
if ($php) {
	define('PHP_BIN', $php);
} else if (defined('PHP_BINARY')) {
	define('PHP_BIN', PHP_BINARY);
} else {
	// PHP-5.3
	define("PHP_BIN", PHP_BINDIR.DIRECTORY_SEPARATOR."php");
}

function serve($cb) {
	foreach (range(8000, 9000) as $port) {
		if (($server = @stream_socket_server("tcp://localhost:$port"))) {
			fprintf(STDERR, "%s\n", $port);
			do {
				$R = array($server); $W = array(); $E = array();
				$select = stream_select($R, $E, $E, 0, 10000);
				if ($select && ($client = stream_socket_accept($server, 1))) {
					if (getenv("PHP_HTTP_TEST_SSL")) {
						stream_socket_enable_crypto($client, true, STREAM_CRYPTO_METHOD_SSLv23_SERVER);
					}
					try {
						while (!feof($client)) {
							$cb($client);
						}
					} catch (Exception $ex) {
						/* ignore disconnect */
						if ($ex->getMessage() !== "Empty message received from stream") {
							fprintf(STDERR, "%s\n", $ex);
						}
						break;
					}
				}
			} while ($select !== false);
			return;
		}
	}
}

function server($handler, $cb) {
	$args = explode(' ', getenv('TEST_PHP_ARGS'));
	$args[] = __DIR__."/$handler";
	foreach ($args as $k => $v) {
		if (!$v) unset($args[$k]);
	}
	proc(PHP_BIN, $args, $cb);
}

function nghttpd($cb) {
	$spec = array(array("pipe","r"), array("pipe","w"), array("pipe","w"));
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

function proc($bin, $args, $cb) {
	$spec = array(array("pipe","r"), array("pipe","w"), array("pipe","w"));
	$comm = escapeshellcmd($bin) . " ". implode(" ", array_map("escapeshellarg", $args));
	if (($proc = proc_open($comm, $spec, $pipes, __DIR__))) {
		$stdin = $pipes[0];
		$stdout = $pipes[1];
		$stderr = $pipes[2];

		do {
			$port = trim(fgets($stderr));
			$R = array($stderr); $W = array(); $E = array();
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