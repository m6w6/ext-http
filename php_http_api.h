/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_API_H
#define PHP_HTTP_API_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PHP_WIN32
#include <php_config.h>
#endif
#include <php.h>
#include <SAPI.h>

#include <ext/raphf/php_raphf.h>
#include <ext/propro/php_propro.h>
#include <ext/standard/php_string.h>
#include <ext/spl/spl_iterators.h>
#include <ext/date/php_date.h>

#include <zend_interfaces.h>
#include <zend_exceptions.h>


#ifdef PHP_WIN32
# define PHP_HTTP_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define PHP_HTTP_API extern __attribute__ ((visibility("default")))
#else
# define PHP_HTTP_API extern
#endif

#if (defined(HAVE_ICONV) || defined(PHP_HTTP_HAVE_EXT_ICONV)) && (PHP_HTTP_SHARED_DEPS || !defined(COMPILE_DL_ICONV))
#	define PHP_HTTP_HAVE_ICONV
#endif

#if (defined(HAVE_HASH_EXT) || defined(PHP_HTTP_HAVE_EXT_HASH)) && (PHP_HTTP_SHARED_DEPS || !defined(COMPILE_DL_HASH)) && defined(PHP_HTTP_HAVE_PHP_HASH_H)
#	define PHP_HTTP_HAVE_HASH
#endif

#include <stddef.h>

#ifdef PHP_WIN32
#	define CURL_STATICLIB
#	include <winsock2.h>
#else
#	ifdef HAVE_NETDB_H
#		include <netdb.h>
#	endif
#	ifdef HAVE_UNISTD_H
#		include <unistd.h>
#	endif
#endif

#if defined(HAVE_WCHAR_H) && defined(HAVE_WCTYPE_H) && defined(HAVE_ISWALNUM) && (defined(HAVE_MBRTOWC) || defined(HAVE_MBTOWC))
#	define PHP_HTTP_HAVE_WCHAR 1
#endif

#include <ctype.h>
#define PHP_HTTP_IS_CTYPE(type, c) is##type((int) (unsigned char) (c))
#define PHP_HTTP_TO_CTYPE(type, c) to##type((int) (unsigned char) (c))

#include "php_http.h"

#include "php_http_buffer.h"
#include "php_http_misc.h"
#include "php_http_options.h"

#include "php_http.h"
#include "php_http_cookie.h"
#include "php_http_encoding.h"
#include "php_http_info.h"
#include "php_http_message.h"
#include "php_http_env.h"
#include "php_http_env_request.h"
#include "php_http_env_response.h"
#include "php_http_etag.h"
#include "php_http_exception.h"
#include "php_http_filter.h"
#include "php_http_header_parser.h"
#include "php_http_header.h"
#include "php_http_message_body.h"
#include "php_http_message_parser.h"
#include "php_http_negotiate.h"
#include "php_http_object.h"
#include "php_http_params.h"
#include "php_http_querystring.h"
#include "php_http_client.h"
#include "php_http_curl.h"
#include "php_http_client_request.h"
#include "php_http_client_response.h"
#include "php_http_client_curl.h"
#include "php_http_url.h"
#include "php_http_version.h"

ZEND_BEGIN_MODULE_GLOBALS(php_http)
	struct php_http_env_globals env;
#ifdef PHP_HTTP_HAVE_CLIENT
	struct {
#ifdef PHP_HTTP_HAVE_CURL
		struct php_http_client_curl_globals curl;
#endif
	} client;
#endif
ZEND_END_MODULE_GLOBALS(php_http)

ZEND_EXTERN_MODULE_GLOBALS(php_http);

#ifdef ZTS
#	include "TSRM/TSRM.h"
#	define PHP_HTTP_G ((zend_php_http_globals *) (*((void ***) tsrm_get_ls_cache()))[TSRM_UNSHUFFLE_RSRC_ID(php_http_globals_id)])
#	undef TSRMLS_FETCH_FROM_CTX
#	define TSRMLS_FETCH_FROM_CTX(ctx) ERROR
#else
#	define PHP_HTTP_G (&php_http_globals)
#endif

#if PHP_DEBUG
#	define _DPF_STR	0
#	define _DPF_IN	1
#	define _DPF_OUT	2
extern void _dpf(int type, const char *data, size_t length);
#else
#	define _dpf(t,s,l);
#endif

#endif /* PHP_HTTP_API_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
