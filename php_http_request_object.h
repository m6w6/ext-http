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

#ifndef PHP_HTTP_REQUEST_OBJECT_H
#define PHP_HTTP_REQUEST_OBJECT_H
#ifdef HTTP_HAVE_CURL
#ifdef ZEND_ENGINE_2

#ifdef	PHP_WIN32
#	include <winsock2.h>
#endif

#include <curl/curl.h>

#include "php_http_request_api.h"
#include "php_http_request_pool_api.h"
#include "phpstr/phpstr.h"

typedef struct {
	zend_object zo;
	CURL *ch;
	http_request_pool *pool;
	phpstr response;
	phpstr request;
	phpstr history;
} http_request_object;

extern zend_class_entry *http_request_object_ce;
extern zend_function_entry http_request_object_fe[];

extern PHP_MINIT_FUNCTION(http_request_object);

#define http_request_object_new _http_request_object_new
extern zend_object_value _http_request_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_request_object_new_ex(ce, ch) _http_request_object_new_ex((ce), (ch) TSRMLS_CC)
#define http_request_object_from_curl(ch) _http_request_object_new_ex(http_request_object_ce, (ch) TSRMLS_CC)
extern zend_object_value _http_request_object_new_ex(zend_class_entry *ce, CURL *ch TSRMLS_DC);
#define http_request_object_free _http_request_object_free
extern void _http_request_object_free(zend_object *object TSRMLS_DC);
#define http_request_object_clone(o) _http_request_object_clone((o) TSRMLS_CC)
extern zend_object_value _http_request_object_clone(zval *object TSRMLS_DC);

#define http_request_object_requesthandler(req, this, body) _http_request_object_requesthandler((req), (this), (body) TSRMLS_CC)
extern STATUS _http_request_object_requesthandler(http_request_object *obj, zval *this_ptr, http_request_body *body TSRMLS_DC);
#define http_request_object_responsehandler(req, this) _http_request_object_responsehandler((req), (this) TSRMLS_CC)
extern STATUS _http_request_object_responsehandler(http_request_object *obj, zval *this_ptr TSRMLS_DC);

PHP_METHOD(HttpRequest, __construct);
PHP_METHOD(HttpRequest, __destruct);
PHP_METHOD(HttpRequest, setOptions);
PHP_METHOD(HttpRequest, getOptions);
PHP_METHOD(HttpRequest, setSslOptions);
PHP_METHOD(HttpRequest, getSslOptions);
PHP_METHOD(HttpRequest, addHeaders);
PHP_METHOD(HttpRequest, getHeaders);
PHP_METHOD(HttpRequest, setHeaders);
PHP_METHOD(HttpRequest, addCookies);
PHP_METHOD(HttpRequest, getCookies);
PHP_METHOD(HttpRequest, setCookies);
PHP_METHOD(HttpRequest, setMethod);
PHP_METHOD(HttpRequest, getMethod);
PHP_METHOD(HttpRequest, setUrl);
PHP_METHOD(HttpRequest, getUrl);
PHP_METHOD(HttpRequest, setContentType);
PHP_METHOD(HttpRequest, getContentType);
PHP_METHOD(HttpRequest, setQueryData);
PHP_METHOD(HttpRequest, getQueryData);
PHP_METHOD(HttpRequest, addQueryData);
PHP_METHOD(HttpRequest, setPostFields);
PHP_METHOD(HttpRequest, getPostFields);
PHP_METHOD(HttpRequest, addPostFields);
PHP_METHOD(HttpRequest, getRawPostData);
PHP_METHOD(HttpRequest, setRawPostData);
PHP_METHOD(HttpRequest, addRawPostData);
PHP_METHOD(HttpRequest, addPostFile);
PHP_METHOD(HttpRequest, setPostFiles);
PHP_METHOD(HttpRequest, getPostFiles);
PHP_METHOD(HttpRequest, setPutFile);
PHP_METHOD(HttpRequest, getPutFile);
PHP_METHOD(HttpRequest, send);
PHP_METHOD(HttpRequest, getResponseData);
PHP_METHOD(HttpRequest, getResponseHeader);
PHP_METHOD(HttpRequest, getResponseCookie);
PHP_METHOD(HttpRequest, getResponseCode);
PHP_METHOD(HttpRequest, getResponseBody);
PHP_METHOD(HttpRequest, getResponseInfo);
PHP_METHOD(HttpRequest, getResponseMessage);
PHP_METHOD(HttpRequest, getRequestMessage);
PHP_METHOD(HttpRequest, getHistory);
PHP_METHOD(HttpRequest, clearHistory);

PHP_METHOD(HttpRequest, get);
PHP_METHOD(HttpRequest, head);
PHP_METHOD(HttpRequest, postData);
PHP_METHOD(HttpRequest, postFields);
PHP_METHOD(HttpRequest, putFile);
PHP_METHOD(HttpRequest, putStream);

PHP_METHOD(HttpRequest, methodRegister);
PHP_METHOD(HttpRequest, methodUnregister);
PHP_METHOD(HttpRequest, methodName);
PHP_METHOD(HttpRequest, methodExists);

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

