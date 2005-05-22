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

#ifndef PHP_HTTP_RESPONSE_OBJECT_H
#define PHP_HTTP_RESPONSE_OBJECT_H
#ifdef ZEND_ENGINE_2

typedef struct {
	zend_object	zo;
} http_response_object;

extern zend_class_entry *http_response_object_ce;
extern zend_function_entry http_response_object_fe[];

#define http_response_object_init() _http_response_object_init(INIT_FUNC_ARGS_PASSTHRU)
extern void _http_response_object_init(INIT_FUNC_ARGS);
#define http_response_object_new _http_response_object_new
extern zend_object_value _http_response_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_response_object_free _http_response_object_free
extern void _http_response_object_free(zend_object *object TSRMLS_DC);

PHP_METHOD(HttpResponse, __construct);
PHP_METHOD(HttpResponse, setETag);
PHP_METHOD(HttpResponse, getETag);
PHP_METHOD(HttpResponse, setContentDisposition);
PHP_METHOD(HttpResponse, getContentDisposition);
PHP_METHOD(HttpResponse, setContentType);
PHP_METHOD(HttpResponse, getContentType);
PHP_METHOD(HttpResponse, setCache);
PHP_METHOD(HttpResponse, getCache);
PHP_METHOD(HttpResponse, setCacheControl);
PHP_METHOD(HttpResponse, getCacheControl);
PHP_METHOD(HttpResponse, setGzip);
PHP_METHOD(HttpResponse, getGzip);
PHP_METHOD(HttpResponse, setThrottleDelay);
PHP_METHOD(HttpResponse, getThrottleDelay);
PHP_METHOD(HttpResponse, setSendBuffersize);
PHP_METHOD(HttpResponse, getSendBuffersize);
PHP_METHOD(HttpResponse, setData);
PHP_METHOD(HttpResponse, getData);
PHP_METHOD(HttpResponse, setFile);
PHP_METHOD(HttpResponse, getFile);
PHP_METHOD(HttpResponse, setStream);
PHP_METHOD(HttpResponse, getStream);
PHP_METHOD(HttpResponse, send);

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

