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

#ifndef PHP_EXT_HTTP_H
#define PHP_EXT_HTTP_H

#define HTTP_PEXT_VERSION "0.7.0-dev"

/* make compile on Win32 */
#include "php_streams.h"
#include "ext/standard/md5.h"
#include "phpstr/phpstr.h"

extern zend_module_entry http_module_entry;
#define phpext_http_ptr &http_module_entry

#ifdef ZTS
#	include "TSRM.h"
#	define HTTP_G(v) TSRMG(http_globals_id, zend_http_globals *, v)
#else
#	define HTTP_G(v) (http_globals.v)
#endif

#ifdef ZEND_ENGINE_2

typedef struct {
	zend_object	zo;
} http_response_object;

#ifdef HTTP_HAVE_CURL

#ifdef	PHP_WIN32
#	include <winsock2.h>
#endif

#include <curl/curl.h>

typedef struct {
	zend_object zo;
	CURL *ch;
} http_request_object;

typedef enum {
	HTTP_GET = 1,
	HTTP_HEAD,
	HTTP_POST,
} http_request_method;

#endif /* HTTP_HAVE _CURL */

PHP_METHOD(HttpUtil, date);
PHP_METHOD(HttpUtil, absoluteURI);
PHP_METHOD(HttpUtil, negotiateLanguage);
PHP_METHOD(HttpUtil, negotiateCharset);
PHP_METHOD(HttpUtil, redirect);
PHP_METHOD(HttpUtil, sendStatus);
PHP_METHOD(HttpUtil, sendLastModified);
PHP_METHOD(HttpUtil, sendContentType);
PHP_METHOD(HttpUtil, sendContentDisposition);
PHP_METHOD(HttpUtil, matchModified);
PHP_METHOD(HttpUtil, matchEtag);
PHP_METHOD(HttpUtil, cacheLastModified);
PHP_METHOD(HttpUtil, cacheEtag);
PHP_METHOD(HttpUtil, chunkedDecode);
PHP_METHOD(HttpUtil, splitResponse);
PHP_METHOD(HttpUtil, parseHeaders);
PHP_METHOD(HttpUtil, getRequestHeaders);
#ifdef HTTP_HAVE_CURL
PHP_METHOD(HttpUtil, get);
PHP_METHOD(HttpUtil, head);
PHP_METHOD(HttpUtil, postData);
PHP_METHOD(HttpUtil, postArray);
#endif /* HTTP_HAVE_CURL */
PHP_METHOD(HttpUtil, authBasic);
PHP_METHOD(HttpUtil, authBasicCallback);


PHP_METHOD(HttpResponse, __construct);/*
PHP_METHOD(HttpResponse, __destruct);*/
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
PHP_METHOD(HttpResponse, setData);
PHP_METHOD(HttpResponse, getData);
PHP_METHOD(HttpResponse, setFile);
PHP_METHOD(HttpResponse, getFile);
PHP_METHOD(HttpResponse, setStream);
PHP_METHOD(HttpResponse, getStream);
PHP_METHOD(HttpResponse, send);

#ifdef HTTP_HAVE_CURL

PHP_METHOD(HttpRequest, __construct);
PHP_METHOD(HttpRequest, __destruct);
PHP_METHOD(HttpRequest, setOptions);
PHP_METHOD(HttpRequest, getOptions);
PHP_METHOD(HttpRequest, unsetOptions);
PHP_METHOD(HttpRequest, setSslOptions);
PHP_METHOD(HttpRequest, getSslOptions);
PHP_METHOD(HttpRequest, unsetSslOptions);
PHP_METHOD(HttpRequest, addHeaders);
PHP_METHOD(HttpRequest, getHeaders);
PHP_METHOD(HttpRequest, unsetHeaders);
PHP_METHOD(HttpRequest, addCookies);
PHP_METHOD(HttpRequest, getCookies);
PHP_METHOD(HttpRequest, unsetCookies);
PHP_METHOD(HttpRequest, setMethod);
PHP_METHOD(HttpRequest, getMethod);
PHP_METHOD(HttpRequest, setURL);
PHP_METHOD(HttpRequest, getURL);
PHP_METHOD(HttpRequest, setContentType);
PHP_METHOD(HttpRequest, getContentType);
PHP_METHOD(HttpRequest, setQueryData);
PHP_METHOD(HttpRequest, getQueryData);
PHP_METHOD(HttpRequest, addQueryData);
PHP_METHOD(HttpRequest, unsetQueryData);
PHP_METHOD(HttpRequest, setPostData);
PHP_METHOD(HttpRequest, getPostData);
PHP_METHOD(HttpRequest, addPostData);
PHP_METHOD(HttpRequest, unsetPostData);
PHP_METHOD(HttpRequest, addPostFile);
PHP_METHOD(HttpRequest, getPostFiles);
PHP_METHOD(HttpRequest, unsetPostFiles);
PHP_METHOD(HttpRequest, send);
PHP_METHOD(HttpRequest, getResponseData);
PHP_METHOD(HttpRequest, getResponseHeader);
PHP_METHOD(HttpRequest, getResponseCookie);
PHP_METHOD(HttpRequest, getResponseCode);
PHP_METHOD(HttpRequest, getResponseBody);
PHP_METHOD(HttpRequest, getResponseInfo);

#endif /* HTTP_HAVE_CURL */

#endif /* ZEND_ENGINE_2 */

PHP_FUNCTION(http_test);
PHP_FUNCTION(http_date);
PHP_FUNCTION(http_absolute_uri);
PHP_FUNCTION(http_negotiate_language);
PHP_FUNCTION(http_negotiate_charset);
PHP_FUNCTION(http_redirect);
PHP_FUNCTION(http_send_status);
PHP_FUNCTION(http_send_last_modified);
PHP_FUNCTION(http_send_content_type);
PHP_FUNCTION(http_send_content_disposition);
PHP_FUNCTION(http_match_modified);
PHP_FUNCTION(http_match_etag);
PHP_FUNCTION(http_cache_last_modified);
PHP_FUNCTION(http_cache_etag);
PHP_FUNCTION(http_send_data);
PHP_FUNCTION(http_send_file);
PHP_FUNCTION(http_send_stream);
PHP_FUNCTION(http_chunked_decode);
PHP_FUNCTION(http_split_response);
PHP_FUNCTION(http_parse_headers);
PHP_FUNCTION(http_get_request_headers);
#ifdef HTTP_HAVE_CURL
PHP_FUNCTION(http_get);
PHP_FUNCTION(http_head);
PHP_FUNCTION(http_post_data);
PHP_FUNCTION(http_post_array);
#endif /* HTTP_HAVE_CURL */
PHP_FUNCTION(http_auth_basic);
PHP_FUNCTION(http_auth_basic_cb);
#ifndef ZEND_ENGINE_2
PHP_FUNCTION(http_build_query);
#endif /* ZEND_ENGINE_2 */
PHP_FUNCTION(ob_httpetaghandler);

PHP_MINIT_FUNCTION(http);
PHP_MSHUTDOWN_FUNCTION(http);
PHP_RINIT_FUNCTION(http);
PHP_RSHUTDOWN_FUNCTION(http);
PHP_MINFO_FUNCTION(http);

ZEND_BEGIN_MODULE_GLOBALS(http)
	zend_bool etag_started;
	PHP_MD5_CTX etag_md5;
	php_stream_statbuf ssb;
	char *ctype;
	char *etag;
	time_t lmod;
	char *allowed_methods;
#ifdef HTTP_HAVE_CURL
	phpstr curlbuf;
	zend_llist to_free;
#endif /* HTTP_HAVE_CURL */
ZEND_END_MODULE_GLOBALS(http)

#endif	/* PHP_HTTP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

