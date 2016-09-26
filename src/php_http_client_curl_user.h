/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_CLIENT_CURL_USER_H
#define PHP_HTTP_CLIENT_CURL_USER_H

#if PHP_HTTP_HAVE_LIBCURL

typedef struct php_http_client_curl_user_context {
	php_http_client_t *client;
	zval user;
	zend_function closure;
	php_http_object_method_t timer;
	php_http_object_method_t socket;
	php_http_object_method_t once;
	php_http_object_method_t wait;
	php_http_object_method_t send;
} php_http_client_curl_user_context_t;

PHP_HTTP_API zend_class_entry *php_http_client_curl_user_get_class_entry();
PHP_HTTP_API php_http_client_curl_ops_t *php_http_client_curl_user_ops_get();
PHP_MINIT_FUNCTION(http_client_curl_user);

#endif

#if 0
<?php

interface http\Client\Curl\User
{
	const POLL_NONE = 0;
	const POLL_IN = 1;
	const POLL_OUT = 2;
	const POLL_INOUT = 3;
	const POLL_REMOVE = 4;

	/**
	 * Initialize the loop
	 *
	 * The callback should be run when:
	 *  - timeout occurs
	 *  - a watched socket needs action
	 *
	 * @param callable $run callback as function(http\Client $client, resource $socket = null, int $action = http\Client\Curl\User::POLL_NONE):int (returns unfinished requests pending)
	 */
	function init(callable $run);

	/**
	 * Register a timeout watcher
	 * @param int $timeout_ms desired timeout with milliseconds resolution
	 */
	function timer(int $timeout_ms);

	/**
	 * (Un-)Register a socket watcher
	 * @param resource $socket the fd to watch
	 * @param int $poll http\Client\Curl\Loop::POLL_* constant
	 */
	function socket($socket, int $poll);

	/**
	 * Run the loop as long as it does not block
	 *
	 * Called by http\Client::once()
	 */
	function once();

	/**
	 * Wait/poll/select (block the loop) until events fire
	 *
	 * Called by http\Client::wait()
	 *
	 * @param int $timeout_ms block for maximal $timeout_ms milliseconds
	 */
	function wait(int $timeout_ms = null);

	/**
	 * Run the loop
	 *
	 * Called by http\Client::send() while there are unfinished requests and
	 * no exception has occurred
	 */
	function send();
}
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
