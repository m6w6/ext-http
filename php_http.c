/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

#include <php_ini.h>
#include <ext/standard/info.h>

#include <zlib.h>

#if PHP_HTTP_HAVE_CURL
#	include <curl/curl.h>
#	if PHP_HTTP_HAVE_EVENT
#		include <event.h>
#	endif
#endif
#if PHP_HTTP_HAVE_SERF
#	include <serf.h>
#endif

ZEND_DECLARE_MODULE_GLOBALS(php_http);

#ifdef COMPILE_DL_HTTP
ZEND_GET_MODULE(http)
#endif

zend_function_entry http_functions[] = {
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http);
PHP_MSHUTDOWN_FUNCTION(http);
PHP_RINIT_FUNCTION(http);
PHP_RSHUTDOWN_FUNCTION(http);
PHP_MINFO_FUNCTION(http);

static zend_module_dep http_module_deps[] = {
	ZEND_MOD_REQUIRED("raphf")
	ZEND_MOD_REQUIRED("spl")
#ifdef PHP_HTTP_HAVE_HASH
	ZEND_MOD_REQUIRED("hash")
#endif
#ifdef PHP_HTTP_HAVE_ICONV
	ZEND_MOD_REQUIRED("iconv")
#endif
#ifdef PHP_HTTP_HAVE_JSON
	ZEND_MOD_REQUIRED("json")
#endif
#ifdef PHP_HTTP_HAVE_EVENT
	ZEND_MOD_CONFLICTS("event")
#endif
	{NULL, NULL, NULL, 0}
};

zend_module_entry http_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	http_module_deps,
	"http",
	http_functions,
	PHP_MINIT(http),
	PHP_MSHUTDOWN(http),
	PHP_RINIT(http),
	PHP_RSHUTDOWN(http),
	PHP_MINFO(http),
	PHP_HTTP_EXT_VERSION,
	STANDARD_MODULE_PROPERTIES
};

int http_module_number;

static HashTable http_module_classes;
void php_http_register_class(zend_class_entry *(*get_ce)(void))
{
	zend_hash_next_index_insert(&http_module_classes, &get_ce, sizeof(get_ce), NULL);
}
static void php_http_registered_classes(php_http_buffer_t *buf, unsigned flags)
{
	HashPosition pos;
	zend_class_entry *(**get_ce)(void);

	FOREACH_HASH_VAL(pos, &http_module_classes, get_ce) {
		zend_class_entry *ce = (*get_ce)();
		if ((flags && (ce->ce_flags & flags)) || (!flags && !(ce->ce_flags & 0x0fff))) {
			if (buf->used) {
				php_http_buffer_appends(buf, ", ");
			}
			php_http_buffer_append(buf, ce->name, ce->name_length);
		}
	}
	php_http_buffer_fix(buf);
}

#if PHP_DEBUG && !HAVE_GCOV
void _dpf(int type, const char *data, size_t length)
{
	static const char _sym[] = "><><><";
	if (type) {
		int nwp = 0;
		for (fprintf(stderr, "%c ", _sym[type-1]); length--; data++) {
			int ip = PHP_HTTP_IS_CTYPE(print, *data);
			if (!ip && *data != '\r' && *data != '\n') nwp = 1;
			fprintf(stderr, ip?"%c":"\\x%02x", (int) (*data & 0xff));
			if (!nwp && *data == '\n' && length) {
				fprintf(stderr, "\n%c ", _sym[type-1]);
			}
		}
		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, "# %.*s\n", (int) length, data);
	}
}
#endif

static void php_http_globals_init_once(zend_php_http_globals *G)
{
	memset(G, 0, sizeof(*G));
}

#if 0
static inline void php_http_globals_init(zend_php_http_globals *G TSRMLS_DC)
{
}

static inline void php_http_globals_free(zend_php_http_globals *G TSRMLS_DC)
{
}
#endif

#if ZTS && PHP_DEBUG && !HAVE_GCOV
zend_php_http_globals *php_http_globals(void)
{
	TSRMLS_FETCH();
	return PHP_HTTP_G;
}
#endif

PHP_INI_BEGIN()
	PHP_HTTP_INI_ENTRY("http.etag.mode", "crc32b", PHP_INI_ALL, OnUpdateString, env.etag_mode)
PHP_INI_END()

PHP_MINIT_FUNCTION(http)
{
	http_module_number = module_number;
	ZEND_INIT_MODULE_GLOBALS(php_http, php_http_globals_init_once, NULL);
	REGISTER_INI_ENTRIES();
	
	zend_hash_init(&http_module_classes, 0, NULL, NULL, 1);

	if (0
	|| SUCCESS != PHP_MINIT_CALL(http_object)
	|| SUCCESS != PHP_MINIT_CALL(http_exception)
	|| SUCCESS != PHP_MINIT_CALL(http_cookie)
	|| SUCCESS != PHP_MINIT_CALL(http_encoding)
	|| SUCCESS != PHP_MINIT_CALL(http_filter)
	|| SUCCESS != PHP_MINIT_CALL(http_header)
	|| SUCCESS != PHP_MINIT_CALL(http_message)
	|| SUCCESS != PHP_MINIT_CALL(http_message_body)
	|| SUCCESS != PHP_MINIT_CALL(http_querystring)
	|| SUCCESS != PHP_MINIT_CALL(http_client_interface)
	|| SUCCESS != PHP_MINIT_CALL(http_client)
	|| SUCCESS != PHP_MINIT_CALL(http_client_request)
	|| SUCCESS != PHP_MINIT_CALL(http_client_response)
	|| SUCCESS != PHP_MINIT_CALL(http_client_datashare)
	|| SUCCESS != PHP_MINIT_CALL(http_client_pool)
	|| SUCCESS != PHP_MINIT_CALL(http_client_factory)
#if PHP_HTTP_HAVE_CURL
	|| SUCCESS != PHP_MINIT_CALL(http_curl)
	|| SUCCESS != PHP_MINIT_CALL(http_curl_client)
	|| SUCCESS != PHP_MINIT_CALL(http_curl_client_pool)
	|| SUCCESS != PHP_MINIT_CALL(http_curl_client_datashare)
#endif
	|| SUCCESS != PHP_MINIT_CALL(http_url)
	|| SUCCESS != PHP_MINIT_CALL(http_env)
	|| SUCCESS != PHP_MINIT_CALL(http_env_request)
	|| SUCCESS != PHP_MINIT_CALL(http_env_response)
	|| SUCCESS != PHP_MINIT_CALL(http_params)
	) {
		return FAILURE;
	}
	
	return SUCCESS;
}



PHP_MSHUTDOWN_FUNCTION(http)
{
	UNREGISTER_INI_ENTRIES();
	
	if (0
	|| SUCCESS != PHP_MSHUTDOWN_CALL(http_message)
#if PHP_HTTP_HAVE_CURL
	|| SUCCESS != PHP_MSHUTDOWN_CALL(http_curl_client)
	|| SUCCESS != PHP_MSHUTDOWN_CALL(http_curl)
#endif
	|| SUCCESS != PHP_MSHUTDOWN_CALL(http_client_factory)
	) {
		return FAILURE;
	}
	
	zend_hash_destroy(&http_module_classes);

	return SUCCESS;
}

PHP_RINIT_FUNCTION(http)
{
	if (0
	|| SUCCESS != PHP_RINIT_CALL(http_env)
#if PHP_HTTP_HAVE_CURL && PHP_HTTP_HAVE_EVENT
	|| SUCCESS != PHP_RINIT_CALL(http_curl_client_pool)
#endif
	) {
		return FAILURE;
	}
	
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http)
{
	if (0
#if PHP_HTTP_HAVE_CURL && PHP_HTTP_HAVE_EVENT
	|| SUCCESS != PHP_RSHUTDOWN_CALL(http_curl_client_pool)
#endif
	|| SUCCESS != PHP_RSHUTDOWN_CALL(http_env)
	) {
		return FAILURE;
	}
	
	return SUCCESS;
}

PHP_MINFO_FUNCTION(http)
{
	unsigned i;
	php_http_buffer_t buf;

	php_http_buffer_init(&buf);

	php_info_print_table_start();
	php_info_print_table_header(2, "HTTP Support", "enabled");
	php_info_print_table_row(2, "Extension Version", PHP_HTTP_EXT_VERSION);
	php_info_print_table_end();
	
	php_info_print_table_start();
	php_info_print_table_header(3, "Used Library", "Compiled", "Linked");
	php_info_print_table_row(3, "libz", ZLIB_VERSION, zlibVersion());
#if PHP_HTTP_HAVE_CURL
	{
		curl_version_info_data *cv = curl_version_info(CURLVERSION_NOW);
		php_info_print_table_row(3, "libcurl", LIBCURL_VERSION, cv->version);
	}
#else
	php_info_print_table_row(3, "libcurl", "disabled", "disabled");
#endif

#if PHP_HTTP_HAVE_EVENT
	php_info_print_table_row(3, "libevent",
#	ifdef LIBEVENT_VERSION
			LIBEVENT_VERSION,
#	else
			PHP_HTTP_EVENT_VERSION,
#	endif
			event_get_version());
#else
	php_info_print_table_row(3, "libevent", "disabled", "disabled");
#endif

#if PHP_HTTP_HAVE_SERF
	{
		int v[3];

		serf_lib_version(&v[0], &v[1], &v[2]);
		php_http_buffer_appendf(&buf, "%d.%d.%d", v[0], v[1], v[2]);
		php_http_buffer_fix(&buf);
		php_info_print_table_row(3, "libserf", SERF_VERSION_STRING, buf.data);
		php_http_buffer_reset(&buf);
	}
#else
	php_info_print_table_row(3, "libserf", "disabled", "disabled");
#endif
	php_info_print_table_end();
	
	php_info_print_table_start();
	php_info_print_table_colspan_header(2, "Registered API");
	for (i = 0; http_functions[i].fname; ++i) {
		if (buf.used) {
			php_http_buffer_appends(&buf, ", ");
		}
		php_http_buffer_appendl(&buf, http_functions[i].fname);
	}
	php_http_buffer_fix(&buf);
	php_info_print_table_row(2, "Functions", buf.data);
	php_http_buffer_reset(&buf);
	php_http_registered_classes(&buf, ZEND_ACC_INTERFACE);
	php_info_print_table_row(2, "Interfaces", buf.data);
	php_http_buffer_reset(&buf);
	php_http_registered_classes(&buf, ZEND_ACC_EXPLICIT_ABSTRACT_CLASS);
	php_info_print_table_row(2, "Abstract Classes", buf.data);
	php_http_buffer_reset(&buf);
	php_http_registered_classes(&buf, 0);
	php_info_print_table_row(2, "Implemented Classes", buf.data);
	php_http_buffer_reset(&buf);
	php_http_registered_classes(&buf, ZEND_ACC_FINAL_CLASS);
	php_info_print_table_row(2, "Final Classes", buf.data);
	php_http_buffer_dtor(&buf);

	php_info_print_table_row(2, "Stream Filters",  "http.chunked_encode, http.chunked_decode, http.inflate, http.deflate");
	php_info_print_table_end();


	DISPLAY_INI_ENTRIES();
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

