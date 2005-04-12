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

PHP_HTTP_API http_message *_http_message_parse_ex(http_message *msg, char *message, size_t message_length, zend_bool dup TSRMLS_DC)
{
	char *body = NULL;
	size_t header_length = 0;
	zend_bool free_msg = msg ? 0 : 1;

	if (message_length < HTTP_MSG_MIN_SIZE) {
		return NULL;
	}

	if (!message) {
		return NULL;
	}

	msg = http_message_init(msg);
	msg->len = message_length;
	msg->raw = dup ? estrndup(message, message_length) : message;

	if (body = strstr(message, HTTP_CRLF HTTP_CRLF)) {
		body += lenof(HTTP_CRLF HTTP_CRLF);
		header_length = body - message;
	} else {
		header_length = message_length;
	}

	if (SUCCESS != http_parse_headers_cb(message, header_length, &msg->hdrs, 1, http_message_parse_headers_callback, (void **) &msg)) {
		if (free_msg) {
			http_message_free(msg);
		}
		return NULL;
	}

	if (body) {
		phpstr_from_string_ex(PHPSTR(msg), body, message_length - header_length);
	}

	return msg;
}

PHP_HTTP_API void _http_message_parse_headers_callback(void **message, char *http_line, size_t line_length, HashTable **headers TSRMLS_DC)
{
	http_message *old = (http_message *) *message;
	http_message *new;

	if (old->type || zend_hash_num_elements(&old->hdrs) || PHPSTR_LEN(old)) {
		new = http_message_new();

		new->nested = old;
		*message = new;
		*headers = &new->hdrs;
	} else {
		new = old;
	}

	// response
	if (!strncmp(http_line, "HTTP/1.", lenof("HTTP/1."))) {
		new->type = HTTP_MSG_RESPONSE;
		new->info.response.http_version = atof(http_line + lenof("HTTP/"));
		new->info.response.status = atoi(http_line + lenof("HTTP/1.1 "));
	} else
	// request
	if (!strncmp(http_line + line_length - lenof("HTTP/1.1"), "HTTP/1.", lenof("HTTP/1."))) {
		const char *method_sep_uri = strchr(http_line, ' ');

		new->type = HTTP_MSG_REQUEST;
		new->info.request.http_version = atof(http_line + line_length - lenof("1.1"));
		new->info.request.method = estrndup(http_line, method_sep_uri - http_line);
		new->info.request.URI = estrndup(method_sep_uri + 1, http_line + line_length - method_sep_uri - 1 - lenof(" HTTP/1.1"));
	}
}

PHP_HTTP_API void _http_message_tostring(http_message *msg, char **string, size_t *length)
{
	phpstr str;
	char *key, *data;
	ulong idx;
	zval **header;

	phpstr_init_ex(&str, msg->len, 1);
	/* set sane alloc size */
	str.size = 4096;

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
			zval **single_header;

			switch (Z_TYPE_PP(header))
			{
				case IS_STRING:
					phpstr_appendf(&str, "%s: %s" HTTP_CRLF, key, Z_STRVAL_PP(header));
				break;

				case IS_ARRAY:
					FOREACH_VAL(*header, single_header) {
						phpstr_appendf(&str, "%s: %s" HTTP_CRLF, key, Z_STRVAL_PP(single_header));
					}
				break;
			}

			key = NULL;
		}
	}

	phpstr_appends(&str, HTTP_CRLF);
	phpstr_append(&str, PHPSTR_VAL(msg), PHPSTR_LEN(msg));

	data = phpstr_data(&str, string, length);
	if (!string) {
		efree(data);
	}

	phpstr_dtor(&str);
}

PHP_HTTP_API void _http_message_dtor(http_message *message)
{
	if (message) {
		zend_hash_destroy(&message->hdrs);
		phpstr_dtor(PHPSTR(message));
		if (message->raw) {
			efree(message->raw);
			message->raw = NULL;
		}
		if (message->type == HTTP_MSG_REQUEST) {
			if (message->info.request.method) {
				efree(message->info.request.method);
				message->info.request.method = NULL;
			}
			if (message->info.request.URI) {
				efree(message->info.request.URI);
				message->info.request.URI = NULL;
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

