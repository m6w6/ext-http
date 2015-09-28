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

#if PHP_HTTP_HAVE_IDN2
#	include <idn2.h>
#elif PHP_HTTP_HAVE_IDN
#	include <idna.h>
#endif

#ifdef PHP_HTTP_HAVE_WCHAR
#	include <wchar.h>
#	include <wctype.h>
#endif

#ifdef HAVE_ARPA_INET_H
#	include <arpa/inet.h>
#endif

#include "php_http_utf8.h"

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

#define url(buf) ((php_http_url_t *) (buf).data)

static php_http_url_t *php_http_url_from_env(TSRMLS_D)
{
	zval *https, *zhost, *zport;
	long port;
	php_http_buffer_t buf;

	php_http_buffer_init_ex(&buf, MAX(PHP_HTTP_BUFFER_DEFAULT_SIZE, sizeof(php_http_url_t)<<2), PHP_HTTP_BUFFER_INIT_PREALLOC);
	php_http_buffer_account(&buf, sizeof(php_http_url_t));
	memset(buf.data, 0, buf.used);

	/* scheme */
	url(buf)->scheme = &buf.data[buf.used];
	https = php_http_env_get_server_var(ZEND_STRL("HTTPS"), 1 TSRMLS_CC);
	if (https && !strcasecmp(Z_STRVAL_P(https), "ON")) {
		php_http_buffer_append(&buf, "https", sizeof("https"));
	} else {
		php_http_buffer_append(&buf, "http", sizeof("http"));
	}

	/* host */
	url(buf)->host = &buf.data[buf.used];
	if ((((zhost = php_http_env_get_server_var(ZEND_STRL("HTTP_HOST"), 1 TSRMLS_CC)) ||
			(zhost = php_http_env_get_server_var(ZEND_STRL("SERVER_NAME"), 1 TSRMLS_CC)) ||
			(zhost = php_http_env_get_server_var(ZEND_STRL("SERVER_ADDR"), 1 TSRMLS_CC)))) && Z_STRLEN_P(zhost)) {
		size_t stop_at = strspn(Z_STRVAL_P(zhost), "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-.");

		php_http_buffer_append(&buf, Z_STRVAL_P(zhost), stop_at);
		php_http_buffer_append(&buf, "", 1);
	} else {
		char *host_str = localhostname();

		php_http_buffer_append(&buf, host_str, strlen(host_str) + 1);
		efree(host_str);
	}

	/* port */
	zport = php_http_env_get_server_var(ZEND_STRL("SERVER_PORT"), 1 TSRMLS_CC);
	if (zport && IS_LONG == is_numeric_string(Z_STRVAL_P(zport), Z_STRLEN_P(zport), &port, NULL, 0)) {
		url(buf)->port = port;
	}

	/* path */
	if (SG(request_info).request_uri && SG(request_info).request_uri[0]) {
		const char *q = strchr(SG(request_info).request_uri, '?');

		url(buf)->path = &buf.data[buf.used];

		if (q) {
			php_http_buffer_append(&buf, SG(request_info).request_uri, q - SG(request_info).request_uri);
			php_http_buffer_append(&buf, "", 1);
		} else {
			php_http_buffer_append(&buf, SG(request_info).request_uri, strlen(SG(request_info).request_uri) + 1);
		}
	}

	/* query */
	if (SG(request_info).query_string && SG(request_info).query_string[0]) {
		url(buf)->query = &buf.data[buf.used];
		php_http_buffer_append(&buf, SG(request_info).query_string, strlen(SG(request_info).query_string) + 1);
	}

	return url(buf);
}

#define url_isset(u,n) \
	((u)&&(u)->n)
#define url_append(buf, append) do { \
	char *_ptr = (buf)->data; \
	php_http_url_t *_url = (php_http_url_t *) _ptr, _mem = *_url; \
	append; \
	/* relocate */ \
	if (_ptr != (buf)->data) { \
		ptrdiff_t diff = (buf)->data - _ptr; \
		_url = (php_http_url_t *) (buf)->data; \
		if (_mem.scheme)	_url->scheme += diff; \
		if (_mem.user)		_url->user += diff; \
		if (_mem.pass)		_url->pass += diff; \
		if (_mem.host)		_url->host += diff; \
		if (_mem.path)		_url->path += diff; \
		if (_mem.query)		_url->query += diff; \
		if (_mem.fragment)	_url->fragment += diff; \
	} \
} while (0)
#define url_copy(n) do { \
	if (url_isset(new_url, n)) { \
		url(buf)->n = &buf.data[buf.used]; \
		url_append(&buf, php_http_buffer_append(&buf, new_url->n, strlen(new_url->n) + 1)); \
	} else if (url_isset(old_url, n)) { \
		url(buf)->n = &buf.data[buf.used]; \
		url_append(&buf, php_http_buffer_append(&buf, old_url->n, strlen(old_url->n) + 1)); \
	} \
} while (0)

php_http_url_t *php_http_url_mod(const php_http_url_t *old_url, const php_http_url_t *new_url, unsigned flags TSRMLS_DC)
{
	php_http_url_t *tmp_url = NULL;
	php_http_buffer_t buf;

	php_http_buffer_init_ex(&buf, MAX(PHP_HTTP_BUFFER_DEFAULT_SIZE, sizeof(php_http_url_t)<<2), PHP_HTTP_BUFFER_INIT_PREALLOC);
	php_http_buffer_account(&buf, sizeof(php_http_url_t));
	memset(buf.data, 0, buf.used);

	/* set from env if requested */
	if (flags & PHP_HTTP_URL_FROM_ENV) {
		php_http_url_t *env_url = php_http_url_from_env(TSRMLS_C);

		old_url = tmp_url = php_http_url_mod(env_url, old_url, flags ^ PHP_HTTP_URL_FROM_ENV TSRMLS_CC);
		php_http_url_free(&env_url);
	}

	url_copy(scheme);

	if (!(flags & PHP_HTTP_URL_STRIP_USER)) {
		url_copy(user);
	}

	if (!(flags & PHP_HTTP_URL_STRIP_PASS)) {
		url_copy(pass);
	}
	
	url_copy(host);
	
	if (!(flags & PHP_HTTP_URL_STRIP_PORT)) {
		url(buf)->port = url_isset(new_url, port) ? new_url->port : ((old_url) ? old_url->port : 0);
	}

	if (!(flags & PHP_HTTP_URL_STRIP_PATH)) {
		if ((flags & PHP_HTTP_URL_JOIN_PATH) && url_isset(old_url, path) && url_isset(new_url, path) && *new_url->path != '/') {
			size_t old_path_len = strlen(old_url->path), new_path_len = strlen(new_url->path);
			char *path = ecalloc(1, old_path_len + new_path_len + 1 + 1);
			
			strcat(path, old_url->path);
			if (path[old_path_len - 1] != '/') {
				php_dirname(path, old_path_len);
				strcat(path, "/");
			}
			strcat(path, new_url->path);
			
			url(buf)->path = &buf.data[buf.used];
			if (path[0] != '/') {
				url_append(&buf, php_http_buffer_append(&buf, "/", 1));
			}
			url_append(&buf, php_http_buffer_append(&buf, path, strlen(path) + 1));
			efree(path);
		} else {
			const char *path = NULL;

			if (url_isset(new_url, path)) {
				path = new_url->path;
			} else if (url_isset(old_url, path)) {
				path = old_url->path;
			}

			if (path) {
				url(buf)->path = &buf.data[buf.used];

				url_append(&buf, php_http_buffer_append(&buf, path, strlen(path) + 1));
			}


		}
	}

	if (!(flags & PHP_HTTP_URL_STRIP_QUERY)) {
		if ((flags & PHP_HTTP_URL_JOIN_QUERY) && url_isset(new_url, query) && url_isset(old_url, query)) {
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

			url(buf)->query = &buf.data[buf.used];
			url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL(qstr), Z_STRLEN(qstr) + 1));

			zval_dtor(&qstr);
			zval_dtor(&qarr);
		} else {
			url_copy(query);
		}
	}

	if (!(flags & PHP_HTTP_URL_STRIP_FRAGMENT)) {
		url_copy(fragment);
	}
	
	/* done with copy & combine & strip */

	if (flags & PHP_HTTP_URL_FROM_ENV) {
		/* free old_url we tainted above */
		php_http_url_free(&tmp_url);
	}

	/* replace directory references if path is not a single slash */
	if ((flags & PHP_HTTP_URL_SANITIZE_PATH)
	&&	url(buf)->path[0] && url(buf)->path[1]) {
		char *ptr, *end = url(buf)->path + strlen(url(buf)->path) + 1;
			
		for (ptr = strchr(url(buf)->path, '/'); ptr; ptr = strchr(ptr, '/')) {
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
								while (ptr != url(buf)->path) {
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
	if (url(buf)->port) {
		if (	((url(buf)->port == 80) && url(buf)->scheme && !strcmp(url(buf)->scheme, "http"))
			||	((url(buf)->port ==443) && url(buf)->scheme && !strcmp(url(buf)->scheme, "https"))
		) {
			url(buf)->port = 0;
		}
	}
	
	return url(buf);
}

char *php_http_url_to_string(const php_http_url_t *url, char **url_str, size_t *url_len, zend_bool persistent)
{
	php_http_buffer_t buf;

	php_http_buffer_init_ex(&buf, PHP_HTTP_BUFFER_DEFAULT_SIZE, persistent ?
			PHP_HTTP_BUFFER_INIT_PERSISTENT : 0);

	if (url->scheme && *url->scheme) {
		php_http_buffer_appendl(&buf, url->scheme);
		php_http_buffer_appends(&buf, "://");
	} else if ((url->user && *url->user) || (url->host && *url->host)) {
		php_http_buffer_appends(&buf, "//");
	}

	if (url->user && *url->user) {
		php_http_buffer_appendl(&buf, url->user);
		if (url->pass && *url->pass) {
			php_http_buffer_appends(&buf, ":");
			php_http_buffer_appendl(&buf, url->pass);
		}
		php_http_buffer_appends(&buf, "@");
	}

	if (url->host && *url->host) {
		php_http_buffer_appendl(&buf, url->host);
		if (url->port) {
			php_http_buffer_appendf(&buf, ":%hu", url->port);
		}
	}

	if (url->path && *url->path) {
		if (*url->path != '/') {
			php_http_buffer_appends(&buf, "/");
		}
		php_http_buffer_appendl(&buf, url->path);
	} else if (buf.used) {
		php_http_buffer_appends(&buf, "/");
	}

	if (url->query && *url->query) {
		php_http_buffer_appends(&buf, "?");
		php_http_buffer_appendl(&buf, url->query);
	}

	if (url->fragment && *url->fragment) {
		php_http_buffer_appends(&buf, "#");
		php_http_buffer_appendl(&buf, url->fragment);
	}

	php_http_buffer_shrink(&buf);
	php_http_buffer_fix(&buf);

	if (url_len) {
		*url_len = buf.used;
	}

	if (url_str) {
		*url_str = buf.data;
	}

	return buf.data;
}

char *php_http_url_authority_to_string(const php_http_url_t *url, char **url_str, size_t *url_len)
{
	php_http_buffer_t buf;

	php_http_buffer_init(&buf);

	if (url->user && *url->user) {
		php_http_buffer_appendl(&buf, url->user);
		if (url->pass && *url->pass) {
			php_http_buffer_appends(&buf, ":");
			php_http_buffer_appendl(&buf, url->pass);
		}
		php_http_buffer_appends(&buf, "@");
	}

	if (url->host && *url->host) {
		php_http_buffer_appendl(&buf, url->host);
		if (url->port) {
			php_http_buffer_appendf(&buf, ":%hu", url->port);
		}
	}

	php_http_buffer_shrink(&buf);
	php_http_buffer_fix(&buf);

	if (url_len) {
		*url_len = buf.used;
	}

	if (url_str) {
		*url_str = buf.data;
	}

	return buf.data;
}

php_http_url_t *php_http_url_from_zval(zval *value, unsigned flags TSRMLS_DC)
{
	zval *zcpy;
	php_http_url_t *purl;

	switch (Z_TYPE_P(value)) {
	case IS_ARRAY:
	case IS_OBJECT:
		purl = php_http_url_from_struct(HASH_OF(value));
		break;

	default:
		zcpy = php_http_ztyp(IS_STRING, value);
		purl = php_http_url_parse(Z_STRVAL_P(zcpy), Z_STRLEN_P(zcpy), flags TSRMLS_CC);
		zval_ptr_dtor(&zcpy);
	}

	return purl;
}

php_http_url_t *php_http_url_from_struct(HashTable *ht)
{
	zval **e;
	php_http_buffer_t buf;

	php_http_buffer_init_ex(&buf, MAX(PHP_HTTP_BUFFER_DEFAULT_SIZE, sizeof(php_http_url_t)<<2), PHP_HTTP_BUFFER_INIT_PREALLOC);
	php_http_buffer_account(&buf, sizeof(php_http_url_t));
	memset(buf.data, 0, buf.used);

	if (SUCCESS == zend_hash_find(ht, "scheme", sizeof("scheme"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url(buf)->scheme = &buf.data[buf.used];
		url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy) + 1));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "user", sizeof("user"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url(buf)->user = &buf.data[buf.used];
		url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy) + 1));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "pass", sizeof("pass"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url(buf)->pass = &buf.data[buf.used];
		url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy) + 1));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "host", sizeof("host"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url(buf)->host = &buf.data[buf.used];
		url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy) + 1));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "port", sizeof("port"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_LONG, *e);
		url(buf)->port = (unsigned short) Z_LVAL_P(cpy);
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "path", sizeof("path"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url(buf)->path = &buf.data[buf.used];
		url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy) + 1));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "query", sizeof("query"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url(buf)->query = &buf.data[buf.used];
		url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy) + 1));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "fragment", sizeof("fragment"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url(buf)->fragment = &buf.data[buf.used];
		url_append(&buf, php_http_buffer_append(&buf, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy) + 1));
		zval_ptr_dtor(&cpy);
	}

	return url(buf);
}

HashTable *php_http_url_to_struct(const php_http_url_t *url, zval *strct TSRMLS_DC)
{
	zval arr;

	if (strct) {
		switch (Z_TYPE_P(strct)) {
			default:
				zval_dtor(strct);
				array_init(strct);
				/* no break */
			case IS_ARRAY:
			case IS_OBJECT:
				INIT_PZVAL_ARRAY((&arr), HASH_OF(strct));
				break;
		}
	} else {
		INIT_PZVAL(&arr);
		array_init(&arr);
	}

	if (url) {
		if (url->scheme) {
			add_assoc_string(&arr, "scheme", url->scheme, 1);
		}
		if (url->user) {
			add_assoc_string(&arr, "user", url->user, 1);
		}
		if (url->pass) {
			add_assoc_string(&arr, "pass", url->pass, 1);
		}
		if (url->host) {
			add_assoc_string(&arr, "host", url->host, 1);
		}
		if (url->port) {
			add_assoc_long(&arr, "port", (long) url->port);
		}
		if (url->path) {
			add_assoc_string(&arr, "path", url->path, 1);
		}
		if (url->query) {
			add_assoc_string(&arr, "query", url->query, 1);
		}
		if (url->fragment) {
			add_assoc_string(&arr, "fragment", url->fragment, 1);
		}
	}

	return Z_ARRVAL(arr);
}

ZEND_RESULT_CODE php_http_url_encode_hash(HashTable *hash, const char *pre_encoded_str, size_t pre_encoded_len, char **encoded_str, size_t *encoded_len TSRMLS_DC)
{
	const char *arg_sep_str = "&";
	size_t arg_sep_len = 1;
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

ZEND_RESULT_CODE php_http_url_encode_hash_ex(HashTable *hash, php_http_buffer_t *qstr, const char *arg_sep_str, size_t arg_sep_len, const char *val_sep_str, size_t val_sep_len, const char *pre_encoded_str, size_t pre_encoded_len TSRMLS_DC)
{
	if (pre_encoded_len && pre_encoded_str) {
		php_http_buffer_append(qstr, pre_encoded_str, pre_encoded_len);
	}

	if (!php_http_params_to_string(qstr, hash, arg_sep_str, arg_sep_len, "", 0, val_sep_str, val_sep_len, PHP_HTTP_PARAMS_QUERY TSRMLS_CC)) {
		return FAILURE;
	}

	return SUCCESS;
}

struct parse_state {
	php_http_url_t url;
#ifdef ZTS
	void ***ts;
#endif
	const char *ptr;
	const char *end;
	size_t maxlen;
	off_t offset;
	unsigned flags;
	char buffer[1]; /* last member */
};

void php_http_url_free(php_http_url_t **url)
{
	if (*url) {
		efree(*url);
		*url = NULL;
	}
}

php_http_url_t *php_http_url_copy(const php_http_url_t *url, zend_bool persistent)
{
	php_http_url_t *cpy;
	const char *end = NULL, *url_ptr = (const char *) url;
	char *cpy_ptr;

	end = MAX(url->scheme, end);
	end = MAX(url->pass, end);
	end = MAX(url->user, end);
	end = MAX(url->host, end);
	end = MAX(url->path, end);
	end = MAX(url->query, end);
	end = MAX(url->fragment, end);

	if (end) {
		end += strlen(end) + 1;
		cpy_ptr = pecalloc(1, end - url_ptr, persistent);
		cpy = (php_http_url_t *) cpy_ptr;

		memcpy(cpy_ptr + sizeof(*cpy), url_ptr + sizeof(*url), end - url_ptr - sizeof(*url));

		cpy->scheme = url->scheme ? cpy_ptr + (url->scheme - url_ptr) : NULL;
		cpy->pass = url->pass ? cpy_ptr + (url->pass - url_ptr) : NULL;
		cpy->user = url->user ? cpy_ptr + (url->user - url_ptr) : NULL;
		cpy->host = url->host ? cpy_ptr + (url->host - url_ptr) : NULL;
		cpy->path = url->path ? cpy_ptr + (url->path - url_ptr) : NULL;
		cpy->query = url->query ? cpy_ptr + (url->query - url_ptr) : NULL;
		cpy->fragment = url->fragment ? cpy_ptr + (url->fragment - url_ptr) : NULL;
	} else {
		cpy = ecalloc(1, sizeof(*url));
	}

	cpy->port = url->port;

	return cpy;
}

static size_t parse_mb_utf8(unsigned *wc, const char *ptr, const char *end)
{
	unsigned wchar;
	size_t consumed = utf8towc(&wchar, (const unsigned char *) ptr, end - ptr);

	if (!consumed || consumed == (size_t) -1) {
		return 0;
	}

	if (wc) {
		*wc = wchar;
	}
	return consumed;
}

#ifdef PHP_HTTP_HAVE_WCHAR
static size_t parse_mb_loc(unsigned *wc, const char *ptr, const char *end)
{
	wchar_t wchar;
	size_t consumed = 0;
#if defined(HAVE_MBRTOWC)
	mbstate_t ps;

	memset(&ps, 0, sizeof(ps));
	consumed = mbrtowc(&wchar, ptr, end - ptr, &ps);
#elif defined(HAVE_MBTOWC)
	consumed = mbtowc(&wchar, ptr, end - ptr);
#endif

	if (!consumed || consumed == (size_t) -1) {
		return 0;
	}

	if (wc) {
		*wc = wchar;
	}
	return consumed;
}
#endif

typedef enum parse_mb_what {
	PARSE_SCHEME,
	PARSE_USERINFO,
	PARSE_HOSTINFO,
	PARSE_PATH,
	PARSE_QUERY,
	PARSE_FRAGMENT
} parse_mb_what_t;

static const char * const parse_what[] = {
	"scheme",
	"userinfo",
	"hostinfo",
	"path",
	"query",
	"fragment"
};

static const char parse_xdigits[] = "0123456789ABCDEF";

static size_t parse_mb(struct parse_state *state, parse_mb_what_t what, const char *ptr, const char *end, const char *begin, zend_bool silent)
{
	unsigned wchar;
	size_t consumed = 0;

	if (state->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
		consumed = parse_mb_utf8(&wchar, ptr, end);
	}
#ifdef PHP_HTTP_HAVE_WCHAR
	else if (state->flags & PHP_HTTP_URL_PARSE_MBLOC) {
		consumed = parse_mb_loc(&wchar, ptr, end);
	}
#endif

	while (consumed) {
		if (!(state->flags & PHP_HTTP_URL_PARSE_TOPCT) || what == PARSE_HOSTINFO || what == PARSE_SCHEME) {
			if (what == PARSE_HOSTINFO && (state->flags & PHP_HTTP_URL_PARSE_TOIDN)) {
				/* idna */
			} else if (state->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
				if (!isualnum(wchar)) {
					break;
				}
#ifdef PHP_HTTP_HAVE_WCHAR
			} else if (state->flags & PHP_HTTP_URL_PARSE_MBLOC) {
				if (!iswalnum(wchar)) {
					break;
				}
#endif
			}
			PHP_HTTP_DUFF(consumed, state->buffer[state->offset++] = *ptr++);
		} else {
			int i = 0;

			PHP_HTTP_DUFF(consumed,
					state->buffer[state->offset++] = '%';
					state->buffer[state->offset++] = parse_xdigits[((unsigned char) ptr[i]) >> 4];
					state->buffer[state->offset++] = parse_xdigits[((unsigned char) ptr[i]) & 0xf];
					++i;
			);
		}

		return consumed;
	}

	if (!silent) {
		TSRMLS_FETCH_FROM_CTX(state->ts);
		if (consumed) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Failed to parse %s; unexpected multibyte sequence 0x%x at pos %u in '%s'",
					parse_what[what], wchar, (unsigned) (ptr - begin), begin);
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Failed to parse %s; unexpected byte 0x%02x at pos %u in '%s'",
					parse_what[what], (unsigned char) *ptr, (unsigned) (ptr - begin), begin);
		}
	}

	return 0;
}

static ZEND_RESULT_CODE parse_userinfo(struct parse_state *state, const char *ptr)
{
	size_t mb;
	const char *password = NULL, *end = state->ptr, *tmp = ptr;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	state->url.user = &state->buffer[state->offset];

	do {
		switch (*ptr) {
		case ':':
			if (password) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse password; duplicate ':' at pos %u in '%s'",
						(unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			password = ptr + 1;
			state->buffer[state->offset++] = 0;
			state->url.pass = &state->buffer[state->offset];
			break;

		case '%':
			if (ptr[1] != '%' && (end - ptr <= 2 || !isxdigit(*(ptr+1)) || !isxdigit(*(ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse userinfo; invalid percent encoding at pos %u in '%s'",
						(unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			state->buffer[state->offset++] = *ptr++;
			state->buffer[state->offset++] = *ptr++;
			state->buffer[state->offset++] = *ptr;
			break;

		case '!': case '$': case '&': case '\'': case '(': case ')': case '*':
		case '+': case ',': case ';': case '=': /* sub-delims */
		case '-': case '.': case '_': case '~': /* unreserved */
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
			/* allowed */
			state->buffer[state->offset++] = *ptr;
			break;

		default:
			if (!(mb = parse_mb(state, PARSE_USERINFO, ptr, end, tmp, 0))) {
				return FAILURE;
			}
			ptr += mb - 1;
		}
	} while(++ptr != end);


	state->buffer[state->offset++] = 0;

	return SUCCESS;
}

#if defined(PHP_WIN32) || defined(HAVE_UIDNA_IDNTOASCII)
typedef size_t (*parse_mb_func)(unsigned *wc, const char *ptr, const char *end);
static ZEND_RESULT_CODE to_utf16(parse_mb_func fn, const char *u8, uint16_t **u16, size_t *len TSRMLS_DC)
{
	size_t offset = 0, u8_len = strlen(u8);

	*u16 = ecalloc(4 * sizeof(uint16_t), u8_len + 1);
	*len = 0;

	while (offset < u8_len) {
		unsigned wc;
		uint16_t buf[2], *ptr = buf;
		size_t consumed = fn(&wc, &u8[offset], &u8[u8_len]);

		if (!consumed) {
			efree(*u16);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse UTF-8 at pos %zu of '%s'", offset, u8);
			return FAILURE;
		} else {
			offset += consumed;
		}

		switch (wctoutf16(buf, wc)) {
		case 2:
			(*u16)[(*len)++] = *ptr++;
			/* no break */
		case 1:
			(*u16)[(*len)++] = *ptr++;
			break;
		case 0:
		default:
			efree(*u16);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to convert UTF-32 'U+%X' to UTF-16", wc);
			return FAILURE;
		}
	}

	return SUCCESS;
}
#endif

#ifndef MAXHOSTNAMELEN
#	define MAXHOSTNAMELEN 256
#endif

#if PHP_HTTP_HAVE_IDN2
static ZEND_RESULT_CODE parse_idn2(struct parse_state *state, size_t prev_len)
{
	char *idn = NULL;
	int rv = -1;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	if (state->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
		rv = idn2_lookup_u8((const unsigned char *) state->url.host, (unsigned char **) &idn, IDN2_NFC_INPUT);
	}
#	ifdef PHP_HTTP_HAVE_WCHAR
	else if (state->flags & PHP_HTTP_URL_PARSE_MBLOC) {
		rv = idn2_lookup_ul(state->url.host, &idn, 0);
	}
#	endif
	if (rv != IDN2_OK) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN; %s", idn2_strerror(rv));
		return FAILURE;
	} else {
		size_t idnlen = strlen(idn);
		memcpy(state->url.host, idn, idnlen + 1);
		free(idn);
		state->offset += idnlen - prev_len;
		return SUCCESS;
	}
}
#elif PHP_HTTP_HAVE_IDN
static ZEND_RESULT_CODE parse_idn(struct parse_state *state, size_t prev_len)
{
	char *idn = NULL;
	int rv = -1;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	if (state->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
		rv = idna_to_ascii_8z(state->url.host, &idn, IDNA_ALLOW_UNASSIGNED|IDNA_USE_STD3_ASCII_RULES);
	}
#	ifdef PHP_HTTP_HAVE_WCHAR
	else if (state->flags & PHP_HTTP_URL_PARSE_MBLOC) {
		rv = idna_to_ascii_lz(state->url.host, &idn, IDNA_ALLOW_UNASSIGNED|IDNA_USE_STD3_ASCII_RULES);
	}
#	endif
	if (rv != IDNA_SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN; %s", idna_strerror(rv));
		return FAILURE;
	} else {
		size_t idnlen = strlen(idn);
		memcpy(state->url.host, idn, idnlen + 1);
		free(idn);
		state->offset += idnlen - prev_len;
		return SUCCESS;
	}
}
#endif

#ifdef HAVE_UIDNA_IDNTOASCII
#	if HAVE_UNICODE_UIDNA_H
#		include <unicode/uidna.h>
#	else
typedef uint16_t UChar;
typedef enum { U_ZERO_ERROR = 0 } UErrorCode;
int32_t uidna_IDNToASCII(const UChar *src, int32_t srcLength, UChar *dest, int32_t destCapacity, int32_t options, void *parseError, UErrorCode *status);
#	endif
static ZEND_RESULT_CODE parse_uidn(struct parse_state *state)
{
	char *host_ptr;
	uint16_t *uhost_str, ahost_str[MAXHOSTNAMELEN], *ahost_ptr;
	size_t uhost_len, ahost_len;
	UErrorCode error = U_ZERO_ERROR;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	if (state->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
		if (SUCCESS != to_utf16(parse_mb_utf8, state->url.host, &uhost_str, &uhost_len TSRMLS_CC)) {
			return FAILURE;
		}
#ifdef PHP_HTTP_HAVE_WCHAR
	} else if (state->flags & PHP_HTTP_URL_PARSE_MBLOC) {
		if (SUCCESS != to_utf16(parse_mb_loc, state->url.host, &uhost_str, &uhost_len TSRMLS_CC)) {
			return FAILURE;
		}
#endif
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN; codepage not specified");
		return FAILURE;
	}

	ahost_len = uidna_IDNToASCII(uhost_str, uhost_len, ahost_str, MAXHOSTNAMELEN, 3, NULL, &error);
	efree(uhost_str);

	if (error != U_ZERO_ERROR) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN; ICU error %d", error);
		return FAILURE;
	}

	host_ptr = state->url.host;
	ahost_ptr = ahost_str;
	PHP_HTTP_DUFF(ahost_len, *host_ptr++ = *ahost_ptr++);

	*host_ptr = '\0';
	state->offset += host_ptr - state->url.host;

	return SUCCESS;
}
#endif

#if 0 && defined(PHP_WIN32)
static ZEND_RESULT_CODE parse_widn(struct parse_state *state)
{
	char *host_ptr;
	uint16_t *uhost_str, ahost_str[MAXHOSTNAMELEN], *ahost_ptr;
	size_t uhost_len;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	if (state->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
		if (SUCCESS != to_utf16(parse_mb_utf8, state->url.host, &uhost_str, &uhost_len)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN");
			return FAILURE;
		}
#ifdef PHP_HTTP_HAVE_WCHAR
	} else if (state->flags & PHP_HTTP_URL_PARSE_MBLOC) {
		if (SUCCESS != to_utf16(parse_mb_loc, state->url.host, &uhost_str, &uhost_len)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN");
			return FAILURE;
		}
#endif
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN");
		return FAILURE;
	}

	if (!IdnToAscii(IDN_ALLOW_UNASSIGNED|IDN_USE_STD3_ASCII_RULES, uhost_str, uhost_len, ahost_str, MAXHOSTNAMELEN)) {
		efree(uhost_str);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN");
		return FAILURE;
	}

	efree(uhost_str);
	host_ptr = state->url.host;
	ahost_ptr = ahost_str;
	PHP_HTTP_DUFF(wcslen(ahost_str), *host_ptr++ = *ahost_ptr++);
	efree(ahost_str);

	*host_ptr = '\0';
	state->offset += host_ptr - state->url.host;

	return SUCCESS;
}
#endif

#ifdef HAVE_INET_PTON
static const char *parse_ip6(struct parse_state *state, const char *ptr)
{
	const char *error = NULL, *end = state->ptr, *tmp = memchr(ptr, ']', end - ptr);
	TSRMLS_FETCH_FROM_CTX(state->ts);

	if (tmp) {
		size_t addrlen = tmp - ptr + 1;
		char buf[16], *addr = estrndup(ptr + 1, addrlen - 2);
		int rv = inet_pton(AF_INET6, addr, buf);

		if (rv == 1) {
			state->buffer[state->offset] = '[';
			state->url.host = &state->buffer[state->offset];
			inet_ntop(AF_INET6, buf, state->url.host + 1, state->maxlen - state->offset);
			state->offset += strlen(state->url.host);
			state->buffer[state->offset++] = ']';
			state->buffer[state->offset++] = 0;
			ptr = tmp + 1;
		} else if (rv == -1) {
			error = strerror(errno);
		} else {
			error = "unexpected '['";
		}
		efree(addr);
	} else {
		error = "expected ']'";
	}

	if (error) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse hostinfo; %s", error);
		return NULL;
	}

	return ptr;
}
#endif

static ZEND_RESULT_CODE parse_hostinfo(struct parse_state *state, const char *ptr)
{
	size_t mb, len;
	const char *end = state->ptr, *tmp = ptr, *port = NULL, *label = NULL;
	TSRMLS_FETCH_FROM_CTX(state->ts);

#ifdef HAVE_INET_PTON
	if (*ptr == '[' && !(ptr = parse_ip6(state, ptr))) {
		return FAILURE;
	}
#endif

	if (ptr != end) do {
		switch (*ptr) {
		case ':':
			if (port) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse port; unexpected ':' at pos %u in '%s'",
						(unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			port = ptr + 1;
			break;

		case '%':
			if (ptr[1] != '%' && (end - ptr <= 2 || !isxdigit(*(ptr+1)) || !isxdigit(*(ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse hostinfo; invalid percent encoding at pos %u in '%s'",
						(unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			state->buffer[state->offset++] = *ptr++;
			state->buffer[state->offset++] = *ptr++;
			state->buffer[state->offset++] = *ptr;
			break;

		case '!': case '$': case '&': case '\'': case '(': case ')': case '*':
		case '+': case ',': case ';': case '=': /* sub-delims */
		case '-': case '.': case '_': case '~': /* unreserved */
			if (port || !label) {
				/* sort of a compromise, just ensure we don't end up
				 * with a dot at the beginning or two consecutive dots
				 */
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse %s; unexpected '%c' at pos %u in '%s'",
						port ? "port" : "host",
						(unsigned char) *ptr, (unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			state->buffer[state->offset++] = *ptr;
			label = NULL;
			break;

		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
			if (port) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse port; unexpected char '%c' at pos %u in '%s'",
						(unsigned char) *ptr, (unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			/* no break */
		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
			/* allowed */
			if (port) {
				state->url.port *= 10;
				state->url.port += *ptr - '0';
			} else {
				label = ptr;
				state->buffer[state->offset++] = *ptr;
			}
			break;

		default:
			if (ptr == end) {
				break;
			} else if (port) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse port; unexpected byte 0x%02x at pos %u in '%s'",
						(unsigned char) *ptr, (unsigned) (ptr - tmp), tmp);
				return FAILURE;
			} else if (!(mb = parse_mb(state, PARSE_HOSTINFO, ptr, end, tmp, 0))) {
				return FAILURE;
			}
			label = ptr;
			ptr += mb - 1;
		}
	} while (++ptr != end);

	if (!state->url.host) {
		len = (port ? port - tmp - 1 : end - tmp);
		state->url.host = &state->buffer[state->offset - len];
		state->buffer[state->offset++] = 0;
	}

	if (state->flags & PHP_HTTP_URL_PARSE_TOIDN) {
#if PHP_HTTP_HAVE_IDN2
		return parse_idn2(state, len);
#elif PHP_HTTP_HAVE_IDN
		return parse_idn(state, len);
#endif
#ifdef HAVE_UIDNA_IDNTOASCII
		return parse_uidn(state);
#endif
#if 0 && defined(PHP_WIN32)
		return parse_widn(state);
#endif
	}

	return SUCCESS;
}

static const char *parse_authority(struct parse_state *state)
{
	const char *tmp = state->ptr, *host = NULL;

	do {
		switch (*state->ptr) {
		case '@':
			/* userinfo delimiter */
			if (host) {
				TSRMLS_FETCH_FROM_CTX(state->ts);
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse userinfo; unexpected '@'");
				return NULL;
			}
			host = state->ptr + 1;
			if (tmp != state->ptr && SUCCESS != parse_userinfo(state, tmp)) {
				return NULL;
			}
			tmp = state->ptr + 1;
			break;

		case '/':
		case '?':
		case '#':
		case '\0':
			EOD:
			/* host delimiter */
			if (tmp != state->ptr && SUCCESS != parse_hostinfo(state, tmp)) {
				return NULL;
			}
			return state->ptr;
		}
	} while (++state->ptr <= state->end);

	--state->ptr;
	goto EOD;
}

static const char *parse_path(struct parse_state *state)
{
	size_t mb;
	const char *tmp;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	/* is there actually a path to parse? */
	if (!*state->ptr) {
		return state->ptr;
	}
	tmp = state->ptr;
	state->url.path = &state->buffer[state->offset];

	do {
		switch (*state->ptr) {
		case '#':
		case '?':
			goto done;

		case '%':
			if (state->ptr[1] != '%' && (state->end - state->ptr <= 2 || !isxdigit(*(state->ptr+1)) || !isxdigit(*(state->ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse path; invalid percent encoding at pos %u in '%s'",
						(unsigned) (state->ptr - tmp), tmp);
				return NULL;
			}
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr;
			break;

		case '/': /* yeah, well */
		case '!': case '$': case '&': case '\'': case '(': case ')': case '*':
		case '+': case ',': case ';': case '=': /* sub-delims */
		case '-': case '.': case '_': case '~': /* unreserved */
		case ':': case '@': /* pchar */
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
			/* allowed */
			state->buffer[state->offset++] = *state->ptr;
			break;

		default:
			if (!(mb = parse_mb(state, PARSE_PATH, state->ptr, state->end, tmp, 0))) {
				return NULL;
			}
			state->ptr += mb - 1;
		}
	} while (++state->ptr < state->end);

	done:
	/* did we have any path component ? */
	if (tmp != state->ptr) {
		state->buffer[state->offset++] = 0;
	} else {
		state->url.path = NULL;
	}
	return state->ptr;
}

static const char *parse_query(struct parse_state *state)
{
	size_t mb;
	const char *tmp = state->ptr + !!*state->ptr;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	/* is there actually a query to parse? */
	if (*state->ptr != '?') {
		return state->ptr;
	}

	/* skip initial '?' */
	tmp = ++state->ptr;
	state->url.query = &state->buffer[state->offset];

	while (state->ptr < state->end) {
		switch (*state->ptr) {
		case '#':
			goto done;

		case '%':
			if (state->ptr[1] != '%' && (state->end - state->ptr <= 2 || !isxdigit(*(state->ptr+1)) || !isxdigit(*(state->ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse query; invalid percent encoding at pos %u in '%s'",
						(unsigned) (state->ptr - tmp), tmp);
				return NULL;
			}
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr;
			break;

		/* RFC1738 unsafe */
		case '{': case '}':
		case '<': case '>':
		case '[': case ']':
		case '|': case '\\': case '^': case '`': case '"': case ' ':
			if (state->flags & PHP_HTTP_URL_PARSE_TOPCT) {
				state->buffer[state->offset++] = '%';
				state->buffer[state->offset++] = parse_xdigits[((unsigned char) *state->ptr) >> 4];
				state->buffer[state->offset++] = parse_xdigits[((unsigned char) *state->ptr) & 0xf];
				break;
			}
			/* no break */

		case '?': case '/': /* yeah, well */
		case '!': case '$': case '&': case '\'': case '(': case ')': case '*':
		case '+': case ',': case ';': case '=': /* sub-delims */
		case '-': case '.': case '_': case '~': /* unreserved */
		case ':': case '@': /* pchar */
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
			/* allowed */
			state->buffer[state->offset++] = *state->ptr;
			break;

		default:
			if (!(mb = parse_mb(state, PARSE_QUERY, state->ptr, state->end, tmp, 0))) {
				return NULL;
			}
			state->ptr += mb - 1;
		}

		++state->ptr;
	}

	done:
	state->buffer[state->offset++] = 0;
	return state->ptr;
}

static const char *parse_fragment(struct parse_state *state)
{
	size_t mb;
	const char *tmp;
	TSRMLS_FETCH_FROM_CTX(state->ts);

	/* is there actually a fragment to parse? */
	if (*state->ptr != '#') {
		return state->ptr;
	}

	/* skip initial '#' */
	tmp = ++state->ptr;
	state->url.fragment = &state->buffer[state->offset];

	do {
		switch (*state->ptr) {
		case '%':
			if (state->ptr[1] != '%' && (state->end - state->ptr <= 2 || !isxdigit(*(state->ptr+1)) || !isxdigit(*(state->ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse fragment; invalid percent encoding at pos %u in '%s'",
						(unsigned) (state->ptr - tmp), tmp);
				return NULL;
			}
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr;
			break;

		/* RFC1738 unsafe */
		case '{': case '}':
		case '<': case '>':
		case '[': case ']':
		case '|': case '\\': case '^': case '`': case '"': case ' ':
			if (state->flags & PHP_HTTP_URL_PARSE_TOPCT) {
				state->buffer[state->offset++] = '%';
				state->buffer[state->offset++] = parse_xdigits[((unsigned char) *state->ptr) >> 4];
				state->buffer[state->offset++] = parse_xdigits[((unsigned char) *state->ptr) & 0xf];
				break;
			}
			/* no break */

		case '?': case '/':
		case '!': case '$': case '&': case '\'': case '(': case ')': case '*':
		case '+': case ',': case ';': case '=': /* sub-delims */
		case '-': case '.': case '_': case '~': /* unreserved */
		case ':': case '@': /* pchar */
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
			/* allowed */
			state->buffer[state->offset++] = *state->ptr;
			break;

		default:
			if (!(mb = parse_mb(state, PARSE_FRAGMENT, state->ptr, state->end, tmp, 0))) {
				return NULL;
			}
			state->ptr += mb - 1;
		}
	} while (++state->ptr < state->end);

	state->buffer[state->offset++] = 0;
	return state->ptr;
}

static const char *parse_hier(struct parse_state *state)
{
	if (*state->ptr == '/') {
		if (state->end - state->ptr > 1) {
			if (*(state->ptr + 1) == '/') {
				state->ptr += 2;
				if (!(state->ptr = parse_authority(state))) {
					return NULL;
				}
			}
		}
	}
	return parse_path(state);
}

static const char *parse_scheme(struct parse_state *state)
{
	size_t mb;
	const char *tmp = state->ptr;

	do {
		switch (*state->ptr) {
		case ':':
			/* scheme delimiter */
			state->url.scheme = &state->buffer[0];
			state->buffer[state->offset++] = 0;
			return ++state->ptr;

		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
		case '+': case '-': case '.':
			if (state->ptr == tmp) {
				return tmp;
			}
			/* no break */
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
			/* scheme part */
			state->buffer[state->offset++] = *state->ptr;
			break;

		default:
			if (!(mb = parse_mb(state, PARSE_SCHEME, state->ptr, state->end, tmp, 1))) {
				/* soft fail; parse path next */
				return tmp;
			}
			state->ptr += mb - 1;
		}
	} while (++state->ptr != state->end);

	return state->ptr = tmp;
}

php_http_url_t *php_http_url_parse(const char *str, size_t len, unsigned flags TSRMLS_DC)
{
	size_t maxlen = 3 * len;
	struct parse_state *state = ecalloc(1, sizeof(*state) + maxlen);

	state->end = str + len;
	state->ptr = str;
	state->flags = flags;
	state->maxlen = maxlen;
	TSRMLS_SET_CTX(state->ts);

	if (!parse_scheme(state)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse URL scheme: '%s'", state->ptr);
		efree(state);
		return NULL;
	}

	if (!parse_hier(state)) {
		efree(state);
		return NULL;
	}

	if (!parse_query(state)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse URL query: '%s'", state->ptr);
		efree(state);
		return NULL;
	}

	if (!parse_fragment(state)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse URL fragment: '%s'", state->ptr);
		efree(state);
		return NULL;
	}

	return (php_http_url_t *) state;
}

php_http_url_t *php_http_url_parse_authority(const char *str, size_t len, unsigned flags TSRMLS_DC)
{
	size_t maxlen = 3 * len;
	struct parse_state *state = ecalloc(1, sizeof(*state) + maxlen);

	state->end = str + len;
	state->ptr = str;
	state->flags = flags;
	state->maxlen = maxlen;
	TSRMLS_SET_CTX(state->ts);

	if (!(state->ptr = parse_authority(state))) {
		efree(state);
		return NULL;
	}

	if (state->ptr != state->end) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Failed to parse URL authority, unexpected character at pos %u in '%s'",
				(unsigned) (state->ptr - str), str);
		efree(state);
		return NULL;
	}

	return (php_http_url_t *) state;
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
		php_http_url_t *res_purl, *new_purl = NULL, *old_purl = NULL;

		if (new_url) {
			new_purl = php_http_url_from_zval(new_url, flags TSRMLS_CC);
			if (!new_purl) {
				zend_restore_error_handling(&zeh TSRMLS_CC);
				return;
			}
		}
		if (old_url) {
			old_purl = php_http_url_from_zval(old_url, flags TSRMLS_CC);
			if (!old_purl) {
				if (new_purl) {
					php_http_url_free(&new_purl);
				}
				zend_restore_error_handling(&zeh TSRMLS_CC);
				return;
			}
		}

		res_purl = php_http_url_mod(old_purl, new_purl, flags TSRMLS_CC);
		php_http_url_to_struct(res_purl, getThis() TSRMLS_CC);

		php_http_url_free(&res_purl);
		if (old_purl) {
			php_http_url_free(&old_purl);
		}
		if (new_purl) {
			php_http_url_free(&new_purl);
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
	long flags = PHP_HTTP_URL_JOIN_PATH | PHP_HTTP_URL_JOIN_QUERY | PHP_HTTP_URL_SANITIZE_PATH;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z!|l", &new_url, &flags), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_bad_url_class_entry, &zeh TSRMLS_CC);
	{
		php_http_url_t *new_purl = NULL, *old_purl = NULL;

		if (new_url) {
			new_purl = php_http_url_from_zval(new_url, flags TSRMLS_CC);
			if (!new_purl) {
				zend_restore_error_handling(&zeh TSRMLS_CC);
				return;
			}
		}

		if ((old_purl = php_http_url_from_struct(HASH_OF(getThis())))) {
			php_http_url_t *res_purl;

			ZVAL_OBJVAL(return_value, zend_objects_clone_obj(getThis() TSRMLS_CC), 0);

			res_purl = php_http_url_mod(old_purl, new_purl, flags TSRMLS_CC);
			php_http_url_to_struct(res_purl, return_value TSRMLS_CC);

			php_http_url_free(&res_purl);
			php_http_url_free(&old_purl);
		}
		if (new_purl) {
			php_http_url_free(&new_purl);
		}
	}
	zend_restore_error_handling(&zeh TSRMLS_CC);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl_toString, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, toString)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_url_t *purl;

		if ((purl = php_http_url_from_struct(HASH_OF(getThis())))) {
			char *str;
			size_t len;

			php_http_url_to_string(purl, &str, &len, 0);
			php_http_url_free(&purl);
			RETURN_STRINGL(str, len, 0);
		}
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl_toArray, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, toArray)
{
	php_http_url_t *purl;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	/* strip any non-URL properties */
	purl = php_http_url_from_struct(HASH_OF(getThis()));
	php_http_url_to_struct(purl, return_value TSRMLS_CC);
	php_http_url_free(&purl);
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

#ifdef PHP_HTTP_HAVE_WCHAR
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_MBLOC"), PHP_HTTP_URL_PARSE_MBLOC TSRMLS_CC);
#endif
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_MBUTF8"), PHP_HTTP_URL_PARSE_MBUTF8 TSRMLS_CC);
#if defined(PHP_HTTP_HAVE_IDN2) || defined(PHP_HTTP_HAVE_IDN) || defined(HAVE_UIDNA_IDNTOASCII)
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_TOIDN"), PHP_HTTP_URL_PARSE_TOIDN TSRMLS_CC);
#endif
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_TOPCT"), PHP_HTTP_URL_PARSE_TOPCT TSRMLS_CC);

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

