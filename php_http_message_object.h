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
} http_message_object;

extern zend_class_entry *http_message_object_ce;
extern zend_function_entry http_message_object_fe[];

#define http_message_object_init _http_message_object_init
extern void _http_message_object_init(INIT_FUNC_ARGS);
#define http_message_object_new _http_message_object_new
extern zend_object_value _http_message_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_message_object_free _http_message_object_free
extern void _http_message_object_free(zend_object *object TSRMLS_DC);

#define HTTP_MSG_PROPHASH_TYPE                 276192743LU
#define HTTP_MSG_PROPHASH_HTTP_VERSION         1138628683LU
#define HTTP_MSG_PROPHASH_RAW                  2090679983LU
#define HTTP_MSG_PROPHASH_BODY                 254474387LU
#define HTTP_MSG_PROPHASH_HEADERS              3199929089LU
#define HTTP_MSG_PROPHASH_NESTED_MESSAGE       3652857165LU
#define HTTP_MSG_PROPHASH_REQUEST_METHOD       1669022159LU
#define HTTP_MSG_PROPHASH_REQUEST_URI          3208695486LU
#define HTTP_MSG_PROPHASH_RESPONSE_STATUS      3857097400LU

PHP_METHOD(HttpMessage, __construct);
PHP_METHOD(HttpMessage, __destruct);

#endif
#endif
