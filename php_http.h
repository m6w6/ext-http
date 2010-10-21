/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id: php_http.h 300300 2010-06-09 07:29:35Z mike $ */

#ifndef PHP_EXT_HTTP_H
#define PHP_EXT_HTTP_H

#define PHP_HTTP_EXT_VERSION "2.0.0dev"

#ifdef HAVE_CONFIG_H
#	include "config.h"
#else
#	ifndef PHP_WIN32
#		include "php_config.h"
#	endif
#endif

#include "php.h"
#if defined(PHP_WIN32)
#	if defined(PHP_HTTP_EXPORTS)
#		define PHP_HTTP_API __declspec(dllexport)
#	elif defined(COMPILE_DL_HTTP)
#		define PHP_HTTP_API __declspec(dllimport)
#	else
#		define PHP_HTTP_API
#	endif
#else
#	define PHP_HTTP_API
#endif

/* make functions that return SUCCESS|FAILURE more obvious */
typedef int STATUS;

#include "php_http_buffer.h"
#include "php_http_strlist.h"

#if (defined(HAVE_ICONV) || defined(PHP_HTTP_HAVE_EXT_ICONV)) && (PHP_HTTP_SHARED_DEPS || !defined(COMPILE_DL_ICONV))
#	define PHP_HTTP_HAVE_ICONV
#endif

#if (defined(HAVE_HASH_EXT) || defined(PHP_HTTP_HAVE_EXT_HASH)) && (PHP_HTTP_SHARED_DEPS || !defined(COMPILE_DL_HASH)) && defined(PHP_HTTP_HAVE_PHP_HASH_H)
#	define PHP_HTTP_HAVE_HASH
#endif

#ifdef PHP_WIN32
#	define CURL_STATICLIB
#	define PHP_HTTP_HAVE_NETDB
#	include <winsock2.h>
#elif defined(HAVE_NETDB_H)
#	define PHP_HTTP_HAVE_NETDB
#	include <netdb.h>
#	ifdef HAVE_UNISTD_H
#		include <unistd.h>
#	endif
#endif

#include <ctype.h>
#define PHP_HTTP_IS_CTYPE(type, c) is##type((int) (unsigned char) (c))
#define PHP_HTTP_TO_CTYPE(type, c) to##type((int) (unsigned char) (c))

extern zend_module_entry http_module_entry;
#define phpext_http_ptr &http_module_entry

extern int http_module_number;

#if PHP_DEBUG
#	define _DPF_STR	0
#	define _DPF_IN	1
#	define _DPF_OUT	2
extern void _dpf(int type, const char *data, size_t length);
#else
#	define _dpf(t,s,l);
#endif

#include "php_http_misc.h"

#include "php_http_cookie.h"
#include "php_http_encoding.h"
#include "php_http_env.h"
#include "php_http_etag.h"
#include "php_http_exception.h"
#include "php_http_fluently_callable.h"
#include "php_http_filter.h"
#include "php_http_headers.h"
#include "php_http_info.h"
#include "php_http_header_parser.h"
#include "php_http_message_body.h"
#include "php_http_message.h"
#include "php_http_message_parser.h"
#include "php_http_negotiate.h"
#include "php_http_object.h"
#include "php_http_params.h"
#include "php_http_persistent_handle.h"
#include "php_http_property_proxy.h"
#include "php_http_querystring.h"
#include "php_http_request_datashare.h"
#include "php_http_request_factory.h"
#include "php_http_request.h"
#include "php_http_curl.h"
#include "php_http_neon.h"
#include "php_http_request_method.h"
#include "php_http_request_pool.h"
#include "php_http_url.h"
#include "php_http_version.h"

ZEND_BEGIN_MODULE_GLOBALS(php_http)
	struct php_http_env_globals env;
	struct php_http_persistent_handle_globals persistent_handle;
	struct php_http_request_datashare_globals request_datashare;
	struct php_http_request_pool_globals request_pool;
ZEND_END_MODULE_GLOBALS(php_http)

ZEND_EXTERN_MODULE_GLOBALS(php_http);

#ifdef ZTS
#	include "TSRM/TSRM.h"
#	define PHP_HTTP_G ((zend_http_globals *) (*((void ***) tsrm_ls))[TSRM_UNSHUFFLE_RSRC_ID(php_http_globals_id)])
#	undef TSRMLS_FETCH_FROM_CTX
#	define TSRMLS_FETCH_FROM_CTX(ctx) ((ctx)?(ctx):ts_resource_ex(0, NULL))
#else
#	define PHP_HTTP_G (&php_http_globals)
#endif


#endif	/* PHP_EXT_HTTP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

