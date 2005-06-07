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

HTTP_DECLARE_ARG_PASS_INFO();

zend_class_entry *http_util_object_ce;
zend_function_entry http_util_object_fe[] = {
	HTTP_STATIC_ME_ALIAS(date, http_date, NULL)
	HTTP_STATIC_ME_ALIAS(absoluteURI, http_absolute_uri, NULL)
	HTTP_STATIC_ME_ALIAS(negotiateLanguage, http_negotiate_language, NULL)
	HTTP_STATIC_ME_ALIAS(negotiateCharset, http_negotiate_charset, NULL)
	HTTP_STATIC_ME_ALIAS(redirect, http_redirect, NULL)
	HTTP_STATIC_ME_ALIAS(sendStatus, http_send_status, NULL)
	HTTP_STATIC_ME_ALIAS(sendLastModified, http_send_last_modified, NULL)
	HTTP_STATIC_ME_ALIAS(sendContentType, http_send_content_type, NULL)
	HTTP_STATIC_ME_ALIAS(sendContentDisposition, http_send_content_disposition, NULL)
	HTTP_STATIC_ME_ALIAS(matchModified, http_match_modified, NULL)
	HTTP_STATIC_ME_ALIAS(matchEtag, http_match_etag, NULL)
	HTTP_STATIC_ME_ALIAS(cacheLastModified, http_cache_last_modified, NULL)
	HTTP_STATIC_ME_ALIAS(cacheEtag, http_cache_etag, NULL)
	HTTP_STATIC_ME_ALIAS(chunkedDecode, http_chunked_decode, NULL)
	HTTP_STATIC_ME_ALIAS(splitResponse, http_split_response, NULL)
	HTTP_STATIC_ME_ALIAS(parseHeaders, http_parse_headers, NULL)
	HTTP_STATIC_ME_ALIAS(getRequestHeaders, http_get_request_headers, NULL)
#ifdef HTTP_HAVE_CURL
	HTTP_STATIC_ME_ALIAS(get, http_get, http_arg_pass_ref_3)
	HTTP_STATIC_ME_ALIAS(head, http_head, http_arg_pass_ref_3)
	HTTP_STATIC_ME_ALIAS(postData, http_post_data, http_arg_pass_ref_4)
	HTTP_STATIC_ME_ALIAS(postFields, http_post_fields, http_arg_pass_ref_5)
#endif /* HTTP_HAVE_CURL */
	HTTP_STATIC_ME_ALIAS(authBasic, http_auth_basic, NULL)
	HTTP_STATIC_ME_ALIAS(authBasicCallback, http_auth_basic_cb, NULL)
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

