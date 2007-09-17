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

#define HTTP_WANT_SAPI
#define HTTP_WANT_NETDB
#include "php_http.h"

#include "zend_ini.h"
#include "php_output.h"
#include "ext/standard/php_string.h"

#include "php_http_api.h"
#include "php_http_querystring_api.h"
#include "php_http_url_api.h"

static inline char *localhostname(void)
{
	char hostname[1024] = {0};
	
#ifdef PHP_WIN32
	if (SUCCESS == gethostname(hostname, lenof(hostname))) {
		return estrdup(hostname);
	}
#elif defined(HAVE_GETHOSTNAME)
	if (SUCCESS == gethostname(hostname, lenof(hostname))) {
#	if defined(HAVE_GETDOMAINNAME)
		size_t hlen = strlen(hostname);
		if (hlen <= lenof(hostname) - lenof("(none)")) {
			hostname[hlen++] = '.';
			if (SUCCESS == getdomainname(&hostname[hlen], lenof(hostname) - hlen)) {
				if (!strcmp(&hostname[hlen], "(none)")) {
					hostname[hlen - 1] = '\0';
				}
				return estrdup(hostname);
			}
		}
#	endif
		if (strcmp(hostname, "(none)")) {
			return estrdup(hostname);
		}
	}
#endif
	return estrndup("localhost", lenof("localhost"));
}

PHP_MINIT_FUNCTION(http_url)
{
	HTTP_LONG_CONSTANT("HTTP_URL_REPLACE", HTTP_URL_REPLACE);
	HTTP_LONG_CONSTANT("HTTP_URL_JOIN_PATH", HTTP_URL_JOIN_PATH);
	HTTP_LONG_CONSTANT("HTTP_URL_JOIN_QUERY", HTTP_URL_JOIN_QUERY);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_USER", HTTP_URL_STRIP_USER);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_PASS", HTTP_URL_STRIP_PASS);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_AUTH", HTTP_URL_STRIP_AUTH);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_PORT", HTTP_URL_STRIP_PORT);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_PATH", HTTP_URL_STRIP_PATH);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_QUERY", HTTP_URL_STRIP_QUERY);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_FRAGMENT", HTTP_URL_STRIP_FRAGMENT);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_ALL", HTTP_URL_STRIP_ALL);
	HTTP_LONG_CONSTANT("HTTP_URL_FROM_ENV", HTTP_URL_FROM_ENV);
	return SUCCESS;
}

PHP_HTTP_API char *_http_absolute_url_ex(const char *url, int flags TSRMLS_DC)
{
	char *abs = NULL;
	php_url *purl = NULL;
	
	if (url) {
		purl = php_url_parse(abs = estrdup(url));
		STR_SET(abs, NULL);
		if (!purl) {
			http_error_ex(HE_WARNING, HTTP_E_URL, "Could not parse URL (%s)", url);
			return NULL;
		}
	}
	
	http_build_url(flags, purl, NULL, NULL, &abs, NULL);
	
	if (purl) {
		php_url_free(purl);
	}
	
	return abs;
}

/* {{{ void http_build_url(int flags, const php_url *, const php_url *, php_url **, char **, size_t *) */
PHP_HTTP_API void _http_build_url(int flags, const php_url *old_url, const php_url *new_url, php_url **url_ptr, char **url_str, size_t *url_len TSRMLS_DC)
{
#if defined(HAVE_GETSERVBYPORT) || defined(HAVE_GETSERVBYNAME)
	struct servent *se;
#endif
	php_url *url = ecalloc(1, sizeof(php_url));

#define __URLSET(u,n) \
	((u)&&(u)->n)
#define __URLCPY(n) \
	url->n = __URLSET(new_url,n) ? estrdup(new_url->n) : (__URLSET(old_url,n) ? estrdup(old_url->n) : NULL)
	
	if (!(flags & HTTP_URL_STRIP_PORT)) {
		url->port = __URLSET(new_url, port) ? new_url->port : ((old_url) ? old_url->port : 0);
	}
	if (!(flags & HTTP_URL_STRIP_USER)) {
		__URLCPY(user);
	}
	if (!(flags & HTTP_URL_STRIP_PASS)) {
		__URLCPY(pass);
	}
	
	__URLCPY(scheme);
	__URLCPY(host);
	
	if (!(flags & HTTP_URL_STRIP_PATH)) {
		if ((flags & HTTP_URL_JOIN_PATH) && __URLSET(old_url, path) && __URLSET(new_url, path) && *new_url->path != '/') {
			size_t old_path_len = strlen(old_url->path), new_path_len = strlen(new_url->path);
			
			url->path = ecalloc(1, old_path_len + new_path_len + 1 + 1);
			
			strcat(url->path, old_url->path);
			if (url->path[old_path_len - 1] != '/') {
				php_dirname(url->path, old_path_len);
				strcat(url->path, "/");
			}
			strcat(url->path, new_url->path);
		} else {
			__URLCPY(path);
		}
	}
	if (!(flags & HTTP_URL_STRIP_QUERY)) {
		if ((flags & HTTP_URL_JOIN_QUERY) && __URLSET(new_url, query) && __URLSET(old_url, query)) {
			zval qarr, qstr;
			
			INIT_PZVAL(&qstr);
			INIT_PZVAL(&qarr);
			array_init(&qarr);
			
			ZVAL_STRING(&qstr, old_url->query, 0);
			http_querystring_modify(&qarr, &qstr);
			ZVAL_STRING(&qstr, new_url->query, 0);
			http_querystring_modify(&qarr, &qstr);
			
			ZVAL_NULL(&qstr);
			http_querystring_update(&qarr, &qstr);
			url->query = Z_STRVAL(qstr);
			zval_dtor(&qarr);
		} else {
			__URLCPY(query);
		}
	}
	if (!(flags & HTTP_URL_STRIP_FRAGMENT)) {
		__URLCPY(fragment);
	}
	
	if (!url->scheme) {
		if (flags & HTTP_URL_FROM_ENV) {
			zval *https = http_get_server_var("HTTPS", 1);
			if (https && !strcasecmp(Z_STRVAL_P(https), "ON")) {
				url->scheme = estrndup("https", lenof("https"));
			} else switch (url->port) {
				case 443:
					url->scheme = estrndup("https", lenof("https"));
					break;

#ifndef HAVE_GETSERVBYPORT
				default:
#endif
				case 80:
					url->scheme = estrndup("http", lenof("http"));
					break;
			
#ifdef HAVE_GETSERVBYPORT
				default:
					if ((se = getservbyport(htons(url->port), "tcp")) && se->s_name) {
						url->scheme = estrdup(se->s_name);
					} else {
						url->scheme = estrndup("http", lenof("http"));
					}
					break;
#endif
			}
		} else {
			url->scheme = estrndup("http", lenof("http"));
		}
	}

	if (!url->host) {
		if (flags & HTTP_URL_FROM_ENV) {
			zval *zhost;
			
			if ((((zhost = http_get_server_var("HTTP_HOST", 1)) || 
					(zhost = http_get_server_var("SERVER_NAME", 1)))) && Z_STRLEN_P(zhost)) {
				url->host = estrndup(Z_STRVAL_P(zhost), Z_STRLEN_P(zhost));
			} else {
				url->host = localhostname();
			}
		} else {
			url->host = estrndup("localhost", lenof("localhost"));
		}
	}
	
	if (!url->path) {
		if ((flags & HTTP_URL_FROM_ENV) && SG(request_info).request_uri && SG(request_info).request_uri[0]) {
			const char *q = strchr(SG(request_info).request_uri, '?');
			
			if (q) {
				url->path = estrndup(SG(request_info).request_uri, q - SG(request_info).request_uri);
			} else {
				url->path = estrdup(SG(request_info).request_uri);
			}
		} else {
			url->path = estrndup("/", 1);
		}
	} else if (url->path[0] != '/') {
		if ((flags & HTTP_URL_FROM_ENV) && SG(request_info).request_uri && SG(request_info).request_uri[0]) {
			size_t ulen = strlen(SG(request_info).request_uri);
			size_t plen = strlen(url->path);
			char *path;
			
			if (SG(request_info).request_uri[ulen-1] != '/') {
				for (--ulen; ulen && SG(request_info).request_uri[ulen - 1] != '/'; --ulen);
			}
			
			path = emalloc(ulen + plen + 1);
			memcpy(path, SG(request_info).request_uri, ulen);
			memcpy(path + ulen, url->path, plen);
			path[ulen + plen] = '\0';
			STR_SET(url->path, path);
		} else {
			size_t plen = strlen(url->path);
			char *path = emalloc(plen + 1 + 1);
			
			path[0] = '/';
			memcpy(&path[1], url->path, plen + 1);
			STR_SET(url->path, path);
		}
	}
	/* replace directory references if path is not a single slash */
	if (url->path[0] && (url->path[0] != '/' || url->path[1])) {
		char *ptr, *end = url->path + strlen(url->path) + 1;
			
		for (ptr = strstr(url->path, "/."); ptr; ptr = strstr(ptr, "/.")) {
			switch (ptr[2]) {
				case '\0':
					ptr[1] = '\0';
					break;
				
				case '/':
					memmove(&ptr[1], &ptr[3], end - &ptr[3]);
					break;
					
				case '.':
					if (ptr[3] == '/') {
						char *pos = &ptr[4];
						while (ptr != url->path) {
							if (*--ptr == '/') {
								break;
							}
						}
						memmove(&ptr[1], pos, end - pos);
						break;
					} else if (!ptr[3]) {
						/* .. at the end */
						ptr[1] = '\0';
					}
					/* fallthrough */
				
				default:
					/* something else */
					++ptr;
					break;
			}
		}
	}
	
	if (url->port) {
		if (	((url->port == 80) && !strcmp(url->scheme, "http"))
			||	((url->port ==443) && !strcmp(url->scheme, "https"))
#ifdef HAVE_GETSERVBYNAME
			||	((se = getservbyname(url->scheme, "tcp")) && se->s_port && 
					(url->port == ntohs(se->s_port)))
#endif
		) {
			url->port = 0;
		}
	}
	
	if (url_str) {
		size_t len;
		
		*url_str = emalloc(HTTP_URL_MAXLEN + 1);
		
		**url_str = '\0';
		strlcat(*url_str, url->scheme, HTTP_URL_MAXLEN);
		strlcat(*url_str, "://", HTTP_URL_MAXLEN);
		
		if (url->user && *url->user) {
			strlcat(*url_str, url->user, HTTP_URL_MAXLEN);
			if (url->pass && *url->pass) {
				strlcat(*url_str, ":", HTTP_URL_MAXLEN);
				strlcat(*url_str, url->pass, HTTP_URL_MAXLEN);
			}
			strlcat(*url_str, "@", HTTP_URL_MAXLEN);
		}
		
		strlcat(*url_str, url->host, HTTP_URL_MAXLEN);
		
		if (url->port) {
			char port_str[8];
			
			snprintf(port_str, sizeof(port_str), "%d", (int) url->port);
			strlcat(*url_str, ":", HTTP_URL_MAXLEN);
			strlcat(*url_str, port_str, HTTP_URL_MAXLEN);
		}
		
		strlcat(*url_str, url->path, HTTP_URL_MAXLEN);
		
		if (url->query && *url->query) {
			strlcat(*url_str, "?", HTTP_URL_MAXLEN);
			strlcat(*url_str, url->query, HTTP_URL_MAXLEN);
		}
		
		if (url->fragment && *url->fragment) {
			strlcat(*url_str, "#", HTTP_URL_MAXLEN);
			strlcat(*url_str, url->fragment, HTTP_URL_MAXLEN);
		}
		
		if (HTTP_URL_MAXLEN == (len = strlen(*url_str))) {
			http_error(HE_NOTICE, HTTP_E_URL, "Length of URL exceeds HTTP_URL_MAXLEN");
		}
		if (url_len) {
			*url_len = len;
		}
	}
	
	if (url_ptr) {
		*url_ptr = url;
	} else {
		php_url_free(url);
	}
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
	HashKey key = initHashKey(0);
	zval **data = NULL;
	HashPosition pos;

	if (!ht || !str) {
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid parameters");
		return FAILURE;
	}
	if (ht->nApplyCount > 0) {
		return SUCCESS;
	}
	
	FOREACH_HASH_KEYVAL(pos, ht, key, data) {
		char *encoded_key;
		int encoded_len;
		phpstr new_prefix;
		
		if (!data || !*data) {
			phpstr_dtor(str);
			return FAILURE;
		}
		
		if (key.type == HASH_KEY_IS_STRING) {
			if (!*key.str) {
				/* only public properties */
				continue;
			}
			if (key.len && key.str[key.len - 1] == '\0') {
				--key.len;
			}
			encoded_key = php_url_encode(key.str, key.len, &encoded_len);
		} else {
			encoded_len = spprintf(&encoded_key, 0, "%ld", key.num);
		}
		
		{
			phpstr_init(&new_prefix);
			if (prefix && prefix_len) {
				phpstr_append(&new_prefix, prefix, prefix_len);
				phpstr_appends(&new_prefix, "%5B");
			}
			
			phpstr_append(&new_prefix, encoded_key, encoded_len);
			efree(encoded_key);
			
			if (prefix && prefix_len) {
				phpstr_appends(&new_prefix, "%5D");
			}
			phpstr_fix(&new_prefix);
		}
		
		if (Z_TYPE_PP(data) == IS_ARRAY || Z_TYPE_PP(data) == IS_OBJECT) {
			STATUS status;
			++ht->nApplyCount;
			status = http_urlencode_hash_recursive(HASH_OF(*data), str, arg_sep, arg_sep_len, PHPSTR_VAL(&new_prefix), PHPSTR_LEN(&new_prefix));
			--ht->nApplyCount;
			if (SUCCESS != status) {
				phpstr_dtor(&new_prefix);
				phpstr_dtor(str);
				return FAILURE;
			}
		} else {
			zval *val = zval_copy(IS_STRING, *data);
			
			if (PHPSTR_LEN(str)) {
				phpstr_append(str, arg_sep, arg_sep_len);
			}
			phpstr_append(str, PHPSTR_VAL(&new_prefix), PHPSTR_LEN(&new_prefix));
			phpstr_appends(str, "=");
			
			if (Z_STRLEN_P(val) && Z_STRVAL_P(val)) {
				char *encoded_val;
				int encoded_len;
				
				encoded_val = php_url_encode(Z_STRVAL_P(val), Z_STRLEN_P(val), &encoded_len);
				phpstr_append(str, encoded_val, encoded_len);
				efree(encoded_val);
			}
			
			zval_free(&val);
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

