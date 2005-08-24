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

ZEND_EXTERN_MODULE_GLOBALS(http);

#define http_message_info_callback _http_message_info_callback
static void _http_message_info_callback(http_message **message, HashTable **headers, http_info *info TSRMLS_DC)
{
	http_message *old = *message;
	
	/* advance message */
	if (old->type || zend_hash_num_elements(&old->hdrs) || PHPSTR_LEN(old)) {
		(*message) = http_message_new();
		(*message)->parent = old;
		(*headers) = &((*message)->hdrs);
	}
	
	(*message)->http.version = info->http.version;
	
	switch (info->type)
	{
		case IS_HTTP_REQUEST:
			(*message)->type = HTTP_MSG_REQUEST;
			HTTP_INFO(*message).request.URI = estrdup(HTTP_INFO(info).request.URI);
			HTTP_INFO(*message).request.method = estrdup(HTTP_INFO(info).request.method);
		break;
		
		case IS_HTTP_RESPONSE:
			(*message)->type = HTTP_MSG_RESPONSE;
			HTTP_INFO(*message).response.code = HTTP_INFO(info).response.code;
			HTTP_INFO(*message).response.status = estrdup(HTTP_INFO(info).response.status);
		break;
	}
}

#define http_message_init_type _http_message_init_type
static inline void _http_message_init_type(http_message *message, http_message_type type)
{
	message->http.version = .0;
	
	switch (message->type = type)
	{
		case HTTP_MSG_RESPONSE:
			message->http.info.response.code = 0;
			message->http.info.response.status = NULL;
		break;

		case HTTP_MSG_REQUEST:
			message->http.info.request.method = NULL;
			message->http.info.request.URI = NULL;
		break;

		case HTTP_MSG_NONE:
		default:
		break;
	}
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
		switch (message->type)
		{
			case HTTP_MSG_REQUEST:
				STR_FREE(message->http.info.request.method);
				STR_FREE(message->http.info.request.URI);
			break;
			
			case HTTP_MSG_RESPONSE:
				STR_FREE(message->http.info.response.status);
			break;
			
			default:
			break;
		}

		/* init */
		http_message_init_type(message, type);
	}
}

PHP_HTTP_API http_message *_http_message_parse_ex(http_message *msg, const char *message, size_t message_length TSRMLS_DC)
{
	const char *body = NULL;
	zend_bool free_msg = msg ? 0 : 1;

	if ((!message) || (message_length < HTTP_MSG_MIN_SIZE)) {
		return NULL;
	}

	msg = http_message_init(msg);

	if (SUCCESS != http_parse_headers_cb(message, &msg->hdrs, 1, (http_info_callback) http_message_info_callback, &msg)) {
		if (free_msg) {
			http_message_free(&msg);
		}
		return NULL;
	}

	/* header parsing stops at (CR)LF (CR)LF */
	if (body = http_locate_body(message)) {
		zval *c;
		const char *continue_at = NULL;

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
		if (HTTP_MSG_TYPE(RESPONSE, msg)) {
			phpstr_from_string_ex(PHPSTR(msg), body, message + message_length - body);
		} else {
			continue_at = body;
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
				msg->http.info.request.method,
				msg->http.info.request.URI,
				msg->http.version);
		break;

		case HTTP_MSG_RESPONSE:
			phpstr_appendf(&str, "HTTP/%1.1f %d%s%s" HTTP_CRLF,
				msg->http.version,
				msg->http.info.response.code,
				*msg->http.info.response.status ? " ":"",
				msg->http.info.response.status);
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

PHP_HTTP_API void _http_message_tostruct_recursive(http_message *msg, zval *obj TSRMLS_DC)
{
	zval strct;
	zval *headers;
	
	Z_TYPE(strct) = IS_ARRAY;
	Z_ARRVAL(strct) = HASH_OF(obj);
	
	add_assoc_long(&strct, "type", msg->type);
	add_assoc_double(&strct, "httpVersion", msg->http.version);
	switch (msg->type)
	{
		case HTTP_MSG_RESPONSE:
			add_assoc_long(&strct, "responseCode", msg->http.info.response.code);
			add_assoc_string(&strct, "responseStatus", msg->http.info.response.status, 1);
		break;
		
		case HTTP_MSG_REQUEST:
			add_assoc_string(&strct, "requestMethod", msg->http.info.request.method, 1);
			add_assoc_string(&strct, "requestUri", msg->http.info.request.URI, 1);
		break;
	}
	
	MAKE_STD_ZVAL(headers);
	array_init(headers);
	zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	add_assoc_zval(&strct, "headers", headers);
	zval_ptr_dtor(&headers);
	
	add_assoc_stringl(&strct, "body", PHPSTR_VAL(msg), PHPSTR_LEN(msg), 1);
	
	if (msg->parent) {
		zval *parent;
		
		MAKE_STD_ZVAL(parent);
		if (Z_TYPE_P(obj) == IS_ARRAY) {
			array_init(parent);
		} else {
			object_init(parent);
		}
		add_assoc_zval(&strct, "parentMessage", parent);
		http_message_tostruct_recursive(msg->parent, parent);
		zval_ptr_dtor(&parent);
	} else {
		add_assoc_null(&strct, "parentMessage");
	}
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
					if (Z_TYPE_PP(val) == IS_ARRAY) {
						zend_bool first = 1;
						zval **data;
						
						FOREACH_VAL(*val, data) {
							http_send_header_ex(key, strlen(key), Z_STRVAL_PP(data), Z_STRLEN_PP(data), first, NULL);
							first = 0;
						}
					} else {
						http_send_header_ex(key, strlen(key), Z_STRVAL_PP(val), Z_STRLEN_PP(val), 1, NULL);
					}
					key = NULL;
				}
			}
			rs =	SUCCESS == http_send_status(message->http.info.response.code) &&
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
					message->http.info.request.URI, strlen(message->http.info.request.URI),
					NULL, 0, host, host_len, port);
				efree(host);
			} else {
				uri = http_absolute_uri(message->http.info.request.URI);
			}

			if (!strcasecmp("POST", message->http.info.request.method)) {
				http_request_body body = {HTTP_REQUEST_BODY_CSTRING, PHPSTR_VAL(message), PHPSTR_LEN(message)};
				rs = http_post(uri, &body, Z_ARRVAL(options), NULL, NULL);
			} else
			if (!strcasecmp("GET", message->http.info.request.method)) {
				rs = http_get(uri, Z_ARRVAL(options), NULL, NULL);
			} else
			if (!strcasecmp("HEAD", message->http.info.request.method)) {
				rs = http_head(uri, Z_ARRVAL(options), NULL, NULL);
			} else {
				http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD,
					"Cannot send HttpMessage. Request method %s not supported",
					message->http.info.request.method);
			}

			efree(uri);
#else
			http_error(HE_WARNING, HTTP_E_RUNTIME, "HTTP requests not supported - ext/http was not linked against libcurl.");
#endif
		}
		break;

		case HTTP_MSG_NONE:
		default:
			http_error(HE_WARNING, HTTP_E_MESSAGE_TYPE, "HttpMessage is neither of type HTTP_MSG_REQUEST nor HTTP_MSG_RESPONSE");
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
		
		switch (message->type)
		{
			case HTTP_MSG_REQUEST:
				STR_SET(message->http.info.request.method, NULL);
				STR_SET(message->http.info.request.URI, NULL);
			break;
			
			case HTTP_MSG_RESPONSE:
				STR_SET(message->http.info.response.status, NULL);
			break;
			
			default:
			break;
		}
	}
}

PHP_HTTP_API void _http_message_free(http_message **message)
{
	if (*message) {
		if ((*message)->parent) {
			http_message_free(&(*message)->parent);
		}
		http_message_dtor(*message);
		efree(*message);
		*message = NULL;
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

