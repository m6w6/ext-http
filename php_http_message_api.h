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

/*
DUMP:
HttpMessage
	->toResponseString();
	->toRequestString();
	->__toString();	->__sleep(); ->serialize();
	->fromString(); __wakeup($message); ->unserialize();
	->setStatusCode();
	->setHeader(); ->addHeader()...
*/

#include "phpstr/phpstr.h"

typedef enum {
	HTTP_MSG_NONE,
	HTTP_MSG_REQUEST,
	HTTP_MSG_RESPONSE
} http_message_type;

typedef struct _http_message http_message;

struct _http_message {
	phpstr body;
	HashTable hdrs;
	http_message_type type;

	union {
		struct {
			float http_version;
			char *method;
			char *URI;
		} request;

		struct {
			float http_version;
			int status;
		} response;

	} info;

	size_t 		len;
	zend_bool 	dup;

	union {
		char *dup;
		const char *ptr;
	} raw;

	http_message *nested;
};

/* required minimum length of an HTTP message "HTTP/1.1 200\r\n" */
#define HTTP_MSG_MIN_SIZE 15

#define HTTP_MSG_RAW(msg) ( (msg)->dup ? msg->raw.dup : msg->raw.ptr )

#define http_message_init() _http_message_init_ex(NULL, 0)
#define http_message_init_ex(m, t) _http_message_init_ex((m), (t))
#define http_message_free(m) _http_message_free((m))
#define http_message_dtor(m) _http_message_dtor((m))

PHP_HTTP_API void _http_message_dtor(http_message *message);
PHP_HTTP_API void _http_message_free(http_message *message);

#define http_message_parse(m, l) http_message_parse_ex((m), (l), 1)
#define http_message_parse_ex(m, l, d) _http_message_parse_ex((m), (l), (d) TSRMLS_CC)
PHP_HTTP_API http_message *_http_message_parse_ex(const char *message, size_t length, zend_bool duplicate TSRMLS_DC);

#define http_message_tostring(m, s, l) _http_message_tostring((m), (s), (l))
PHP_HTTP_API void _http_message_tostring(http_message *msg, char **string, size_t *length);


