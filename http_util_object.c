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

#ifdef ZEND_ENGINE_2

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_util_object.h"

#define HTTP_BEGIN_ARGS(method, req_args) 		HTTP_BEGIN_ARGS_EX(HttpUtil, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)		HTTP_EMPTY_ARGS_EX(HttpUtil, method, ret_ref)

#define HTTP_UTIL_ALIAS(method, func)			HTTP_STATIC_ME_ALIAS(method, func, HTTP_ARGS(HttpUtil, method))

HTTP_BEGIN_ARGS(date, 0)
	HTTP_ARG_VAL(timestamp, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(absoluteUri, 1)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(proto, 0)
	HTTP_ARG_VAL(host, 0)
	HTTP_ARG_VAL(port, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(negotiateLanguage, 1)
	HTTP_ARG_VAL(supported, 0)
	HTTP_ARG_VAL(default, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(negotiateCharset, 1)
	HTTP_ARG_VAL(supported, 0)
	HTTP_ARG_VAL(default, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(matchModified, 1)
	HTTP_ARG_VAL(last_modified, 0)
	HTTP_ARG_VAL(for_range, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(matchEtag, 1)
	HTTP_ARG_VAL(plain_etag, 0)
	HTTP_ARG_VAL(for_range, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(matchRequestHeader, 2)
	HTTP_ARG_VAL(header_name, 0)
	HTTP_ARG_VAL(header_value, 0)
	HTTP_ARG_VAL(case_sensitive, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(chunkedDecode, 1)
	HTTP_ARG_VAL(encoded_string, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(parseMessage, 1)
	HTTP_ARG_VAL(message_string, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(parseHeaders, 1)
	HTTP_ARG_VAL(headers_string, 0)
HTTP_END_ARGS;

zend_class_entry *http_util_object_ce;
zend_function_entry http_util_object_fe[] = {
	HTTP_UTIL_ALIAS(date, http_date)
	HTTP_UTIL_ALIAS(absoluteUri, http_absolute_uri)
	HTTP_UTIL_ALIAS(negotiateLanguage, http_negotiate_language)
	HTTP_UTIL_ALIAS(negotiateCharset, http_negotiate_charset)
	HTTP_UTIL_ALIAS(matchModified, http_match_modified)
	HTTP_UTIL_ALIAS(matchEtag, http_match_etag)
	HTTP_UTIL_ALIAS(matchRequestHeader, http_match_request_header)
	HTTP_UTIL_ALIAS(chunkedDecode, http_chunked_decode)
	HTTP_UTIL_ALIAS(parseMessage, http_parse_message)
	HTTP_UTIL_ALIAS(parseHeaders, http_parse_headers)
	{NULL, NULL, NULL}
};

void _http_util_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS(HttpUtil, http_util_object, NULL, 0);
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

