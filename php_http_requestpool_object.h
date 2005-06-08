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

#ifndef PHP_HTTP_REQUESTPOOL_OBJECT_H
#define PHP_HTTP_REQUESTPOOL_OBJECT_H
#ifdef HTTP_HAVE_CURL
#ifdef ZEND_ENGINE_2

#ifdef	PHP_WIN32
#	include <winsock2.h>
#endif

#include <curl/curl.h>

#include "php_http_request_api.h"

typedef struct {
	zend_object zo;
	http_request_pool pool;
} http_requestpool_object;

extern zend_class_entry *http_requestpool_object_ce;
extern zend_function_entry http_requestpool_object_fe[];

#define http_requestpool_object_init() _http_requestpool_object_init(INIT_FUNC_ARGS_PASSTHRU)
extern void _http_requestpool_object_init(INIT_FUNC_ARGS);
#define http_requestpool_object_new _http_requestpool_object_new
extern zend_object_value _http_requestpool_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_requestpool_object_free _http_requestpool_object_free
extern void _http_requestpool_object_free(zend_object *object TSRMLS_DC);
#define http_requestpool_object_ondestruct(p) _http_requestpool_object_ondestruct((p) TSRMLS_CC)
extern void _http_requestpool_object_ondestruct(http_request_pool *pool TSRMLS_DC);

PHP_METHOD(HttpRequestPool, __construct);
PHP_METHOD(HttpRequestPool, __destruct);
PHP_METHOD(HttpRequestPool, attach);
PHP_METHOD(HttpRequestPool, detach);
PHP_METHOD(HttpRequestPool, send);

#endif
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

