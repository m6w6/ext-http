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

#ifndef PHP_HTTP_MESSAGE_OBJECT_H
#define PHP_HTTP_MESSAGE_OBJECT_H
#ifdef ZEND_ENGINE_2

#include "php_http_message_api.h"

typedef struct {
	zend_object zo;
	http_message *message;
	zend_object_value parent;
} http_message_object;

extern zend_class_entry *http_message_object_ce;
extern zend_function_entry http_message_object_fe[];

#define http_message_object_init() _http_message_object_init(INIT_FUNC_ARGS_PASSTHRU)
extern void _http_message_object_init(INIT_FUNC_ARGS);
#define http_message_object_new _http_message_object_new
extern zend_object_value _http_message_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_message_object_new_ex(ce, msg) _http_message_object_new_ex(ce, msg TSRMLS_CC)
#define http_message_object_from_msg(msg) _http_message_object_new_ex(http_message_object_ce, msg TSRMLS_CC)
extern zend_object_value _http_message_object_new_ex(zend_class_entry *ce, http_message *msg TSRMLS_DC);
#define http_message_object_clone(zobj) _http_message_object_clone(zobj TSRMLS_CC)
extern zend_object_value _http_message_object_clone(zval *object TSRMLS_DC);
#define http_message_object_free _http_message_object_free
extern void _http_message_object_free(zend_object *object TSRMLS_DC);

#define HTTP_MSG_PROPHASH_TYPE                  276192743LU
#define HTTP_MSG_PROPHASH_HTTP_VERSION         1138628683LU
#define HTTP_MSG_PROPHASH_BODY                  254474387LU
#define HTTP_MSG_PROPHASH_HEADERS              3199929089LU
#define HTTP_MSG_PROPHASH_PARENT_MESSAGE       2105714836LU
#define HTTP_MSG_PROPHASH_REQUEST_METHOD       1669022159LU
#define HTTP_MSG_PROPHASH_REQUEST_URI          3208695486LU
#define HTTP_MSG_PROPHASH_RESPONSE_STATUS      3857097400LU
#define HTTP_MSG_PROPHASH_RESPONSE_CODE        1305615119LU

#define HTTP_MSG_CHECK_OBJ(obj, dofail) \
	if (!(obj)->message) { \
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is empty"); \
		dofail; \
	}
#define HTTP_MSG_CHECK_STD() HTTP_MSG_CHECK_OBJ(obj, RETURN_FALSE)

#define HTTP_MSG_INIT_OBJ(obj) \
	if (!(obj)->message) { \
		(obj)->message = http_message_new(); \
	}
#define HTTP_MSG_INIT_STD() HTTP_MSG_INIT_OBJ(obj)

PHP_METHOD(HttpMessage, __construct);
PHP_METHOD(HttpMessage, getBody);
PHP_METHOD(HttpMessage, getHeaders);
PHP_METHOD(HttpMessage, setHeaders);
PHP_METHOD(HttpMessage, addHeaders);
PHP_METHOD(HttpMessage, getType);
PHP_METHOD(HttpMessage, setType);
PHP_METHOD(HttpMessage, getResponseCode);
PHP_METHOD(HttpMessage, setResponseCode);
PHP_METHOD(HttpMessage, getRequestMethod);
PHP_METHOD(HttpMessage, setRequestMethod);
PHP_METHOD(HttpMessage, getRequestUri);
PHP_METHOD(HttpMessage, setRequestUri);
PHP_METHOD(HttpMessage, getHttpVersion);
PHP_METHOD(HttpMessage, setHttpVersion);
PHP_METHOD(HttpMessage, getParentMessage);
PHP_METHOD(HttpMessage, send);
PHP_METHOD(HttpMessage, toString);

PHP_METHOD(HttpMessage, fromString);

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

