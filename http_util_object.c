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
#include "php_http_util_object.h"

#ifdef ZEND_ENGINE_2

#ifdef HTTP_HAVE_CURL
static
ZEND_BEGIN_ARG_INFO(http_request_info_ref_3, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();

static
ZEND_BEGIN_ARG_INFO(http_request_info_ref_4, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();
#endif /* HTTP_HAVE_CURL */

zend_class_entry *http_util_object_ce;
zend_function_entry http_util_object_fe[] = {
#define HTTP_UTIL_ME(me, al, ai) ZEND_FENTRY(me, ZEND_FN(al), ai, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	HTTP_UTIL_ME(date, http_date, NULL)
	HTTP_UTIL_ME(absoluteURI, http_absolute_uri, NULL)
	HTTP_UTIL_ME(negotiateLanguage, http_negotiate_language, NULL)
	HTTP_UTIL_ME(negotiateCharset, http_negotiate_charset, NULL)
	HTTP_UTIL_ME(redirect, http_redirect, NULL)
	HTTP_UTIL_ME(sendStatus, http_send_status, NULL)
	HTTP_UTIL_ME(sendLastModified, http_send_last_modified, NULL)
	HTTP_UTIL_ME(sendContentType, http_send_content_type, NULL)
	HTTP_UTIL_ME(sendContentDisposition, http_send_content_disposition, NULL)
	HTTP_UTIL_ME(matchModified, http_match_modified, NULL)
	HTTP_UTIL_ME(matchEtag, http_match_etag, NULL)
	HTTP_UTIL_ME(cacheLastModified, http_cache_last_modified, NULL)
	HTTP_UTIL_ME(cacheEtag, http_cache_etag, NULL)
	HTTP_UTIL_ME(chunkedDecode, http_chunked_decode, NULL)
	HTTP_UTIL_ME(splitResponse, http_split_response, NULL)
	HTTP_UTIL_ME(parseHeaders, http_parse_headers, NULL)
	HTTP_UTIL_ME(getRequestHeaders, http_get_request_headers, NULL)
#ifdef HTTP_HAVE_CURL
	HTTP_UTIL_ME(get, http_get, http_request_info_ref_3)
	HTTP_UTIL_ME(head, http_head, http_request_info_ref_3)
	HTTP_UTIL_ME(postData, http_post_data, http_request_info_ref_4)
	HTTP_UTIL_ME(postArray, http_post_array, http_request_info_ref_4)
#endif /* HTTP_HAVE_CURL */
	HTTP_UTIL_ME(authBasic, http_auth_basic, NULL)
	HTTP_UTIL_ME(authBasicCallback, http_auth_basic_cb, NULL)
	{NULL, NULL, NULL}
};

void _http_util_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS(HttpUtil, http_util_object, NULL, ZEND_ACC_FINAL_CLASS);
}

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

