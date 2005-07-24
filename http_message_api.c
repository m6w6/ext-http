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
#include "php_http_api.h"
#include "php_http_message_api.h"
#include "php_http_headers_api.h"
#include "php_http_send_api.h"
#include "php_http_request_api.h"
#include "php_http_url_api.h"

#include "phpstr/phpstr.h"

#define http_message_headers_cb _http_message_headers_cb
static void _http_message_headers_cb(const char *http_line, HashTable **headers, void **message TSRMLS_DC)
{
	size_t line_length;
	char *crlf = NULL;
	http_message *new, *old = (http_message *) *message;

	if (crlf = strstr(http_line, HTTP_CRLF)) {
		line_length = crlf - http_line;
	} else {
		line_length = strlen(http_line);
	}

	if (old->type || zend_hash_num_elements(&old->hdrs) || PHPSTR_LEN(old)) {
		new = http_message_new();

		new->parent = old;
		*message = new;
		*headers = &new->hdrs;
	} else {
		new = old;
	}

	while (isspace(http_line[line_length-1])) --line_length;

	// response
	if (!strncmp(http_line, "HTTP/1.", lenof("HTTP/1."))) {
		new->type = HTTP_MSG_RESPONSE;
		new->info.response.http_version = atof(http_line + lenof("HTTP/"));
		new->info.response.code = atoi(http_line + lenof("HTTP/1.1 "));
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

#define http_message_init_type _http_message_init_type
static inline void _http_message_init_type(http_message *message, http_message_type type)
{
	switch (message->type = type)
	{
		case HTTP_MSG_RESPONSE:
			message->info.response.http_version = .0;
			message->info.response.code = 0;
		break;

		case HTTP_MSG_REQUEST:
			message->info.request.http_version = .0;
			message->info.request.method = NULL;
			message->info.request.URI = NULL;
		break;

		case HTTP_MSG_NONE:
		default:
		break;
	}
}

#define http_message_header(m, h) _http_message_header_ex((m), (h), sizeof(h))
#define http_message_header_ex _http_message_header_ex
static inline zval *_http_message_header_ex(http_message *msg, char *key_str, size_t key_len)
{
	zval **header;
	if (SUCCESS == zend_hash_find(&msg->hdrs, key_str, key_len, (void **) &header)) {
		return *header;
	}
	return NULL;
}

PHP_HTTP_API http_message *_http_message_init_ex(http_message *message, http_message_type type)
{
	if (!message) {
		message = ecalloc(1, sizeof(http_message));
	}

	http_message_init_type(message, type);
	message->parent = NULL;
	phpstr_init(&message->body);
	zend_hash_init(&message->hdrs, 0, NULL, ZVAL_PTR_DTOR, 0);

	return message;
}


PHP_HTTP_API void _http_message_set_type(http_message *message, http_message_type type)
{
	/* just act if different */
	if (type != message->type) {

		/* free request info */
		if (message->type == HTTP_MSG_REQUEST) {
			if (message->info.request.method) {
				efree(message->info.request.method);
			}
			if (message->info.request.URI) {
				efree(message->info.request.URI);
			}
		}

		/* init */
		http_message_init_type(message, type);
	}
}

PHP_HTTP_API http_message *_http_message_parse_ex(http_message *msg, const char *message, size_t message_length TSRMLS_DC)
{
	char *body = NULL;
	zend_bool free_msg = msg ? 0 : 1;

	if (message_length < HTTP_MSG_MIN_SIZE) {
		return NULL;
	}

	if (!message) {
		return NULL;
	}

	msg = http_message_init(msg);

	if (SUCCESS != http_parse_headers_cb(message, &msg->hdrs, 1, http_message_headers_cb, (void **) &msg)) {
		if (free_msg) {
			http_message_free(msg);
		}
		return NULL;
	}

	/* header parsing stops at CRLF CRLF */
	if (body = strstr(message, HTTP_CRLF HTTP_CRLF)) {
		zval *c;
		const char *continue_at = NULL;

		body += lenof(HTTP_CRLF HTTP_CRLF);

		/* message has content-length header */
		if (c = http_message_header(msg, "Content-Length")) {
			long len = atol(Z_STRVAL_P(c));
			phpstr_from_string_ex(PHPSTR(msg), body, len);
			continue_at = body + len;
		} else

		/* message has chunked transfer encoding */
		if (c = http_message_header(msg, "Transfer-Encoding")) {
			if (!strcasecmp("chunked", Z_STRVAL_P(c))) {
				char *decoded;
				size_t decoded_len;

				/* decode and replace Transfer-Encoding with Content-Length header */
				if (continue_at = http_chunked_decode(body, message + message_length - body, &decoded, &decoded_len)) {
					phpstr_from_string_ex(PHPSTR(msg), decoded, decoded_len);
					efree(decoded);
					{
						zval *len;
						char *tmp;

						spprintf(&tmp, 0, "%lu", (ulong) decoded_len);
						MAKE_STD_ZVAL(len);
						ZVAL_STRING(len, tmp, 0);

						zend_hash_del(&msg->hdrs, "Transfer-Encoding", sizeof("Transfer-Encoding"));
						zend_hash_add(&msg->hdrs, "Content-Length", sizeof("Content-Length"), (void *) &len, sizeof(zval *), NULL);
					}
				}
			}
		} else

		/* message has content-range header */
		if (c = http_message_header(msg, "Content-Range")) {
			ulong start = 0, end = 0;

			sscanf(Z_STRVAL_P(c), "bytes=%lu-%lu", &start, &end);
			if (end > start) {
				phpstr_from_string_ex(PHPSTR(msg), body, (size_t) (end - start));
				continue_at = body + (end - start);
			}
		} else

		/* no headers that indicate content length */
		if (1) {
			phpstr_from_string_ex(PHPSTR(msg), body, message + message_length - body);
		}

		/* check for following messages */
		if (continue_at) {
			while (isspace(*continue_at)) ++continue_at;
			if (continue_at < (message + message_length)) {
				http_message *next = NULL, *most = NULL;

				/* set current message to parent of most parent following messages and return deepest */
				if (most = next = http_message_parse(continue_at, message + message_length - continue_at)) {
					while (most->parent) most = most->parent;
					most->parent = msg;
					msg = next;
				}
			}
		}
	}

	return msg;
}

PHP_HTTP_API void _http_message_tostring(http_message *msg, char **string, size_t *length)
{
	phpstr str;
	char *key, *data;
	ulong idx;
	zval **header;

	phpstr_init_ex(&str, 4096, 0);

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
				msg->info.response.code);
		break;

		case HTTP_MSG_NONE:
		default:
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

	if (PHPSTR_LEN(msg)) {
		phpstr_appends(&str, HTTP_CRLF);
		phpstr_append(&str, PHPSTR_VAL(msg), PHPSTR_LEN(msg));
		phpstr_appends(&str, HTTP_CRLF);
	}

	data = phpstr_data(&str, string, length);
	if (!string) {
		efree(data);
	}

	phpstr_dtor(&str);
}

PHP_HTTP_API void _http_message_serialize(http_message *message, char **string, size_t *length)
{
	char *buf;
	size_t len;
	phpstr str;

	phpstr_init(&str);

	do {
		http_message_tostring(message, &buf, &len);
		phpstr_prepend(&str, buf, len);
		efree(buf);
	} while (message = message->parent);

	buf = phpstr_data(&str, string, length);
	if (!string) {
		efree(buf);
	}

	phpstr_dtor(&str);
}

PHP_HTTP_API STATUS _http_message_send(http_message *message TSRMLS_DC)
{
	STATUS rs = FAILURE;

	switch (message->type)
	{
		case HTTP_MSG_RESPONSE:
		{
			char *key;
			ulong idx;
			zval **val;

			FOREACH_HASH_KEYVAL(&message->hdrs, key, idx, val) {
				if (key) {
					char *header;
					spprintf(&header, 0, "%s: %s", key, Z_STRVAL_PP(val));
					http_send_header(header);
					efree(header);
					key = NULL;
				}
			}
			rs =	SUCCESS == http_send_status(message->info.response.code) &&
					SUCCESS == http_send_data(PHPSTR_VAL(message), PHPSTR_LEN(message)) ?
					SUCCESS : FAILURE;
		}
		break;

		case HTTP_MSG_REQUEST:
		{
#ifdef HTTP_HAVE_CURL
			char *uri = NULL;
			zval **zhost, options, headers;

			array_init(&options);
			array_init(&headers);
			zend_hash_copy(Z_ARRVAL(headers), &message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
			add_assoc_zval(&options, "headers", &headers);

			/* check host header */
			if (SUCCESS == zend_hash_find(&message->hdrs, "Host", sizeof("Host"), (void **) &zhost)) {
				char *colon = NULL, *host = NULL;
				size_t host_len = 0;
				int port = 0;

				/* check for port */
				if (colon = strchr(Z_STRVAL_PP(zhost), ':')) {
					port = atoi(colon + 1);
					host = estrndup(Z_STRVAL_PP(zhost), host_len = (Z_STRVAL_PP(zhost) - colon - 1));
				} else {
					host = estrndup(Z_STRVAL_PP(zhost), host_len = Z_STRLEN_PP(zhost));
				}
				uri = http_absolute_uri_ex(
					message->info.request.URI, strlen(message->info.request.URI),
					NULL, 0, host, host_len, port);
				efree(host);
			} else {
				uri = http_absolute_uri(message->info.request.URI);
			}

			if (!strcasecmp("POST", message->info.request.method)) {
				http_request_body body = {HTTP_REQUEST_BODY_CSTRING, PHPSTR_VAL(message), PHPSTR_LEN(message)};
				rs = http_post(uri, &body, Z_ARRVAL(options), NULL, NULL);
			} else
			if (!strcasecmp("GET", message->info.request.method)) {
				rs = http_get(uri, Z_ARRVAL(options), NULL, NULL);
			} else
			if (!strcasecmp("HEAD", message->info.request.method)) {
				rs = http_head(uri, Z_ARRVAL(options), NULL, NULL);
			} else {
				http_error_ex(E_WARNING, HTTP_E_MSG,
					"Cannot send HttpMessage. Request method %s not supported",
					message->info.request.method);
			}

			efree(uri);
#else
			http_error(E_WARNING, HTTP_E_MSG, "HTTP requests not supported - ext/http was not linked against libcurl.");
#endif
		}
		break;

		case HTTP_MSG_NONE:
		default:
			http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is neither of type HTTP_MSG_REQUEST nor HTTP_MSG_RESPONSE");
		break;
	}

	return rs;
}

PHP_HTTP_API http_message *_http_message_dup(http_message *msg TSRMLS_DC)
{
	/*
	 * TODO: unroll
	 */
	http_message *new;
	char *serialized_data;
	size_t serialized_length;

	http_message_serialize(msg, &serialized_data, &serialized_length);
	new = http_message_parse(serialized_data, serialized_length);
	efree(serialized_data);
	return new;
}

PHP_HTTP_API void _http_message_dtor(http_message *message)
{
	if (message) {
		zend_hash_destroy(&message->hdrs);
		phpstr_dtor(PHPSTR(message));
		if (HTTP_MSG_TYPE(REQUEST, message)) {
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
		if (message->parent) {
			http_message_free(message->parent);
			message->parent = NULL;
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
