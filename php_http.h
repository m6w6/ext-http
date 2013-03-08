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

/* $Id$ */

#ifndef PHP_EXT_HTTP_H
#define PHP_EXT_HTTP_H

#define PHP_HTTP_VERSION "1.7.6dev"

#ifdef HAVE_CONFIG_H
#	include "config.h"
#else
#	ifndef PHP_WIN32
#		include "php_config.h"
#	endif
#endif

#include "php.h"
#include "missing.h"
#include "php_http_std_defs.h"
#include "phpstr/phpstr.h"

#ifdef HTTP_WANT_SAPI
#	if PHP_API_VERSION > 20041225
#		define HTTP_HAVE_SAPI_RTIME
#	endif
#	include "SAPI.h"
#endif

#ifdef HTTP_WANT_NETDB
#	ifdef PHP_WIN32
#		define HTTP_HAVE_NETDB
#		include <winsock2.h>
#	elif defined(HAVE_NETDB_H)
#		define HTTP_HAVE_NETDB
#		include <netdb.h>
#		ifdef HAVE_UNISTD_H
#			include <unistd.h>
#		endif
#	endif
#endif

#if defined(HTTP_WANT_CURL) && defined(HTTP_HAVE_CURL)
#	ifdef PHP_WIN32
#		include <winsock2.h>
#		define CURL_STATICLIB
#	endif
#	include <curl/curl.h>
#	define HTTP_CURL_VERSION(x, y, z) (LIBCURL_VERSION_NUM >= (((x)<<16) + ((y)<<8) + (z)))
#
#	if defined(HTTP_WANT_EVENT) && defined(HTTP_HAVE_EVENT)
#		include <event.h>
#	endif
#endif

#if defined(HTTP_WANT_MAGIC) && defined(HTTP_HAVE_MAGIC)
#	if defined(PHP_WIN32) && !defined(USE_MAGIC_DLL) && !defined(USE_MAGIC_STATIC)
#		define USE_MAGIC_STATIC
#	endif
#	include <magic.h>
#endif

#if defined(HTTP_WANT_ZLIB) && defined(HTTP_HAVE_ZLIB)
#	include <zlib.h>
#endif

#include <ctype.h>
#define HTTP_IS_CTYPE(type, c) is##type((int) (unsigned char) (c))
#define HTTP_TO_CTYPE(type, c) to##type((int) (unsigned char) (c))

extern zend_module_entry http_module_entry;
#define phpext_http_ptr &http_module_entry

extern int http_module_number;

ZEND_BEGIN_MODULE_GLOBALS(http)

	struct _http_globals_etag {
		char *mode;
		void *ctx;
		zend_bool started;
	} etag;

	struct _http_globals_log {
		char *cache;
		char *redirect;
		char *not_found;
		char *allowed_methods;
		char *composite;
	} log;

	struct _http_globals_send {
		double throttle_delay;
		size_t buffer_size;
		char *content_type;
		char *unquoted_etag;
		time_t last_modified;
		struct _http_globals_send_deflate {
			zend_bool response;
			zend_bool start_auto;
			long start_flags;
			int encoding;
			void *stream;
		} deflate;
		struct _http_globals_send_inflate {
			zend_bool start_auto;
			long start_flags;
			void *stream;
		} inflate;
		zend_bool not_found_404;
	} send;

	struct _http_globals_request {
		time_t time;
		HashTable *headers;
		struct _http_globals_request_methods {
			HashTable registered;
			char *allowed;
			char *custom;
		} methods;
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)
		struct _http_globals_request_datashare {
			zend_llist handles;
			zend_bool cookie;
			zend_bool dns;
			zend_bool ssl;
			zend_bool connect;
		} datashare;
#endif
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_EVENT)
		struct _http_globals_request_pool {
			struct _http_globals_request_pool_event {
				void *base;
			} event;
		} pool;
#endif
	} request;

	struct _http_globals_persistent {
		struct _http_globals_persistent_handles {
			ulong limit;
			struct _http_globals_persistent_handles_ident {
				ulong h;
				char *s;
				size_t l;
			} ident;
		} handles;
	} persistent;

#ifdef ZEND_ENGINE_2
	zend_bool only_exceptions;
#endif

	zend_bool force_exit;
	zend_bool read_post_data;
	zval *server_var;

ZEND_END_MODULE_GLOBALS(http)

ZEND_EXTERN_MODULE_GLOBALS(http);

#ifdef ZTS
#	include "TSRM.h"
#	define HTTP_G ((zend_http_globals *) (*((void ***) tsrm_ls))[TSRM_UNSHUFFLE_RSRC_ID(http_globals_id)])
#else
#	define HTTP_G (&http_globals)
#endif

#if defined(HAVE_ICONV) && (HTTP_SHARED_DEPS || !defined(COMPILE_DL_ICONV))
#	define HTTP_HAVE_ICONV
#endif

#if defined(HAVE_PHP_SESSION) && (HTTP_SHARED_DEPS || !defined(COMPILE_DL_SESSION))
#	define HTTP_HAVE_SESSION
#endif

#if defined(HAVE_HASH_EXT) && (HTTP_SHARED_DEPS || !defined(COMPILE_DL_HASH)) && defined(HTTP_HAVE_PHP_HASH_H)
#	define HTTP_HAVE_HASH
#endif

#if defined(HAVE_SPL)
#	define HTTP_HAVE_SPL
#endif

PHP_FUNCTION(http_date);
PHP_FUNCTION(http_build_url);
PHP_FUNCTION(http_build_str);
PHP_FUNCTION(http_negotiate_language);
PHP_FUNCTION(http_negotiate_charset);
PHP_FUNCTION(http_negotiate_content_type);
PHP_FUNCTION(http_negotiate);
PHP_FUNCTION(http_redirect);
PHP_FUNCTION(http_throttle);
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
PHP_FUNCTION(http_parse_message);
PHP_FUNCTION(http_parse_headers);
PHP_FUNCTION(http_parse_cookie);
PHP_FUNCTION(http_build_cookie);
PHP_FUNCTION(http_parse_params);
PHP_FUNCTION(http_get_request_headers);
PHP_FUNCTION(http_get_request_body);
PHP_FUNCTION(http_get_request_body_stream);
PHP_FUNCTION(http_match_request_header);
PHP_FUNCTION(http_persistent_handles_count);
PHP_FUNCTION(http_persistent_handles_clean);
PHP_FUNCTION(http_persistent_handles_ident);
#ifdef HTTP_HAVE_CURL
PHP_FUNCTION(http_get);
PHP_FUNCTION(http_head);
PHP_FUNCTION(http_post_data);
PHP_FUNCTION(http_post_fields);
PHP_FUNCTION(http_put_data);
PHP_FUNCTION(http_put_file);
PHP_FUNCTION(http_put_stream);
PHP_FUNCTION(http_request);
PHP_FUNCTION(http_request_body_encode);
#endif /* HTTP_HAVE_CURL */
PHP_FUNCTION(http_request_method_register);
PHP_FUNCTION(http_request_method_unregister);
PHP_FUNCTION(http_request_method_exists);
PHP_FUNCTION(http_request_method_name);
PHP_FUNCTION(ob_etaghandler);
#ifdef HTTP_HAVE_ZLIB
PHP_FUNCTION(http_deflate);
PHP_FUNCTION(http_inflate);
PHP_FUNCTION(ob_deflatehandler);
PHP_FUNCTION(ob_inflatehandler);
#endif
PHP_FUNCTION(http_support);

#endif	/* PHP_HTTP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

