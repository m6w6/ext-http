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

#include "php_http_api.h"

#include "php_ini.h"
#include "ext/standard/info.h"

#include <zlib.h>

#if PHP_HTTP_HAVE_LIBCURL
#	include <curl/curl.h>
#	if PHP_HTTP_HAVE_LIBEVENT
#		if PHP_HTTP_HAVE_LIBEVENT2
#			include <event2/event.h>
#			include <event2/event_struct.h>
#		else
#			include <event.h>
#		endif
#	endif
#endif
#if PHP_HTTP_HAVE_LIBICU
#	include <unicode/uversion.h>
#endif
#if PHP_HTTP_HAVE_LIBIDN
#	include <idna.h>
#endif
#if PHP_HTTP_HAVE_LIBIDN2
#	include <idn2.h>
#endif
#if PHP_HTTP_HAVE_LIBIDNKIT2 || PHP_HTTP_HAVE_LIBIDNKIT
#include "idn/version.h"
#endif

ZEND_DECLARE_MODULE_GLOBALS(php_http);

#if COMPILE_DL_HTTP
ZEND_GET_MODULE(http)
#endif

zend_function_entry http_functions[] = {
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http);
PHP_MSHUTDOWN_FUNCTION(http);
PHP_RSHUTDOWN_FUNCTION(http);
PHP_MINFO_FUNCTION(http);

static zend_module_dep http_module_deps[] = {
	ZEND_MOD_REQUIRED("raphf")
	ZEND_MOD_REQUIRED("propro")
	ZEND_MOD_REQUIRED("spl")
#if PHP_HTTP_HAVE_HASH
	ZEND_MOD_REQUIRED("hash")
#endif
#if PHP_HTTP_HAVE_ICONV
	ZEND_MOD_REQUIRED("iconv")
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
	NULL,
	PHP_RSHUTDOWN(http),
	PHP_MINFO(http),
	PHP_PECL_HTTP_VERSION,
	STANDARD_MODULE_PROPERTIES
};

int http_module_number;

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
static inline void php_http_globals_init(zend_php_http_globals *G)
{
}

static inline void php_http_globals_free(zend_php_http_globals *G)
{
}
#endif

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("http.etag.mode", "crc32b", PHP_INI_ALL, OnUpdateString, env.etag_mode, zend_php_http_globals, php_http_globals)
PHP_INI_END()

PHP_MINIT_FUNCTION(http)
{
	http_module_number = module_number;
	ZEND_INIT_MODULE_GLOBALS(php_http, php_http_globals_init_once, NULL);
	REGISTER_INI_ENTRIES();
	
	if (0
	|| SUCCESS != PHP_MINIT_CALL(http_object)
	|| SUCCESS != PHP_MINIT_CALL(http_exception)
	|| SUCCESS != PHP_MINIT_CALL(http_cookie)
	|| SUCCESS != PHP_MINIT_CALL(http_encoding)
	|| SUCCESS != PHP_MINIT_CALL(http_encoding_zlib)
#if PHP_HTTP_HAVE_LIBBROTLI
	|| SUCCESS != PHP_MINIT_CALL(http_encoding_brotli)
#endif
	|| SUCCESS != PHP_MINIT_CALL(http_filter)
	|| SUCCESS != PHP_MINIT_CALL(http_header)
	|| SUCCESS != PHP_MINIT_CALL(http_header_parser)
	|| SUCCESS != PHP_MINIT_CALL(http_message)
	|| SUCCESS != PHP_MINIT_CALL(http_message_parser)
	|| SUCCESS != PHP_MINIT_CALL(http_message_body)
	|| SUCCESS != PHP_MINIT_CALL(http_querystring)
	|| SUCCESS != PHP_MINIT_CALL(http_client)
	|| SUCCESS != PHP_MINIT_CALL(http_client_request)
	|| SUCCESS != PHP_MINIT_CALL(http_client_response)
#if PHP_HTTP_HAVE_LIBCURL
	|| SUCCESS != PHP_MINIT_CALL(http_curl)
	|| SUCCESS != PHP_MINIT_CALL(http_client_curl)
	|| SUCCESS != PHP_MINIT_CALL(http_client_curl_user)
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
#if PHP_HTTP_HAVE_LIBCURL
	|| SUCCESS != PHP_MSHUTDOWN_CALL(http_client_curl)
	|| SUCCESS != PHP_MSHUTDOWN_CALL(http_curl)
#endif
	|| SUCCESS != PHP_MSHUTDOWN_CALL(http_client)
	) {
		return FAILURE;
	}
	
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http)
{
	if (0
	|| SUCCESS != PHP_RSHUTDOWN_CALL(http_env)
	) {
		return FAILURE;
	}
	
	return SUCCESS;
}

PHP_MINFO_FUNCTION(http)
{
	php_http_buffer_t buf;

	php_http_buffer_init(&buf);

	php_info_print_table_start();
	php_info_print_table_header(2, "HTTP Support", "enabled");
	php_info_print_table_row(2, "Extension Version", PHP_PECL_HTTP_VERSION);
	php_info_print_table_end();
	
	php_info_print_table_start();
	php_info_print_table_header(3, "Used Library", "Compiled", "Linked");
	php_info_print_table_row(3, "libz", ZLIB_VERSION, zlibVersion());
#if PHP_HTTP_HAVE_LIBCURL
	{
		curl_version_info_data *cv = curl_version_info(CURLVERSION_NOW);
		php_info_print_table_row(3, "libcurl", LIBCURL_VERSION, cv->version);
	}
#else
	php_info_print_table_row(3, "libcurl", "disabled", "disabled");
#endif

#if PHP_HTTP_HAVE_LIBEVENT
	php_info_print_table_row(3, "libevent",
#	ifdef LIBEVENT_VERSION
			LIBEVENT_VERSION,
#	else
			PHP_HTTP_LIBEVENT_VERSION,
#	endif
			event_get_version());
#else
	php_info_print_table_row(3, "libevent", "disabled", "disabled");
#endif

#if PHP_HTTP_HAVE_LIBICU
	{
		UVersionInfo uv = {0};
		char us[U_MAX_VERSION_STRING_LENGTH] = {0};

		u_getVersion(uv);
		u_versionToString(uv, us);
		php_info_print_table_row(3, "libicu "
#if HAVE_UIDNA_NAMETOASCII_UTF8 && HAVE_UIDNA_IDNTOASCII
				"(IDNA2008/IDNA2003)"
#elif HAVE_UIDNA_NAMETOASCII_UTF8
				"(IDNA2008)"
#elif HAVE_UIDNA_IDNTOASCII
				"(IDNA2003)"
#endif
				, U_ICU_VERSION, us);
	}
#else
	php_info_print_table_row(3, "libicu (IDNA2008/IDNA2003)", "disabled", "disabled");
#endif
#if PHP_HTTP_HAVE_LIBIDN2
	php_info_print_table_row(3, "libidn2 (IDNA2008)", IDN2_VERSION, idn2_check_version(NULL));
#else
	php_info_print_table_row(3, "libidn2 (IDNA2008)", "disabled", "disabled");
#endif
#if PHP_HTTP_HAVE_LIBIDN
	php_info_print_table_row(3, "libidn (IDNA2003)", PHP_HTTP_LIBIDN_VERSION, "unknown");
#else
	php_info_print_table_row(3, "libidn (IDNA2003)", "disabled", "disabled");
#endif
#if PHP_HTTP_HAVE_LIBIDNKIT2
	php_info_print_table_row(3, "libidnkit2 (IDNA2008)",  IDNKIT_VERSION_LIBIDN, idn_version_libidn());
#else
	php_info_print_table_row(3, "libidnkit2 (IDNA2008)", "disabled", "disabled");
#endif
#if PHP_HTTP_HAVE_LIBIDNKIT
	php_info_print_table_row(3, "libidnkit (IDNA2003)",  IDNKIT_VERSION, idn_version_getstring());
#else
	php_info_print_table_row(3, "libidnkit (IDNA2003)", "disabled", "disabled");
#endif
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

