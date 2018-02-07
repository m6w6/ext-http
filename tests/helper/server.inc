<?php

ini_set("log_errors", true);
ini_set("error_log", __DIR__."/server.log");

function logger() {
	if (!ini_get("date.timezone")) {
		date_default_timezone_set(@date_default_timezone_get());
	}
	error_log(sprintf("%s(%s): %s", 
		basename(getenv("SCRIPT_FILENAME"), ".php"), 
		basename(current(get_included_files()), ".inc"), 
		call_user_func_array("sprintf", func_get_args())
	));
}

$php = getenv('TEST_PHP_EXECUTABLE');
if ($php) {
	define('PHP_BIN', $php);
} else if (defined('PHP_BINARY')) {
	define('PHP_BIN', PHP_BINARY);
} else {
	// PHP-5.3
	define("PHP_BIN", PHP_BINDIR.DIRECTORY_SEPARATOR."php");
}

foreach (array("raphf", "propro", "http") as $ext) {
	if (!extension_loaded($ext)) {
		switch (PHP_SHLIB_SUFFIX) {
			case "dll":
				dl("php_$ext.dll"); 
				break;
			default:
				dl($ext .".". PHP_SHLIB_SUFFIX);
		}
	}
}

function serve($cb) {
	/* stream_socket_server() automatically sets SO_REUSEADDR, 
	 * which is, well, bad if the tests are run in parallel
	 */
	$offset = rand(0,2000);
	foreach (range(8000+$offset, 9000+$offset) as $port) {
		logger("serve: Trying port %d", $port);
		if (($server = @stream_socket_server("tcp://localhost:$port"))) {
			fprintf(STDERR, "%s\n", $port);
			logger("serve: Using port %d", $port);
			do {
				$R = array($server); $W = array(); $E = array();
				$select = stream_select($R, $E, $E, 10, 0);
				if ($select && ($client = stream_socket_accept($server, 1))) {
					logger("serve: Accept client %d", (int) $client);
					if (getenv("PHP_HTTP_TEST_SSL")) {
						stream_socket_enable_crypto($client, true, STREAM_CRYPTO_METHOD_SSLv23_SERVER);
					}
					try {
						$R = array($client);
						while (!feof($client) && stream_select($R, $W, $E, 1, 0)) {
							logger("serve: Handle client %d", (int) $client);
							$cb($client);
						}
						logger("serve: EOF/timeout on client %d", (int) $client);
					} catch (Exception $ex) {
						logger("serve: Exception on client %d: %s", (int) $client, $ex->getMessage());
						/* ignore disconnect */
						if ($ex->getMessage() !== "Empty message received from stream") {
							fprintf(STDERR, "%s\n", $ex);
						}
						break;
					}
				}
			} while ($select);
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
	$offset = rand(0,2000);
	foreach (range(8000+$offset, 9000+$offset) as $port) {
		$comm = "exec nghttpd -d html $port http2.key http2.crt";
		if (($proc = proc_open($comm, $spec, $pipes, __DIR__))) {
			$stdin = $pipes[0];
			$stdout = $pipes[1];
			$stderr = $pipes[2];
			
			sleep(1);
			$status = proc_get_status($proc);
			logger("nghttpd: %s", new http\Params($status));
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
	logger("proc: %s %s", $bin, implode(" ", $args));
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
