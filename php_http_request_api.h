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

#ifndef PHP_HTTP_REQUEST_API_H
#define PHP_HTTP_REQUEST_API_H

#include "php_http_std_defs.h"
#include "phpstr/phpstr.h"

#ifdef PHP_WIN32
#	include <winsock2.h>
#endif

#include <curl/curl.h>

typedef enum {
	HTTP_NO_REQUEST_METHOD	= 0,
	/* HTTP/1.1 */
	HTTP_GET				= 1,
	HTTP_HEAD				= 2,
	HTTP_POST				= 3,
	HTTP_PUT				= 4,
	HTTP_DELETE				= 5,
	HTTP_OPTIONS			= 6,
	HTTP_TRACE				= 7,
	HTTP_CONNECT			= 8,
	/* WebDAV - RFC 2518 */
	HTTP_PROPFIND			= 9,
	HTTP_PROPPATCH			= 10,
	HTTP_MKCOL				= 11,
	HTTP_COPY				= 12,
	HTTP_MOVE				= 13,
	HTTP_LOCK				= 14,
	HTTP_UNLOCK				= 15,
	/* WebDAV Versioning - RFC 3253 */
	HTTP_VERSION_CONTROL	= 16,
	HTTP_REPORT				= 17,
	HTTP_CHECKOUT			= 18,
	HTTP_CHECKIN			= 19,
	HTTP_UNCHECKOUT			= 20,
	HTTP_MKWORKSPACE		= 21,
	HTTP_UPDATE				= 22,
	HTTP_LABEL				= 23,
	HTTP_MERGE				= 24,
	HTTP_BASELINE_CONTROL	= 25,
	HTTP_MKACTIVITY			= 26,
	/* WebDAV Access Control - RFC 3744 */
	HTTP_ACL				= 27,
	HTTP_MAX_REQUEST_METHOD	= 28
} http_request_method;

#define HTTP_STD_REQUEST_METHOD(m) ((m > HTTP_NO_REQUEST_METHOD) && (m < HTTP_MAX_REQUEST_METHOD))
#define HTTP_CUSTOM_REQUEST_METHOD(m) (m - HTTP_MAX_REQUEST_METHOD)

#define HTTP_REQUEST_BODY_CSTRING		1
#define HTTP_REQUEST_BODY_CURLPOST		2
#define HTTP_REQUEST_BODY_UPLOADFILE	3
typedef struct {
	int type;
	void *data;
	size_t size;
} http_request_body;

typedef struct {
	CURLM *ch;
	zend_llist handles;
	zend_llist bodies;
	int unfinished;
} http_request_pool;

#define COPY_STRING	1
#define	COPY_SLIST	2
#define http_request_data_copy(type, data) _http_request_data_copy((type), (data) TSRMLS_CC)
extern void *_http_request_data_copy(int type, void *data TSRMLS_DC);
#define http_request_data_free_string _http_request_data_free_string
extern void _http_request_data_free_string(void *string);
#define http_request_data_free_slist _http_request_data_free_slist
extern void _http_request_data_free_slist(void *list);

#define http_request_pool_responsehandler _http_request_pool_responsehandler
extern void _http_request_pool_responsehandler(zval **req TSRMLS_DC);

#define http_request_global_init _http_request_global_init
extern STATUS _http_request_global_init(void);

#define http_request_method_name(m) _http_request_method_name((m) TSRMLS_CC)
PHP_HTTP_API const char *_http_request_method_name(http_request_method m TSRMLS_DC);

#define http_request_method_exists(u, l, c) _http_request_method_exists((u), (l), (c) TSRMLS_CC)
PHP_HTTP_API unsigned long _http_request_method_exists(zend_bool by_name, unsigned long id, const char *name TSRMLS_DC);

#define http_request_method_register(m) _http_request_method_register((m) TSRMLS_CC)
PHP_HTTP_API unsigned long _http_request_method_register(const char *method TSRMLS_DC);

#define http_request_method_unregister(mn) _http_request_method_unregister((mn) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_method_unregister(unsigned long method TSRMLS_DC);

#define http_request_body_new() _http_request_body_new(TSRMLS_C)
PHP_HTTP_API http_request_body *_http_request_body_new(TSRMLS_D);

#define http_request_body_fill(b, fields, files) _http_request_body_fill((b), (fields), (files) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_body_fill(http_request_body *body, HashTable *fields, HashTable *files TSRMLS_DC);

#define http_request_body_dtor(b) _http_request_body_dtor((b) TSRMLS_CC)
PHP_HTTP_API void _http_request_body_dtor(http_request_body *body TSRMLS_DC);

#define http_request_body_free(b) _http_request_body_free((b) TSRMLS_CC)
PHP_HTTP_API void _http_request_body_free(http_request_body *body TSRMLS_DC);

#define http_request_pool_init(p) _http_request_pool_init((p) TSRMLS_CC)
PHP_HTTP_API http_request_pool *_http_request_pool_init(http_request_pool *pool TSRMLS_DC);

#define http_request_pool_attach(p, r) _http_request_pool_attach((p), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_pool_attach(http_request_pool *pool, zval *request TSRMLS_DC);

#define http_request_pool_detach(p, r) _http_request_pool_detach((p), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_pool_detach(http_request_pool *pool, zval *request TSRMLS_DC);

#define http_request_pool_detach_all(p) _http_request_pool_detach_all((p) TSRMLS_CC)
PHP_HTTP_API void _http_request_pool_detach_all(http_request_pool *pool TSRMLS_DC);

#define http_request_pool_send(p) _http_request_pool_send((p) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_pool_send(http_request_pool *pool TSRMLS_DC);

#define http_request_pool_select _http_request_pool_select
PHP_HTTP_API STATUS _http_request_pool_select(http_request_pool *pool);

#define http_request_pool_perform _http_request_pool_perform
PHP_HTTP_API int _http_request_pool_perform(http_request_pool *pool);

#define http_request_pool_dtor(p) _http_request_pool_dtor((p) TSRMLS_CC)
PHP_HTTP_API void _http_request_pool_dtor(http_request_pool *pool TSRMLS_DC);

#define http_request_init(ch, meth, url, body, options, response) _http_request_init((ch), (meth), (url), (body), (options), (response) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_init(CURL *ch, http_request_method meth, const char *url, http_request_body *body, HashTable *options, phpstr *response TSRMLS_DC);

#define http_request_exec(ch, i) _http_request_exec((ch), (i) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_exec(CURL *ch, HashTable *info TSRMLS_DC);

#define http_request_info(ch, i) _http_request_info((ch), (i) TSRMLS_CC)
PHP_HTTP_API void _http_request_info(CURL *ch, HashTable *info TSRMLS_DC);

#define http_request(meth, url, body, opt, info, resp) _http_request_ex(NULL, (meth), (url), (body), (opt), (info), (resp) TSRMLS_CC)
#define http_request_ex(ch, meth, url, body, opt, info, resp) _http_request_ex((ch), (meth), (url), (body), (opt), (info), (resp) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_ex(CURL *ch, http_request_method meth, const char *URL, http_request_body *body, HashTable *options, HashTable *info, phpstr *response TSRMLS_DC);

#define http_get(u, o, i, r) _http_request_ex(NULL, HTTP_GET, (u), NULL, (o), (i), (r) TSRMLS_CC)
#define http_get_ex(c, u, o, i, r) _http_request_ex((c), HTTP_GET, (u), NULL, (o), (i), (r) TSRMLS_CC)

#define http_head(u, o, i, r) _http_request_ex(NULL, HTTP_HEAD, (u), NULL, (o), (i), (r) TSRMLS_CC)
#define http_head_ex(c, u, o, i, r) _http_request_ex((c), HTTP_HEAD, (u), NULL, (o), (i), (r) TSRMLS_CC)

#define http_post(u, b, o, i, r) _http_request_ex(NULL, HTTP_POST, (u), (b), (o), (i), (r) TSRMLS_CC)
#define http_post_ex(c, u, b, o, i, r) _http_request_ex((c), HTTP_POST, (u), (b), (o), (i), (r) TSRMLS_CC)

#define http_put(u, b, o, i, r) _http_request_ex(NULL, HTTP_PUT, (u), (b), (o), (i), (r) TSRMLS_CC)
#define http_put_ex(c, u, b, o, i, r) _http_request_ex((c), HTTP_PUT, (u), (b), (o), (i), (r) TSRMLS_CC)

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

