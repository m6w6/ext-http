/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

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

static inline unsigned port(const char *scheme)
{
	unsigned port = 80;

#if defined(ZTS) && defined(HAVE_GETSERVBYPORT_R)
	int rc;
	size_t len = 0xff;
	char *buf = NULL;
	struct servent *se_res = NULL, se_buf = {0};

	do {
		buf = erealloc(buf, len);
		rc = getservbyname_r(scheme, "tcp", &se_buf, buf, len, &se_res);
		len *= 2;
	} while (rc == ERANGE && len <= 0xfff);

	if (!rc) {
		port = ntohs(se_res->s_port);
	}

	efree(buf);
#elif !defined(ZTS) && defined(HAVE_GETSERVBYPORT)
	struct servent *se;

	if ((se = getservbyname(scheme, "tcp")) && se->s_port) {
		port = ntohs(se->s_port);
	}
#endif

	return port;
}
static inline char *scheme(unsigned port)
{
	char *scheme;
#if defined(ZTS) && defined(HAVE_GETSERVBYPORT_R)
	int rc;
	size_t len = 0xff;
	char *buf = NULL;
	struct servent *se_res = NULL, se_buf = {0};
#elif !defined(ZTS) && defined(HAVE_GETSERVBYPORT)
	struct servent *se;
#endif

	switch (port) {
	case 443:
		scheme = estrndup("https", lenof("https"));
		break;

#if defined(ZTS) && !defined(HAVE_GETSERVBYPORT_R)
	default:
#elif !defined(ZTS) && !defined(HAVE_GETSERVBYPORT)
	default:
#endif
	case 80:
	case 0:
		scheme = estrndup("http", lenof("http"));
		break;

#if defined(ZTS) && defined(HAVE_GETSERVBYPORT_R)
	default:
		do {
			buf = erealloc(buf, len);
			rc = getservbyport_r(htons(port), "tcp", &se_buf, buf, len, &se_res);
			len *= 2;
		} while (rc == ERANGE && len <= 0xfff);

		if (!rc && se_res) {
			scheme = estrdup(se_res->s_name);
		} else {
			scheme = estrndup("http", lenof("http"));
		}

		efree(buf);
		break;

#elif !defined(ZTS) && defined(HAVE_GETSERVBYPORT)
	default:
		if ((se = getservbyport(htons(port), "tcp")) && se->s_name) {
			scheme = estrdup(se->s_name);
		} else {
			scheme = estrndup("http", lenof("http"));
		}
		break;
#endif
	}
	return scheme;
}

static php_url *php_http_url_from_env(php_url *url TSRMLS_DC)
{
	zval *https, *zhost, *zport;
	long port;

	if (!url) {
		url = ecalloc(1, sizeof(*url));
	}

	/* port */
	zport = php_http_env_get_server_var(ZEND_STRL("SERVER_PORT"), 1 TSRMLS_CC);
	if (zport && IS_LONG == is_numeric_string(Z_STRVAL_P(zport), Z_STRLEN_P(zport), &port, NULL, 0)) {
		url->port = port;
	}

	/* scheme */
	https = php_http_env_get_server_var(ZEND_STRL("HTTPS"), 1 TSRMLS_CC);
	if (https && !strcasecmp(Z_STRVAL_P(https), "ON")) {
		url->scheme = estrndup("https", lenof("https"));
	} else {
		url->scheme = scheme(url->port);
	}

	/* host */
	if ((((zhost = php_http_env_get_server_var(ZEND_STRL("HTTP_HOST"), 1 TSRMLS_CC)) ||
			(zhost = php_http_env_get_server_var(ZEND_STRL("SERVER_NAME"), 1 TSRMLS_CC)) ||
			(zhost = php_http_env_get_server_var(ZEND_STRL("SERVER_ADDR"), 1 TSRMLS_CC)))) && Z_STRLEN_P(zhost)) {
		size_t stop_at = strspn(Z_STRVAL_P(zhost), "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-.");

		url->host = estrndup(Z_STRVAL_P(zhost), stop_at);
	} else {
		url->host = localhostname();
	}

	/* path */
	if (SG(request_info).request_uri && SG(request_info).request_uri[0]) {
		const char *q = strchr(SG(request_info).request_uri, '?');

		if (q) {
			url->path = estrndup(SG(request_info).request_uri, q - SG(request_info).request_uri);
		} else {
			url->path = estrdup(SG(request_info).request_uri);
		}
	}

	/* query */
	if (SG(request_info).query_string && SG(request_info).query_string[0]) {
		url->query = estrdup(SG(request_info).query_string);
	}

	return url;
}

void php_http_url(int flags, const php_url *old_url, const php_url *new_url, php_url **url_ptr, char **url_str, size_t *url_len TSRMLS_DC)
{
	php_url *url, *tmp_url = NULL;

	/* set from env if requested */
	if (flags & PHP_HTTP_URL_FROM_ENV) {
		php_url *env_url = php_http_url_from_env(NULL TSRMLS_CC);

		php_http_url(flags ^ PHP_HTTP_URL_FROM_ENV, env_url, old_url, &tmp_url, NULL, NULL TSRMLS_CC);

		php_url_free(env_url);
		old_url = tmp_url;
	}

	url = ecalloc(1, sizeof(*url));

#define __URLSET(u,n) \
	((u)&&(u)->n)
#define __URLCPY(n) \
	url->n = __URLSET(new_url,n) ? estrdup(new_url->n) : (__URLSET(old_url,n) ? estrdup(old_url->n) : NULL)
	
	if (!(flags & PHP_HTTP_URL_STRIP_PORT)) {
		url->port = __URLSET(new_url, port) ? new_url->port : ((old_url) ? old_url->port : 0);
	}
	if (!(flags & PHP_HTTP_URL_STRIP_USER)) {
		__URLCPY(user);
	}
	if (!(flags & PHP_HTTP_URL_STRIP_PASS)) {
		__URLCPY(pass);
	}
	
	__URLCPY(scheme);
	__URLCPY(host);
	
	if (!(flags & PHP_HTTP_URL_STRIP_PATH)) {
		if ((flags & PHP_HTTP_URL_JOIN_PATH) && __URLSET(old_url, path) && __URLSET(new_url, path) && *new_url->path != '/') {
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
	if (!(flags & PHP_HTTP_URL_STRIP_QUERY)) {
		if ((flags & PHP_HTTP_URL_JOIN_QUERY) && __URLSET(new_url, query) && __URLSET(old_url, query)) {
			zval qarr, qstr;
			
			INIT_PZVAL(&qstr);
			INIT_PZVAL(&qarr);
			array_init(&qarr);
			
			ZVAL_STRING(&qstr, old_url->query, 0);
			php_http_querystring_update(&qarr, &qstr, NULL TSRMLS_CC);
			ZVAL_STRING(&qstr, new_url->query, 0);
			php_http_querystring_update(&qarr, &qstr, NULL TSRMLS_CC);
			
			ZVAL_NULL(&qstr);
			php_http_querystring_update(&qarr, NULL, &qstr TSRMLS_CC);
			url->query = Z_STRVAL(qstr);
			zval_dtor(&qarr);
		} else {
			__URLCPY(query);
		}
	}
	if (!(flags & PHP_HTTP_URL_STRIP_FRAGMENT)) {
		__URLCPY(fragment);
	}
	
	/* done with copy & combine & strip */

	if (flags & PHP_HTTP_URL_FROM_ENV) {
		/* free old_url we tainted above */
		php_url_free(tmp_url);
	}

	/* set some sane defaults */

	if (!url->scheme) {
		url->scheme = estrndup("http", lenof("http"));
	}

	if (!url->host) {
		url->host = estrndup("localhost", lenof("localhost"));
	}
	
	if (!url->path) {
		url->path = estrndup("/", 1);
	} else if (url->path[0] != '/') {
		size_t plen = strlen(url->path);
		char *path = emalloc(plen + 1 + 1);

		path[0] = '/';
		memcpy(&path[1], url->path, plen + 1);
		STR_SET(url->path, path);
	}
	/* replace directory references if path is not a single slash */
	if ((flags & PHP_HTTP_URL_SANITIZE_PATH)
	&&	url->path[0] && (url->path[0] != '/' || url->path[1])) {
		char *ptr, *end = url->path + strlen(url->path) + 1;
			
		for (ptr = strchr(url->path, '/'); ptr; ptr = strchr(ptr, '/')) {
			switch (ptr[1]) {
				case '/':
					memmove(&ptr[1], &ptr[2], end - &ptr[2]);
					break;
					
				case '.':
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
							/* no break */

						default:
							/* something else */
							++ptr;
							break;
					}
					break;

				default:
					++ptr;
					break;
			}
		}
	}
	/* unset default ports */
	if (url->port) {
		if (	((url->port == 80) && !strcmp(url->scheme, "http"))
			||	((url->port ==443) && !strcmp(url->scheme, "https"))
			||	( url->port == port(url->scheme))
		) {
			url->port = 0;
		}
	}
	
	if (url_str) {
		php_http_url_to_string(url, url_str, url_len TSRMLS_CC);
	}
	
	if (url_ptr) {
		*url_ptr = url;
	} else {
		php_url_free(url);
	}
}

STATUS php_http_url_encode_hash(HashTable *hash, const char *pre_encoded_str, size_t pre_encoded_len, char **encoded_str, size_t *encoded_len TSRMLS_DC)
{
	const char *arg_sep_str;
	size_t arg_sep_len;
	php_http_buffer_t *qstr = php_http_buffer_new();

	php_http_url_argsep(&arg_sep_str, &arg_sep_len TSRMLS_CC);

	if (SUCCESS != php_http_url_encode_hash_ex(hash, qstr, arg_sep_str, arg_sep_len, "=", 1, pre_encoded_str, pre_encoded_len TSRMLS_CC)) {
		php_http_buffer_free(&qstr);
		return FAILURE;
	}

	php_http_buffer_data(qstr, encoded_str, encoded_len);
	php_http_buffer_free(&qstr);

	return SUCCESS;
}

STATUS php_http_url_encode_hash_ex(HashTable *hash, php_http_buffer_t *qstr, const char *arg_sep_str, size_t arg_sep_len, const char *val_sep_str, size_t val_sep_len, const char *pre_encoded_str, size_t pre_encoded_len TSRMLS_DC)
{
	if (pre_encoded_len && pre_encoded_str) {
		php_http_buffer_append(qstr, pre_encoded_str, pre_encoded_len);
	}

	if (!php_http_params_to_string(qstr, hash, arg_sep_str, arg_sep_len, "", 0, val_sep_str, val_sep_len, PHP_HTTP_PARAMS_QUERY TSRMLS_CC)) {
		return FAILURE;
	}

	return SUCCESS;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, old_url)
	ZEND_ARG_INFO(0, new_url)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, __construct)
{
	zval *new_url = NULL, *old_url = NULL;
	long flags = PHP_HTTP_URL_FROM_ENV;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!z!l", &old_url, &new_url, &flags), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_bad_url_class_entry, &zeh TSRMLS_CC);
	{
		php_url *res_purl, *new_purl = NULL, *old_purl = NULL;

		if (new_url) {
			switch (Z_TYPE_P(new_url)) {
				case IS_OBJECT:
				case IS_ARRAY:
					new_purl = php_http_url_from_struct(NULL, HASH_OF(new_url) TSRMLS_CC);
					break;
				default: {
					zval *cpy = php_http_ztyp(IS_STRING, new_url);

					new_purl = php_url_parse(Z_STRVAL_P(cpy));
					zval_ptr_dtor(&cpy);
					break;
				}
			}
			if (!new_purl) {
				zend_restore_error_handling(&zeh TSRMLS_CC);
				return;
			}
		}
		if (old_url) {
			switch (Z_TYPE_P(old_url)) {
				case IS_OBJECT:
				case IS_ARRAY:
					old_purl = php_http_url_from_struct(NULL, HASH_OF(old_url) TSRMLS_CC);
					break;
				default: {
					zval *cpy = php_http_ztyp(IS_STRING, old_url);

					old_purl = php_url_parse(Z_STRVAL_P(cpy));
					zval_ptr_dtor(&cpy);
					break;
				}
			}
			if (!old_purl) {
				if (new_purl) {
					php_url_free(new_purl);
				}
				zend_restore_error_handling(&zeh TSRMLS_CC);
				return;
			}
		}

		php_http_url(flags, old_purl, new_purl, &res_purl, NULL, NULL TSRMLS_CC);
		php_http_url_to_struct(res_purl, getThis() TSRMLS_CC);

		php_url_free(res_purl);
		if (old_purl) {
			php_url_free(old_purl);
		}
		if (new_purl) {
			php_url_free(new_purl);
		}
	}
	zend_restore_error_handling(&zeh TSRMLS_CC);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl_mod, 0, 0, 1)
	ZEND_ARG_INFO(0, more_url_parts)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, mod)
{
	zval *new_url = NULL;
	long flags = PHP_HTTP_URL_JOIN_PATH | PHP_HTTP_URL_JOIN_QUERY;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z!|l", &new_url, &flags), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_bad_url_class_entry, &zeh TSRMLS_CC);
	{
		php_url *new_purl = NULL, *old_purl = NULL;

		if (new_url) {
			switch (Z_TYPE_P(new_url)) {
				case IS_OBJECT:
				case IS_ARRAY:
					new_purl = php_http_url_from_struct(NULL, HASH_OF(new_url) TSRMLS_CC);
					break;
				default: {
					zval *cpy = php_http_ztyp(IS_STRING, new_url);

					new_purl = php_url_parse(Z_STRVAL_P(new_url));
					zval_ptr_dtor(&cpy);
					break;
				}
			}
			if (!new_purl) {
				zend_restore_error_handling(&zeh TSRMLS_CC);
				return;
			}
		}

		if ((old_purl = php_http_url_from_struct(NULL, HASH_OF(getThis()) TSRMLS_CC))) {
			php_url *res_purl;

			ZVAL_OBJVAL(return_value, zend_objects_clone_obj(getThis() TSRMLS_CC), 0);

			php_http_url(flags, old_purl, new_purl, &res_purl, NULL, NULL TSRMLS_CC);
			php_http_url_to_struct(res_purl, return_value TSRMLS_CC);

			php_url_free(res_purl);
			php_url_free(old_purl);
		}
		if (new_purl) {
			php_url_free(new_purl);
		}
	}
	zend_restore_error_handling(&zeh TSRMLS_CC);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl_toString, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, toString)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_url *purl;

		if ((purl = php_http_url_from_struct(NULL, HASH_OF(getThis()) TSRMLS_CC))) {
			char *str;
			size_t len;

			php_http_url(0, purl, NULL, NULL, &str, &len TSRMLS_CC);
			php_url_free(purl);
			RETURN_STRINGL(str, len, 0);
		}
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl_toArray, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, toArray)
{
	php_url *purl;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	/* strip any non-URL properties */
	purl = php_http_url_from_struct(NULL, HASH_OF(getThis()) TSRMLS_CC);
	php_http_url_to_struct(purl, return_value TSRMLS_CC);
	php_url_free(purl);
}

static zend_function_entry php_http_url_methods[] = {
	PHP_ME(HttpUrl, __construct,  ai_HttpUrl___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpUrl, mod,          ai_HttpUrl_mod, ZEND_ACC_PUBLIC)
	PHP_ME(HttpUrl, toString,     ai_HttpUrl_toString, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpUrl, __toString, toString, ai_HttpUrl_toString, ZEND_ACC_PUBLIC)
	PHP_ME(HttpUrl, toArray,      ai_HttpUrl_toArray, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

zend_class_entry *php_http_url_class_entry;

PHP_MINIT_FUNCTION(http_url)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Url", php_http_url_methods);
	php_http_url_class_entry = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("scheme"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("user"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("pass"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("host"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("port"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("path"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("query"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("fragment"), ZEND_ACC_PUBLIC TSRMLS_CC);

	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("REPLACE"), PHP_HTTP_URL_REPLACE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("JOIN_PATH"), PHP_HTTP_URL_JOIN_PATH TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("JOIN_QUERY"), PHP_HTTP_URL_JOIN_QUERY TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_USER"), PHP_HTTP_URL_STRIP_USER TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_PASS"), PHP_HTTP_URL_STRIP_PASS TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_AUTH"), PHP_HTTP_URL_STRIP_AUTH TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_PORT"), PHP_HTTP_URL_STRIP_PORT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_PATH"), PHP_HTTP_URL_STRIP_PATH TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_QUERY"), PHP_HTTP_URL_STRIP_QUERY TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_FRAGMENT"), PHP_HTTP_URL_STRIP_FRAGMENT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_ALL"), PHP_HTTP_URL_STRIP_ALL TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("FROM_ENV"), PHP_HTTP_URL_FROM_ENV TSRMLS_CC);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("SANITIZE_PATH"), PHP_HTTP_URL_SANITIZE_PATH TSRMLS_CC);

	return SUCCESS;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

