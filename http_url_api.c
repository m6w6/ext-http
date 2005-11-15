/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#include "SAPI.h"
#include "zend_ini.h"
#include "php_output.h"
#include "ext/standard/url.h"

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_url_api.h"
#include "php_http_std_defs.h"

#include "phpstr/phpstr.h"

#ifdef PHP_WIN32
#	include <winsock2.h>
#elif defined(HAVE_NETDB_H)
#	include <netdb.h>
#endif

ZEND_EXTERN_MODULE_GLOBALS(http);

/* {{{ char *http_absolute_url(char *) */
PHP_HTTP_API char *_http_absolute_url_ex(
	const char *url,	size_t url_len,
	const char *proto,	size_t proto_len,
	const char *host,	size_t host_len,
	unsigned port TSRMLS_DC)
{
#if defined(PHP_WIN32) || defined(HAVE_NETDB_H)
	struct servent *se;
#endif
	php_url *purl = NULL, furl;
	size_t full_len = 0;
	zval *zhost = NULL;
	char *scheme = NULL, *uri, *URL;

	if ((!url || !url_len) && (
			(!(url = SG(request_info).request_uri)) ||
			(!(url_len = strlen(SG(request_info).request_uri))))) {
		http_error(HE_WARNING, HTTP_E_RUNTIME, "Cannot build an absolute URI if supplied URL and REQUEST_URI is empty");
		return NULL;
	}

	URL = ecalloc(1, HTTP_URI_MAXLEN + 1);
	uri = estrndup(url, url_len);
	if (!(purl = php_url_parse(uri))) {
		http_error_ex(HE_WARNING, HTTP_E_URL, "Could not parse supplied URL: %s", url);
		return NULL;
	}

	furl.user		= purl->user;
	furl.pass		= purl->pass;
	furl.path		= purl->path;
	furl.query		= purl->query;
	furl.fragment	= purl->fragment;

	if (proto && proto_len) {
		furl.scheme = scheme = estrdup(proto);
	} else if (purl->scheme) {
		furl.scheme = purl->scheme;
#if defined(PHP_WIN32) || defined(HAVE_NETDB_H)
	} else if (port && (se = getservbyport(port, "tcp"))) {
		furl.scheme = (scheme = estrdup(se->s_name));
#endif
	} else {
		furl.scheme = "http";
	}

	if (port) {
		furl.port = port;
	} else if (purl->port) {
		furl.port = purl->port;
	} else if (strncmp(furl.scheme, "http", 4)) {
#if defined(PHP_WIN32) || defined(HAVE_NETDB_H)
		if ((se = getservbyname(furl.scheme, "tcp"))) {
			furl.port = se->s_port;
		}
#endif
	} else {
		furl.port = (furl.scheme[4] == 's') ? 443 : 80;
	}

	if (host) {
		furl.host = (char *) host;
	} else if (purl->host) {
		furl.host = purl->host;
	} else if (	(zhost = http_get_server_var("HTTP_HOST")) ||
				(zhost = http_get_server_var("SERVER_NAME"))) {
		furl.host = Z_STRVAL_P(zhost);
	} else {
		furl.host = "localhost";
	}

#define HTTP_URI_STRLCATS(URL, full_len, add_string) HTTP_URI_STRLCAT(URL, full_len, add_string, sizeof(add_string)-1)
#define HTTP_URI_STRLCATL(URL, full_len, add_string) HTTP_URI_STRLCAT(URL, full_len, add_string, strlen(add_string))
#define HTTP_URI_STRLCAT(URL, full_len, add_string, add_len) \
	if ((full_len += add_len) > HTTP_URI_MAXLEN) { \
		http_error_ex(HE_NOTICE, HTTP_E_URL, \
			"Absolute URI would have exceeded max URI length (%d bytes) - " \
			"tried to add %d bytes ('%s')", \
			HTTP_URI_MAXLEN, add_len, add_string); \
		if (scheme) { \
			efree(scheme); \
		} \
		php_url_free(purl); \
		efree(uri); \
		return URL; \
	} else { \
		strcat(URL, add_string); \
	}

	HTTP_URI_STRLCATL(URL, full_len, furl.scheme);
	HTTP_URI_STRLCATS(URL, full_len, "://");

	if (furl.user) {
		HTTP_URI_STRLCATL(URL, full_len, furl.user);
		if (furl.pass) {
			HTTP_URI_STRLCATS(URL, full_len, ":");
			HTTP_URI_STRLCATL(URL, full_len, furl.pass);
		}
		HTTP_URI_STRLCATS(URL, full_len, "@");
	}

	HTTP_URI_STRLCATL(URL, full_len, furl.host);

	if (	(!strcmp(furl.scheme, "http") && (furl.port != 80)) ||
			(!strcmp(furl.scheme, "https") && (furl.port != 443))) {
		char port_string[8] = {0};
		snprintf(port_string, 7, ":%u", furl.port);
		HTTP_URI_STRLCATL(URL, full_len, port_string);
	}

	if (furl.path) {
		if (furl.path[0] != '/') {
			HTTP_URI_STRLCATS(URL, full_len, "/");
		}
		HTTP_URI_STRLCATL(URL, full_len, furl.path);
	} else {
		HTTP_URI_STRLCATS(URL, full_len, "/");
	}

	if (furl.query) {
		HTTP_URI_STRLCATS(URL, full_len, "?");
		HTTP_URI_STRLCATL(URL, full_len, furl.query);
	}

	if (furl.fragment) {
		HTTP_URI_STRLCATS(URL, full_len, "#");
		HTTP_URI_STRLCATL(URL, full_len, furl.fragment);
	}

	if (scheme) {
		efree(scheme);
	}
	php_url_free(purl);
	efree(uri);

	return URL;
}
/* }}} */

/* {{{ STATUS http_urlencode_hash_ex(HashTable *, zend_bool, char *, size_t, char **, size_t *) */
PHP_HTTP_API STATUS _http_urlencode_hash_ex(HashTable *hash, zend_bool override_argsep,
	char *pre_encoded_data, size_t pre_encoded_len,
	char **encoded_data, size_t *encoded_len TSRMLS_DC)
{
	char *arg_sep;
	size_t arg_sep_len;
	phpstr *qstr = phpstr_new();

	if (override_argsep || !(arg_sep_len = strlen(arg_sep = INI_STR("arg_separator.output")))) {
		arg_sep = HTTP_URL_ARGSEP;
		arg_sep_len = lenof(HTTP_URL_ARGSEP);
	}

	if (pre_encoded_len && pre_encoded_data) {
		phpstr_append(qstr, pre_encoded_data, pre_encoded_len);
	}

	if (SUCCESS != http_urlencode_hash_recursive(hash, qstr, arg_sep, arg_sep_len, NULL, 0)) {
		phpstr_free(&qstr);
		return FAILURE;
	}

	phpstr_data(qstr, encoded_data, encoded_len);
	phpstr_free(&qstr);

	return SUCCESS;
}
/* }}} */

/* {{{ http_urlencode_hash_recursive */
PHP_HTTP_API STATUS _http_urlencode_hash_recursive(HashTable *ht, phpstr *str, const char *arg_sep, size_t arg_sep_len, const char *prefix, size_t prefix_len TSRMLS_DC)
{
	char *key = NULL;
	uint len = 0;
	ulong idx = 0;
	zval **data = NULL;
	HashPosition pos;

	if (!ht || !str) {
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid parameters");
		return FAILURE;
	}
	if (ht->nApplyCount > 0) {
		return SUCCESS;
	}
	
	FOREACH_HASH_KEYLENVAL(pos, ht, key, len, idx, data) {
		char *encoded_key;
		int encoded_len;
		phpstr new_prefix;
		
		if (!data || !*data) {
			return FAILURE;
		}
		
		if (key) {
			if (len && key[len - 1] == '\0') {
				--len;
			}
			encoded_key = php_url_encode(key, len, &encoded_len);
			key = NULL;
		} else {
			encoded_len = spprintf(&encoded_key, 0, "%ld", idx);
		}
		
		{
			phpstr_init(&new_prefix);
			if (prefix && prefix_len) {
				phpstr_append(&new_prefix, prefix, prefix_len);
				phpstr_appends(&new_prefix, "[");
			}
			
			phpstr_append(&new_prefix, encoded_key, encoded_len);
			efree(encoded_key);
			
			if (prefix && prefix_len) {
				phpstr_appends(&new_prefix, "]");
			}
			phpstr_fix(&new_prefix);
		}
		
		if (Z_TYPE_PP(data) == IS_ARRAY) {
			STATUS status;
			++ht->nApplyCount;
			status = http_urlencode_hash_recursive(Z_ARRVAL_PP(data), str, arg_sep, arg_sep_len, PHPSTR_VAL(&new_prefix), PHPSTR_LEN(&new_prefix));
			--ht->nApplyCount;
			if (SUCCESS != status) {
				phpstr_dtor(&new_prefix);
				return FAILURE;
			}
		} else {
			char *encoded_val;
			int encoded_len;
			zval *cpy, *val = convert_to_type_ex(IS_STRING, *data, &cpy);
			
			if (PHPSTR_LEN(str)) {
				phpstr_append(str, arg_sep, arg_sep_len);
			}
			phpstr_append(str, PHPSTR_VAL(&new_prefix), PHPSTR_LEN(&new_prefix));
			phpstr_appends(str, "=");
			
			encoded_val = php_url_encode(Z_STRVAL_P(val), Z_STRLEN_P(val), &encoded_len);
			phpstr_append(str, encoded_val, encoded_len);
			efree(encoded_val);
			
			if (cpy) {
				zval_ptr_dtor(&cpy);
			}
		}
		
		phpstr_dtor(&new_prefix);
	}
	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

