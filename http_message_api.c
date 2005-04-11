/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "php.h"
#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_message_api.h"
#include "php_http_headers_api.h"

#include "phpstr/phpstr.h"

PHP_HTTP_API void _http_message_dtor(http_message *message)
{
	if (message) {
		zend_hash_destroy(&message->hdrs);
		phpstr_dtor(PHPSTR(message));
		if (message->raw) {
			efree(message->raw);
		}
		if (message->type == HTTP_MSG_REQUEST) {
			if (message->info.request.method) {
				efree(message->info.request.method);
			}
			if (message->info.request.URI) {
				efree(message->info.request.URI);
			}
		}
	}
}

PHP_HTTP_API void _http_message_free(http_message *message)
{
	if (message) {
		if (message->nested) {
			http_message_free(message->nested);
		}
		http_message_dtor(message);
		efree(message);
	}
}

PHP_HTTP_API http_message *_http_message_init_ex(http_message *message, http_message_type type)
{
	if (!message) {
		message = ecalloc(1, sizeof(http_message));
	}

	message->type = type;
	message->nested = NULL;
	phpstr_init(&message->body);
	zend_hash_init(&message->hdrs, 0, NULL, ZVAL_PTR_DTOR, 0);

	return message;
}

PHP_HTTP_API http_message *_http_message_parse_ex(char *message, size_t length, zend_bool dup TSRMLS_DC)
{
	char *message_start = message, *body = NULL;
	size_t message_length = length, header_length = 0;
	http_message *msg;

	if (length < HTTP_MSG_MIN_SIZE) {
		return NULL;
	}
	if (!message) {
		return NULL;
	}

	if (!(message_start = strstr(message, HTTP_CRLF))) {
		return NULL;
	}

	msg = http_message_init();

	msg->len = length;
	msg->raw = dup ? estrndup(message, length) : message;

	// response
	if (!strncmp(message, "HTTP/1.", lenof("HTTP/1."))) {
		msg->type = HTTP_MSG_RESPONSE;
		msg->info.response.http_version = atof(message + lenof("HTTP/"));
		msg->info.response.status = atoi(message + lenof("HTTP/1.1 "));
	} else
	// request
	if (!strncmp(message_start - lenof("HTTP/1.1"), "HTTP/1.", lenof("HTTP/1."))) {
		const char *method_sep_uri = strchr(message, ' ');

		msg->type = HTTP_MSG_REQUEST;
		msg->info.request.http_version = atof(message_start - lenof("1.1"));
		msg->info.request.method = estrndup(message, method_sep_uri - message);
		msg->info.request.URI = estrndup(method_sep_uri + 1, message_start - method_sep_uri - 1 - lenof(" HTTP/1.1"));
	} else {
		http_message_free(msg);
		return NULL;
	}

	message_start += lenof(HTTP_CRLF);
	message_length -= message_start - message;

	if (body = strstr(message_start, HTTP_CRLF HTTP_CRLF)) {
		body += lenof(HTTP_CRLF HTTP_CRLF);
		header_length = body - message_start;
		phpstr_from_string_ex(PHPSTR(msg), body, message_length - header_length);
	} else {
		header_length = message_length;
	}

	if (SUCCESS != http_parse_headers_ex(message_start, header_length, &msg->hdrs, 1)) {
		http_message_free(msg);
		return NULL;
	}
	return msg;
}

PHP_HTTP_API void _http_message_tostring(http_message *msg, char **string, size_t *length)
{
	phpstr str;
	char *key, *data;
	ulong idx;
	zval **header;

	phpstr_init_ex(&str, msg->len, 1);

	switch (msg->type)
	{
		case HTTP_MSG_REQUEST:
			phpstr_appendf(&str, "%s %s HTTP/%1.1f" HTTP_CRLF,
				msg->info.request.method,
				msg->info.request.URI,
				msg->info.request.http_version);
		break;

		case HTTP_MSG_RESPONSE:
			phpstr_appendf(&str, "HTTP/%1.1f %d" HTTP_CRLF,
				msg->info.response.http_version,
				msg->info.response.status);
		break;
	}

	FOREACH_HASH_KEYVAL(&msg->hdrs, key, idx, header) {
		if (key) {
			phpstr_appendf(&str, "%s: %s" HTTP_CRLF, key, Z_STRVAL_PP(header));
			key = NULL;
		}
	}

	phpstr_appends(&str, HTTP_CRLF);
	phpstr_append(&str, msg->body.data, msg->body.used);
	phpstr_fix(&str);

	data = phpstr_data(&str, string, length);
	if (!string) {
		efree(data);
	}

	phpstr_dtor(&str);
}
