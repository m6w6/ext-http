<?php

include "dump.inc";
include "server.inc";

serve(function($client) {
	$request = new http\Message($client, false);
	$response = new http\Env\Response;
	$response->setEnvRequest($request);
	$response->setContentEncoding(http\Env\Response::CONTENT_ENCODING_GZIP);
	$response->setHeader("X-Request-Content-Length", $request->getBody()->stat("size"));
	ob_start($response);
	if ($request->isMultipart()) {
		$files = [];
		foreach ($request->splitMultipartBody() as $part) {
			$cd = $part->getHeader("Content-Disposition", http\Header::class)->getParams();
			foreach ($cd->params as $key => $val) {
				if ($key === "form-data" && $val["value"] === true) {
					if (isset($val["arguments"]["filename"])) {
						$files[$val["arguments"]["name"]] = [
							"name" => $val["arguments"]["filename"],
							"type" => $part->getHeader("Content-Type"),
							"size" => $part->getBody()->stat("s"),
						];
					}
				}
			}
			print_r($files);
		}
	} else {
		if (($c = $request->getHeader("Cookie"))) {
			print_r((new http\Cookie($c))->getCookies());
		}
		if ($request->getBody()->stat("s")) {
			var_dump($request->getBody()->toString());
		}
	}
	ob_end_flush();
	$response->send($client);
});
