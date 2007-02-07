/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#include "php_http.h"

#ifdef ZEND_ENGINE_2

#include "ext/standard/php_http.h"

#include "php_http_util_object.h"

#define HTTP_BEGIN_ARGS(method, req_args)	HTTP_BEGIN_ARGS_EX(HttpUtil, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpUtil, method, 0)

#define HTTP_UTIL_ALIAS(method, func)		HTTP_STATIC_ME_ALIAS(method, func, HTTP_ARGS(HttpUtil, method))

HTTP_BEGIN_ARGS(date, 0)
	HTTP_ARG_VAL(timestamp, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(buildStr, 1)
	HTTP_ARG_VAL(query, 0)
	HTTP_ARG_VAL(prefix, 0)
	HTTP_ARG_VAL(arg_sep, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(buildUrl, 1)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(parts, 0)
	HTTP_ARG_VAL(flags, 0)
	HTTP_ARG_VAL(composed, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(negotiateLanguage, 1)
	HTTP_ARG_VAL(supported, 0)
	HTTP_ARG_VAL(result, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(negotiateCharset, 1)
	HTTP_ARG_VAL(supported, 0)
	HTTP_ARG_VAL(result, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(negotiateContentType, 1)
	HTTP_ARG_VAL(supported, 0)
	HTTP_ARG_VAL(result, 1)
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

HTTP_BEGIN_ARGS(parseMessage, 1)
	HTTP_ARG_VAL(message_string, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(parseHeaders, 1)
	HTTP_ARG_VAL(headers_string, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(parseCookie, 1)
	HTTP_ARG_VAL(cookie_string, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(buildCookie, 1)
	HTTP_ARG_VAL(cookie_array, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(parseParams, 1)
	HTTP_ARG_VAL(param_string, 0)
	HTTP_ARG_VAL(flags, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(chunkedDecode, 1)
	HTTP_ARG_VAL(encoded_string, 0)
HTTP_END_ARGS;

#ifdef HTTP_HAVE_ZLIB
HTTP_BEGIN_ARGS(deflate, 1)
	HTTP_ARG_VAL(plain, 0)
	HTTP_ARG_VAL(flags, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(inflate, 1)
	HTTP_ARG_VAL(encoded, 0)
HTTP_END_ARGS;
#endif

HTTP_BEGIN_ARGS(support, 0)
	HTTP_ARG_VAL(feature, 0)
HTTP_END_ARGS;

zend_class_entry *http_util_object_ce;
zend_function_entry http_util_object_fe[] = {
	HTTP_UTIL_ALIAS(date, http_date)
	HTTP_UTIL_ALIAS(buildUrl, http_build_url)
	HTTP_UTIL_ALIAS(buildStr, http_build_str)
	HTTP_UTIL_ALIAS(negotiateLanguage, http_negotiate_language)
	HTTP_UTIL_ALIAS(negotiateCharset, http_negotiate_charset)
	HTTP_UTIL_ALIAS(negotiateContentType, http_negotiate_content_type)
	HTTP_UTIL_ALIAS(matchModified, http_match_modified)
	HTTP_UTIL_ALIAS(matchEtag, http_match_etag)
	HTTP_UTIL_ALIAS(matchRequestHeader, http_match_request_header)
	HTTP_UTIL_ALIAS(parseMessage, http_parse_message)
	HTTP_UTIL_ALIAS(parseHeaders, http_parse_headers)
	HTTP_UTIL_ALIAS(parseCookie, http_parse_cookie)
	HTTP_UTIL_ALIAS(buildCookie, http_build_cookie)
	HTTP_UTIL_ALIAS(parseParams, http_parse_params)
	HTTP_UTIL_ALIAS(chunkedDecode, http_chunked_decode)
#ifdef HTTP_HAVE_ZLIB
	HTTP_UTIL_ALIAS(deflate, http_deflate)
	HTTP_UTIL_ALIAS(inflate, http_inflate)
#endif /* HTTP_HAVE_ZLIB */
	HTTP_UTIL_ALIAS(support, http_support)
	
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_util_object)
{
	HTTP_REGISTER_CLASS(HttpUtil, http_util_object, NULL, 0);
	return SUCCESS;
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

