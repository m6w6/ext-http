<?php
$r = new HttpRequest('http://www.google.com');

// store Googles cookies in a dedicated file
$r->setOptions(
	array(	'cookiestore'	=> '../cookies/google.txt',
	)
);

$r->setQueryData(
	array(	'q'		=> '+"pecl_http" -msg -cvs -list',
			'hl'	=> 'de'
	)
);

// HttpRequest::send() returns an HttpMessage object
// of type HttpMessage::RESPONSE or throws an exception
try {
	print $r->send()->getBody();
} catch (HttpException $e) {
	print $e;
}
?>
