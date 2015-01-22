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

#ifdef PHP_HTTP_HAVE_IDN
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

#define url(buf) ((php_http_url_t *) buf.data)

static php_http_url_t *php_http_url_from_env(void)
{
	zval *https, *zhost, *zport;
	long port;
	php_http_buffer_t buf;

	php_http_buffer_init_ex(&buf, MAX(PHP_HTTP_BUFFER_DEFAULT_SIZE, sizeof(php_http_url_t)<<2), PHP_HTTP_BUFFER_INIT_PREALLOC);
	php_http_buffer_account(&buf, sizeof(php_http_url_t));
	memset(buf.data, 0, buf.used);

	/* scheme */
	url(buf)->scheme = &buf.data[buf.used];
	https = php_http_env_get_server_var(ZEND_STRL("HTTPS"), 1);
	if (https && !strcasecmp(Z_STRVAL_P(https), "ON")) {
		php_http_buffer_append(&buf, "https", sizeof("https"));
	} else {
		php_http_buffer_append(&buf, "http", sizeof("http"));
	}

	/* host */
	url(buf)->host = &buf.data[buf.used];
	if ((((zhost = php_http_env_get_server_var(ZEND_STRL("HTTP_HOST"), 1)) ||
			(zhost = php_http_env_get_server_var(ZEND_STRL("SERVER_NAME"), 1)) ||
			(zhost = php_http_env_get_server_var(ZEND_STRL("SERVER_ADDR"), 1)))) && Z_STRLEN_P(zhost)) {
		size_t stop_at = strspn(Z_STRVAL_P(zhost), "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-.");

		php_http_buffer_append(&buf, Z_STRVAL_P(zhost), stop_at);
		php_http_buffer_append(&buf, "", 1);
	} else {
		char *host_str = localhostname();

		php_http_buffer_append(&buf, host_str, strlen(host_str) + 1);
		efree(host_str);
	}

	/* port */
	zport = php_http_env_get_server_var(ZEND_STRL("SERVER_PORT"), 1);
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
#define url_copy(n) do { \
	if (url_isset(new_url, n)) { \
		url(buf)->n = &buf.data[buf.used]; \
		php_http_buffer_append(&buf, new_url->n, strlen(new_url->n) + 1); \
	} else if (url_isset(old_url, n)) { \
		url(buf)->n = &buf.data[buf.used]; \
		php_http_buffer_append(&buf, old_url->n, strlen(old_url->n) + 1); \
	} \
} while (0)

php_http_url_t *php_http_url_mod(const php_http_url_t *old_url, const php_http_url_t *new_url, unsigned flags)
{
	php_http_url_t *tmp_url = NULL;
	php_http_buffer_t buf;

	php_http_buffer_init_ex(&buf, MAX(PHP_HTTP_BUFFER_DEFAULT_SIZE, sizeof(php_http_url_t)<<2), PHP_HTTP_BUFFER_INIT_PREALLOC);
	php_http_buffer_account(&buf, sizeof(php_http_url_t));
	memset(buf.data, 0, buf.used);

	/* set from env if requested */
	if (flags & PHP_HTTP_URL_FROM_ENV) {
		php_http_url_t *env_url = php_http_url_from_env();

		old_url = tmp_url = php_http_url_mod(env_url, old_url, flags ^ PHP_HTTP_URL_FROM_ENV);
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
				php_http_buffer_append(&buf, "/", 1);
			}
			php_http_buffer_append(&buf, path, strlen(path) + 1);
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

				php_http_buffer_append(&buf, path, strlen(path) + 1);
			}


		}
	}

	if (!(flags & PHP_HTTP_URL_STRIP_QUERY)) {
		if ((flags & PHP_HTTP_URL_JOIN_QUERY) && url_isset(new_url, query) && url_isset(old_url, query)) {
			zval qarr, qstr;
			
			array_init(&qarr);
			
			ZVAL_STRING(&qstr, old_url->query);
			php_http_querystring_update(&qarr, &qstr, NULL);
			zval_ptr_dtor(&qstr);
			ZVAL_STRING(&qstr, new_url->query);
			php_http_querystring_update(&qarr, &qstr, NULL);
			zval_ptr_dtor(&qstr);
			
			ZVAL_NULL(&qstr);
			php_http_querystring_update(&qarr, NULL, &qstr);

			url(buf)->query = &buf.data[buf.used];
			php_http_buffer_append(&buf, Z_STRVAL(qstr), Z_STRLEN(qstr) + 1);

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
		if (	((url(buf)->port == 80) && !strcmp(url(buf)->scheme, "http"))
			||	((url(buf)->port ==443) && !strcmp(url(buf)->scheme, "https"))
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

php_http_url_t *php_http_url_from_zval(zval *value, unsigned flags)
{
	zend_string *zs;
	php_http_url_t *purl;

	switch (Z_TYPE_P(value)) {
	case IS_ARRAY:
	case IS_OBJECT:
		purl = php_http_url_from_struct(HASH_OF(value));
		break;

	default:
		zs = zval_get_string(value);
		purl = php_http_url_parse(zs->val, zs->len, flags);
		zend_string_release(zs);
	}

	return purl;
}

php_http_url_t *php_http_url_from_struct(HashTable *ht)
{
	zval *e;
	php_http_buffer_t buf;

	php_http_buffer_init_ex(&buf, MAX(PHP_HTTP_BUFFER_DEFAULT_SIZE, sizeof(php_http_url_t)<<2), PHP_HTTP_BUFFER_INIT_PREALLOC);
	php_http_buffer_account(&buf, sizeof(php_http_url_t));
	memset(buf.data, 0, buf.used);

	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("scheme")))) {
		zend_string *zs = zval_get_string(e);
		url(buf)->scheme = &buf.data[buf.used];
		php_http_buffer_append(&buf, zs->val, zs->len + 1);
		zend_string_release(zs);
	}
	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("user")))) {
		zend_string *zs = zval_get_string(e);
		url(buf)->user = &buf.data[buf.used];
		php_http_buffer_append(&buf, zs->val, zs->len + 1);
		zend_string_release(zs);
	}
	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("pass")))) {
		zend_string *zs = zval_get_string(e);
		url(buf)->pass = &buf.data[buf.used];
		php_http_buffer_append(&buf, zs->val, zs->len + 1);
		zend_string_release(zs);
	}
	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("host")))) {
		zend_string *zs = zval_get_string(e);
		url(buf)->host = &buf.data[buf.used];
		php_http_buffer_append(&buf, zs->val, zs->len + 1);
		zend_string_release(zs);
	}
	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("port")))) {
		url(buf)->port = (unsigned short) zval_get_long(e);
	}
	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("path")))) {
		zend_string *zs = zval_get_string(e);
		url(buf)->path = &buf.data[buf.used];
		php_http_buffer_append(&buf, zs->val, zs->len + 1);
		zend_string_release(zs);
	}
	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("query")))) {
		zend_string *zs = zval_get_string(e);
		url(buf)->query = &buf.data[buf.used];
		php_http_buffer_append(&buf, zs->val, zs->len + 1);
		zend_string_release(zs);
	}
	if ((e = zend_hash_str_find_ind(ht, ZEND_STRL("fragment")))) {
		zend_string *zs = zval_get_string(e);
		url(buf)->fragment = &buf.data[buf.used];
		php_http_buffer_append(&buf, zs->val, zs->len + 1);
		zend_string_release(zs);
	}

	return url(buf);
}

HashTable *php_http_url_to_struct(const php_http_url_t *url, zval *strct)
{
	HashTable *ht;
	zval tmp;

	if (strct) {
		switch (Z_TYPE_P(strct)) {
			default:
				zval_dtor(strct);
				array_init(strct);
				/* no break */
			case IS_ARRAY:
			case IS_OBJECT:
				ht = HASH_OF(strct);
				break;
		}
	} else {
		ALLOC_HASHTABLE(ht);
		zend_hash_init(ht, 8, NULL, ZVAL_PTR_DTOR, 0);
	}

#define url_struct_add(part) \
	if (Z_TYPE_P(strct) == IS_ARRAY) { \
		zend_hash_str_update(Z_ARRVAL_P(strct), part, lenof(part), &tmp); \
	} else { \
		zend_update_property(Z_OBJCE_P(strct), strct, part, lenof(part), &tmp); \
		zval_ptr_dtor(&tmp); \
	}

	if (url) {
		if (url->scheme) {
			ZVAL_STRING(&tmp, url->scheme);
			url_struct_add("scheme");
		}
		if (url->user) {
			ZVAL_STRING(&tmp, url->user);
			url_struct_add("user");
		}
		if (url->pass) {
			ZVAL_STRING(&tmp, url->pass);
			url_struct_add("pass");
		}
		if (url->host) {
			ZVAL_STRING(&tmp, url->host);
			url_struct_add("host");
		}
		if (url->port) {
			ZVAL_LONG(&tmp, url->port);
			url_struct_add("port");
		}
		if (url->path) {
			ZVAL_STRING(&tmp, url->path);
			url_struct_add("path");
		}
		if (url->query) {
			ZVAL_STRING(&tmp, url->query);
			url_struct_add("query");
		}
		if (url->fragment) {
			ZVAL_STRING(&tmp, url->fragment);
			url_struct_add("fragment");
		}
	}

	return ht;
}

ZEND_RESULT_CODE php_http_url_encode_hash(HashTable *hash, const char *pre_encoded_str, size_t pre_encoded_len, char **encoded_str, size_t *encoded_len)
{
	const char *arg_sep_str;
	size_t arg_sep_len;
	php_http_buffer_t *qstr = php_http_buffer_new();

	php_http_url_argsep(&arg_sep_str, &arg_sep_len);

	if (SUCCESS != php_http_url_encode_hash_ex(hash, qstr, arg_sep_str, arg_sep_len, "=", 1, pre_encoded_str, pre_encoded_len)) {
		php_http_buffer_free(&qstr);
		return FAILURE;
	}

	php_http_buffer_data(qstr, encoded_str, encoded_len);
	php_http_buffer_free(&qstr);

	return SUCCESS;
}

ZEND_RESULT_CODE php_http_url_encode_hash_ex(HashTable *hash, php_http_buffer_t *qstr, const char *arg_sep_str, size_t arg_sep_len, const char *val_sep_str, size_t val_sep_len, const char *pre_encoded_str, size_t pre_encoded_len)
{
	if (pre_encoded_len && pre_encoded_str) {
		php_http_buffer_append(qstr, pre_encoded_str, pre_encoded_len);
	}

	if (!php_http_params_to_string(qstr, hash, arg_sep_str, arg_sep_len, "", 0, val_sep_str, val_sep_len, PHP_HTTP_PARAMS_QUERY)) {
		return FAILURE;
	}

	return SUCCESS;
}

struct parse_state {
	php_http_url_t url;
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
	mbstate_t ps = {0};

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
		php_error_docref(NULL, E_WARNING,
				"Failed to parse %s; unexpected byte 0x%02x at pos %u in '%s'",
				parse_what[what], (unsigned char) *ptr, (unsigned) (ptr - begin), begin);
	}

	return 0;
}

static ZEND_RESULT_CODE parse_userinfo(struct parse_state *state, const char *ptr)
{
	size_t mb;
	const char *password = NULL, *end = state->ptr, *tmp = ptr;

	state->url.user = &state->buffer[state->offset];

	do {
		switch (*ptr) {
		case ':':
			if (password) {
				php_error_docref(NULL, E_WARNING,
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
				php_error_docref(NULL, E_WARNING,
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

static ZEND_RESULT_CODE parse_hostinfo(struct parse_state *state, const char *ptr)
{
	size_t mb, len;
	const char *end = state->ptr, *tmp = ptr, *port = NULL;


#ifdef HAVE_INET_PTON
	if (*ptr == '[') {
		char *error = NULL, *tmp = memchr(ptr, ']', end - ptr);

		if (tmp) {
			size_t addrlen = tmp - ptr + 1;
			char buf[16], *addr = estrndup(ptr + 1, addrlen - 2);
			int rv = inet_pton(AF_INET6, addr, buf);

			efree(addr);
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
		} else {
			error = "expected ']'";
		}

		if (error) {
			php_error_docref(NULL, E_WARNING, "Failed to parse hostinfo; %s", error);
			return FAILURE;
		}
	}
#endif
	if (ptr != end) do {
		switch (*ptr) {
		case ':':
			if (port) {
				php_error_docref(NULL, E_WARNING,
						"Failed to parse port; unexpected ':' at pos %u in '%s'",
						(unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			port = ptr + 1;
			break;

		case '%':
			if (ptr[1] != '%' && (end - ptr <= 2 || !isxdigit(*(ptr+1)) || !isxdigit(*(ptr+2)))) {
				php_error_docref(NULL, E_WARNING,
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
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
			if (port) {
				php_error_docref(NULL, E_WARNING,
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
				state->buffer[state->offset++] = *ptr;
			}
			break;

		default:
			if (port) {
				php_error_docref(NULL, E_WARNING,
						"Failed to parse port; unexpected byte 0x%02x at pos %u in '%s'",
						(unsigned char) *ptr, (unsigned) (ptr - tmp), tmp);
				return FAILURE;
			} else if (!(mb = parse_mb(state, PARSE_HOSTINFO, ptr, end, tmp, 0))) {
				return FAILURE;
			}
			ptr += mb - 1;
		}
	} while (++ptr != end);

	if (!state->url.host) {
		len = (port ? port - tmp - 1 : end - tmp);
		state->url.host = &state->buffer[state->offset - len];
		state->buffer[state->offset++] = 0;
	}

#ifdef PHP_HTTP_HAVE_IDN
	if (state->flags & PHP_HTTP_URL_PARSE_TOIDN) {
		char *idn = NULL;
		int rv = -1;

		if (state->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
			rv = idna_to_ascii_8z(state->url.host, &idn, IDNA_ALLOW_UNASSIGNED|IDNA_USE_STD3_ASCII_RULES);
		}
#	ifdef PHP_HTTP_HAVE_WCHAR
		else if (state->flags & PHP_HTTP_URL_PARSE_MBLOC) {
			rv = idna_to_ascii_lz(state->url.host, &idn, IDNA_ALLOW_UNASSIGNED|IDNA_USE_STD3_ASCII_RULES);
		}
#	endif
		if (rv != IDNA_SUCCESS) {
			php_error_docref(NULL, E_WARNING, "Failed to parse IDN; %s", idna_strerror(rv));
			return FAILURE;
		} else {
			size_t idnlen = strlen(idn);
			memcpy(state->url.host, idn, idnlen + 1);
			free(idn);
			state->offset += idnlen - len;
		}
	}
#endif

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
				php_error_docref(NULL, E_WARNING,
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
			/* host delimiter */
			if (tmp != state->ptr && SUCCESS != parse_hostinfo(state, tmp)) {
				return NULL;
			}
			return state->ptr;
		}
	} while (++state->ptr <= state->end);

	return NULL;
}

static const char *parse_path(struct parse_state *state)
{
	size_t mb;
	const char *tmp;

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
				php_error_docref(NULL, E_WARNING,
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

	/* is there actually a query to parse? */
	if (*state->ptr != '?') {
		return state->ptr;
	}

	/* skip initial '?' */
	tmp = ++state->ptr;
	state->url.query = &state->buffer[state->offset];

	do {
		switch (*state->ptr) {
		case '#':
			goto done;

		case '%':
			if (state->ptr[1] != '%' && (state->end - state->ptr <= 2 || !isxdigit(*(state->ptr+1)) || !isxdigit(*(state->ptr+2)))) {
				php_error_docref(NULL, E_WARNING,
						"Failed to parse query; invalid percent encoding at pos %u in '%s'",
						(unsigned) (state->ptr - tmp), tmp);
				return NULL;
			}
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr;
			break;

		case ']':
		case '[':
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
	} while (++state->ptr < state->end);

	done:
	state->buffer[state->offset++] = 0;
	return state->ptr;
}

static const char *parse_fragment(struct parse_state *state)
{
	size_t mb;
	const char *tmp;

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
				php_error_docref(NULL, E_WARNING,
						"Failed to parse fragment; invalid percent encoding at pos %u in '%s'",
						(unsigned) (state->ptr - tmp), tmp);
				return NULL;
			}
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr++;
			state->buffer[state->offset++] = *state->ptr;
			break;

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

php_http_url_t *php_http_url_parse(const char *str, size_t len, unsigned flags)
{
	size_t maxlen = 3 * len;
	struct parse_state *state = ecalloc(1, sizeof(*state) + maxlen);

	state->end = str + len;
	state->ptr = str;
	state->flags = flags;
	state->maxlen = maxlen;

	if (!parse_scheme(state)) {
		php_error_docref(NULL, E_WARNING, "Failed to parse URL scheme: '%s'", state->ptr);
		efree(state);
		return NULL;
	}

	if (!parse_hier(state)) {
		efree(state);
		return NULL;
	}

	if (!parse_query(state)) {
		php_error_docref(NULL, E_WARNING, "Failed to parse URL query: '%s'", state->ptr);
		efree(state);
		return NULL;
	}

	if (!parse_fragment(state)) {
		php_error_docref(NULL, E_WARNING, "Failed to parse URL fragment: '%s'", state->ptr);
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
	zend_long flags = PHP_HTTP_URL_FROM_ENV;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|z!z!l", &old_url, &new_url, &flags), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_bad_url_class_entry, &zeh);
	{
		php_http_url_t *res_purl, *new_purl = NULL, *old_purl = NULL;

		if (new_url) {
			new_purl = php_http_url_from_zval(new_url, flags);
			if (!new_purl) {
				zend_restore_error_handling(&zeh);
				return;
			}
		}
		if (old_url) {
			old_purl = php_http_url_from_zval(old_url, flags);
			if (!old_purl) {
				if (new_purl) {
					php_http_url_free(&new_purl);
				}
				zend_restore_error_handling(&zeh);
				return;
			}
		}

		res_purl = php_http_url_mod(old_purl, new_purl, flags);
		php_http_url_to_struct(res_purl, getThis());

		php_http_url_free(&res_purl);
		if (old_purl) {
			php_http_url_free(&old_purl);
		}
		if (new_purl) {
			php_http_url_free(&new_purl);
		}
	}
	zend_restore_error_handling(&zeh);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl_mod, 0, 0, 1)
	ZEND_ARG_INFO(0, more_url_parts)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, mod)
{
	zval *new_url = NULL;
	zend_long flags = PHP_HTTP_URL_JOIN_PATH | PHP_HTTP_URL_JOIN_QUERY;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "z!|l", &new_url, &flags), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_bad_url_class_entry, &zeh);
	{
		php_http_url_t *new_purl = NULL, *old_purl = NULL;

		if (new_url) {
			new_purl = php_http_url_from_zval(new_url, flags);
			if (!new_purl) {
				zend_restore_error_handling(&zeh);
				return;
			}
		}

		if ((old_purl = php_http_url_from_struct(HASH_OF(getThis())))) {
			php_http_url_t *res_purl;

			ZVAL_OBJ(return_value, zend_objects_clone_obj(getThis()));

			res_purl = php_http_url_mod(old_purl, new_purl, flags);
			php_http_url_to_struct(res_purl, return_value);

			php_http_url_free(&res_purl);
			php_http_url_free(&old_purl);
		}
		if (new_purl) {
			php_http_url_free(&new_purl);
		}
	}
	zend_restore_error_handling(&zeh);
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
			RETURN_STR(php_http_cs2zs(str, len));
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
	php_http_url_to_struct(purl, return_value);
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
	php_http_url_class_entry = zend_register_internal_class(&ce);

	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("scheme"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("user"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("pass"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("host"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("port"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("path"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("query"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_url_class_entry, ZEND_STRL("fragment"), ZEND_ACC_PUBLIC);

	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("REPLACE"), PHP_HTTP_URL_REPLACE);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("JOIN_PATH"), PHP_HTTP_URL_JOIN_PATH);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("JOIN_QUERY"), PHP_HTTP_URL_JOIN_QUERY);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_USER"), PHP_HTTP_URL_STRIP_USER);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_PASS"), PHP_HTTP_URL_STRIP_PASS);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_AUTH"), PHP_HTTP_URL_STRIP_AUTH);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_PORT"), PHP_HTTP_URL_STRIP_PORT);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_PATH"), PHP_HTTP_URL_STRIP_PATH);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_QUERY"), PHP_HTTP_URL_STRIP_QUERY);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_FRAGMENT"), PHP_HTTP_URL_STRIP_FRAGMENT);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("STRIP_ALL"), PHP_HTTP_URL_STRIP_ALL);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("FROM_ENV"), PHP_HTTP_URL_FROM_ENV);
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("SANITIZE_PATH"), PHP_HTTP_URL_SANITIZE_PATH);

#ifdef PHP_HTTP_HAVE_WCHAR
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_MBLOC"), PHP_HTTP_URL_PARSE_MBLOC);
#endif
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_MBUTF8"), PHP_HTTP_URL_PARSE_MBUTF8);
#ifdef PHP_HTTP_HAVE_IDN
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_TOIDN"), PHP_HTTP_URL_PARSE_TOIDN);
#endif
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_TOPCT"), PHP_HTTP_URL_PARSE_TOPCT);

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

