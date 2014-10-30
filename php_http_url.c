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
		url->scheme = estrndup("http", lenof("http"));
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

void php_http_url_free(php_http_url_t **url)
{
	if (*url) {
		efree(*url);
		*url = NULL;
	}
}

static size_t parse_mb_utf8(php_http_url_t *url, const char *ptr, const char *end, zend_bool idn)
{
	unsigned wchar;
	size_t consumed = utf8towc(&wchar, (const unsigned char *) ptr, end - ptr);

	if (!consumed || consumed == (size_t) -1) {
		return 0;
	}
	if (!idn && !isualnum(wchar)) {
		return 0;
	}

	return consumed;
}

#ifdef PHP_HTTP_HAVE_WCHAR
static size_t parse_mb_loc(php_http_url_t *url, const char *ptr, const char *end, zend_bool idn)
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
	if (!idn && !iswalnum(wchar)) {
		return 0;
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

static size_t parse_mb(php_http_url_t *url, parse_mb_what_t what, const char *ptr, const char *end, const char *begin, zend_bool silent)
{
	size_t consumed = 0;
	zend_bool idn = (what == PARSE_HOSTINFO) && (url->flags & PHP_HTTP_URL_PARSE_IDN);

	if (url->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
		consumed = parse_mb_utf8(url, ptr, end, idn);
	}
#ifdef PHP_HTTP_HAVE_WCHAR
	else if (url->flags & PHP_HTTP_URL_PARSE_MBLOC) {
		consumed = parse_mb_loc(url, ptr, end, idn);
	}
#endif

	if (consumed) {
		if (!(url->flags & PHP_HTTP_URL_PARSE_PCTENC) || what == PARSE_HOSTINFO || what == PARSE_SCHEME) {
			PHP_HTTP_DUFF(consumed, url->buffer[url->offset++] = *ptr++);
		} else {
			int i = 0;

			PHP_HTTP_DUFF(consumed,
					url->buffer[url->offset++] = '%';
					url->buffer[url->offset++] = parse_xdigits[((unsigned char) ptr[i]) >> 4];
					url->buffer[url->offset++] = parse_xdigits[((unsigned char) ptr[i]) & 0xf];
					++i;
			);
		}
	} else if (!silent) {
		TSRMLS_FETCH_FROM_CTX(url->ts);
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Failed to parse %s; unexpected byte 0x%02x at pos %u in '%s'",
				parse_what[what], (unsigned char) *ptr, (unsigned) (ptr - begin), begin);
	}

	return consumed;
}

static STATUS parse_userinfo(php_http_url_t *url, const char *ptr)
{
	size_t mb;
	const char *password = NULL, *end = url->ptr, *tmp = ptr;
	TSRMLS_FETCH_FROM_CTX(url->ts);

	url->user = &url->buffer[url->offset];

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
			url->buffer[url->offset++] = 0;
			url->pass = &url->buffer[url->offset];
			break;

		case '%':
			if (ptr[1] != '%' && (end - ptr <= 2 || !isxdigit(*(ptr+1)) || !isxdigit(*(ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse userinfo; invalid percent encoding at pos %u in '%s'",
						(unsigned) (ptr - tmp), tmp);
				return FAILURE;
			}
			url->buffer[url->offset++] = *ptr++;
			url->buffer[url->offset++] = *ptr++;
			url->buffer[url->offset++] = *ptr;
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
			url->buffer[url->offset++] = *ptr;
			break;

		default:
			if (!(mb = parse_mb(url, PARSE_USERINFO, ptr, end, tmp, 0))) {
				return FAILURE;
			}
			ptr += mb - 1;
		}
	} while(++ptr != end);


	url->buffer[url->offset++] = 0;

	return SUCCESS;
}

static STATUS parse_hostinfo(php_http_url_t *url, const char *ptr)
{
	size_t mb, len;
	const char *end = url->ptr, *tmp = ptr, *port = NULL;
	TSRMLS_FETCH_FROM_CTX(url->ts);


#ifdef HAVE_INET_PTON
	if (*ptr == '[') {
		char *error = NULL, *tmp = memchr(ptr, ']', end - ptr);

		if (tmp) {
			size_t addrlen = tmp - ptr + 1;
			char buf[16], *addr = estrndup(ptr + 1, addrlen - 2);
			int rv = inet_pton(AF_INET6, addr, buf);

			efree(addr);
			if (rv == 1) {
				url->buffer[url->offset] = '[';
				url->host = &url->buffer[url->offset];
				inet_ntop(AF_INET6, buf, url->host + 1, url->maxlen - url->offset);
				url->offset += strlen(url->host);
				url->buffer[url->offset++] = ']';
				url->buffer[url->offset++] = 0;
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
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse hostinfo; %s", error);
			return FAILURE;
		}
	}
#endif
	if (ptr != end) do {
		switch (*ptr) {
		case ':':
			if (port) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse port; duplicate ':' at pos %u in '%s'",
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
			url->buffer[url->offset++] = *ptr++;
			url->buffer[url->offset++] = *ptr++;
			url->buffer[url->offset++] = *ptr;
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
				url->port *= 10;
				url->port += *ptr - '0';
			} else {
				url->buffer[url->offset++] = *ptr;
			}
			break;

		default:
			if (port) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse port; unexpected byte 0x%02x at pos %u in '%s'",
						(unsigned char) *ptr, (unsigned) (ptr - tmp), tmp);
				return FAILURE;
			} else if (!(mb = parse_mb(url, PARSE_HOSTINFO, ptr, end, tmp, 0))) {
				return FAILURE;
			}
			ptr += mb - 1;
		}
	} while (++ptr != end);

	if (!url->host) {
		len = (port ? port - tmp - 1 : end - tmp);
		url->host = &url->buffer[url->offset - len];
		url->buffer[url->offset++] = 0;
	}

#ifdef PHP_HTTP_HAVE_IDN
	if (url->flags & PHP_HTTP_URL_PARSE_IDN) {
		char *idn = NULL;
		int rv = -1;

		if (url->flags & PHP_HTTP_URL_PARSE_MBUTF8) {
			rv = idna_to_ascii_8z(url->host, &idn, IDNA_ALLOW_UNASSIGNED|IDNA_USE_STD3_ASCII_RULES);
		}
#	ifdef PHP_HTTP_HAVE_WCHAR
		else if (url->flags & PHP_HTTP_URL_PARSE_MBLOC) {
			rv = idna_to_ascii_lz(url->host, &idn, IDNA_ALLOW_UNASSIGNED|IDNA_USE_STD3_ASCII_RULES);
		}
#	endif
		if (rv != IDNA_SUCCESS) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse IDN; %s", idna_strerror(rv));
			return FAILURE;
		} else {
			size_t idnlen = strlen(idn);
			memcpy(url->host, idn, idnlen + 1);
			free(idn);
			url->offset += idnlen - len;
		}
	}
#endif

	return SUCCESS;
}

static const char *parse_authority(php_http_url_t *url)
{
	const char *tmp = url->ptr;

	do {
		switch (*url->ptr) {
		case '@':
			/* userinfo delimiter */
			if (tmp != url->ptr && SUCCESS != parse_userinfo(url, tmp)) {
				return NULL;
			}
			tmp = url->ptr + 1;
			break;

		case '/':
		case '?':
		case '#':
		case '\0':
			/* host delimiter */
			if (tmp != url->ptr && SUCCESS != parse_hostinfo(url, tmp)) {
				return NULL;
			}
			return url->ptr;
		}
	} while (++url->ptr <= url->end);

	return NULL;
}

static const char *parse_path(php_http_url_t *url)
{
	size_t mb;
	const char *tmp;
	TSRMLS_FETCH_FROM_CTX(url->ts);

	/* is there actually a path to parse? */
	if (!*url->ptr) {
		return url->ptr;
	}
	tmp = url->ptr;
	url->path = &url->buffer[url->offset];

	do {
		switch (*url->ptr) {
		case '#':
		case '?':
		case '\0':
			/* did we have any path component ? */
			if (tmp != url->ptr) {
				url->buffer[url->offset++] = 0;
			}
			return url->ptr;

		case '%':
			if (url->ptr[1] != '%' && (url->end - url->ptr <= 2 || !isxdigit(*(url->ptr+1)) || !isxdigit(*(url->ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse path; invalid percent encoding at pos %u in '%s'",
						(unsigned) (url->ptr - tmp), tmp);
				return NULL;
			}
			url->buffer[url->offset++] = *url->ptr++;
			url->buffer[url->offset++] = *url->ptr++;
			url->buffer[url->offset++] = *url->ptr;
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
			url->buffer[url->offset++] = *url->ptr;
			break;

		default:
			if (!(mb = parse_mb(url, PARSE_PATH, url->ptr, url->end, tmp, 0))) {
				return NULL;
			}
			url->ptr += mb - 1;
		}
	} while (++url->ptr <= url->end);

	return NULL;
}

static const char *parse_query(php_http_url_t *url)
{
	size_t mb;
	const char *tmp = url->ptr + !!*url->ptr;
	TSRMLS_FETCH_FROM_CTX(url->ts);

	/* is there actually a query to parse ? */
	if (!*url->ptr && *url->ptr != '?') {
		return url->ptr;
	}

	/* skip initial '?' */
	tmp = ++url->ptr;
	url->query = &url->buffer[url->offset];

	do {
		switch (*url->ptr) {
		case '#':
		case '\0':
			url->buffer[url->offset++] = 0;
			return url->ptr;

		case '%':
			if (url->ptr[1] != '%' && (url->end - url->ptr <= 2 || !isxdigit(*(url->ptr+1)) || !isxdigit(*(url->ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse query; invalid percent encoding at pos %u in '%s'",
						(unsigned) (url->ptr - tmp), tmp);
				return NULL;
			}
			url->buffer[url->offset++] = *url->ptr++;
			url->buffer[url->offset++] = *url->ptr++;
			url->buffer[url->offset++] = *url->ptr;
			break;

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
			url->buffer[url->offset++] = *url->ptr;
			break;

		default:
			if (!(mb = parse_mb(url, PARSE_QUERY, url->ptr, url->end, tmp, 0))) {
				return NULL;
			}
			url->ptr += mb - 1;
		}
	} while (++url->ptr <= url->end);

	return NULL;
}

static const char *parse_fragment(php_http_url_t *url)
{
	size_t mb;
	const char *tmp;
	TSRMLS_FETCH_FROM_CTX(url->ts);

	/* is there actually a fragment to parse */
	if (!*url->ptr && *url->ptr != '#') {
		return url->ptr;
	}

	/* skip initial '#' */
	tmp = ++url->ptr;
	url->fragment = &url->buffer[url->offset];

	do {
		switch (*url->ptr) {
		case '\0':
			url->buffer[url->offset++] = 0;
			return url->ptr;

		case '%':
			if (url->ptr[1] != '%' && (url->end - url->ptr <= 2 || !isxdigit(*(url->ptr+1)) || !isxdigit(*(url->ptr+2)))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Failed to parse fragment; invalid percent encoding at pos %u in '%s'",
						(unsigned) (url->ptr - tmp), tmp);
				return NULL;
			}
			url->buffer[url->offset++] = *url->ptr++;
			url->buffer[url->offset++] = *url->ptr++;
			url->buffer[url->offset++] = *url->ptr;
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
			url->buffer[url->offset++] = *url->ptr;
			break;

		default:
			if (!(mb = parse_mb(url, PARSE_FRAGMENT, url->ptr, url->end, tmp, 0))) {
				return NULL;
			}
			url->ptr += mb - 1;
		}
	} while (++url->ptr <= url->end);

	return NULL;
}

static const char *parse_hier(php_http_url_t *url)
{
	if (*url->ptr == '/') {
		if (url->end - url->ptr > 1) {
			if (*(url->ptr + 1) == '/') {
				url->ptr += 2;
				if (!(url->ptr = parse_authority(url))) {
					return NULL;
				}
			}
		}
	}
	return parse_path(url);
}

static const char *parse_scheme(php_http_url_t *url)
{
	size_t mb;
	const char *tmp = url->ptr;

	do {
		switch (*url->ptr) {
		case ':':
			/* scheme delimiter */
			url->scheme = &url->buffer[0];
			url->buffer[url->offset++] = 0;
			return ++url->ptr;

		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
		case '+': case '-': case '.':
			if (url->ptr == tmp) {
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
			url->buffer[url->offset++] = *url->ptr;
			break;

		default:
			if (!(mb = parse_mb(url, PARSE_SCHEME, url->ptr, url->end, tmp, 1))) {
				/* soft fail; parse path next */
				return tmp;
			}
			url->ptr += mb - 1;
		}
	} while (++url->ptr != url->end);

	return tmp;
}

struct parser_state {
};

php_http_url_t *php_http_url_parse(const char *str, size_t len, unsigned flags TSRMLS_DC)
{
	size_t maxlen = 3 * len;
	php_http_url_t *url = ecalloc(1, sizeof(*url) + maxlen);

	url->end = str + len;
	url->ptr = str;
	url->flags = flags;
	url->maxlen = maxlen;
	TSRMLS_SET_CTX(url->ts);

	if (!parse_scheme(url)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse URL scheme: '%s'", url->ptr);
		php_http_url_free(&url);
		return NULL;
	}

	if (!parse_hier(url)) {
		php_http_url_free(&url);
		return NULL;
	}

	if (!parse_query(url)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse URL query: '%s'", url->ptr);
		php_http_url_free(&url);
		return NULL;
	}

	if (!parse_fragment(url)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse URL fragment: '%s'", url->ptr);
		php_http_url_free(&url);
		return NULL;
	}

	return url;
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

ZEND_BEGIN_ARG_INFO_EX(ai_HttpUrl_parse, 0, 0, 1)
	ZEND_ARG_INFO(0, url)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpUrl, parse)
{
	char *str;
	int len;
	long flags = 0;
	php_http_url_t *url;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &str, &len, &flags), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_bad_url_class_entry, &zeh TSRMLS_CC);
	if ((url = php_http_url_parse(str, len, flags TSRMLS_CC))) {
		object_init_ex(return_value, php_http_url_class_entry);
		if (url->scheme) {
			zend_update_property_string(php_http_url_class_entry, return_value,
					ZEND_STRL("scheme"), url->scheme TSRMLS_CC);
		}
		if (url->user) {
			zend_update_property_string(php_http_url_class_entry, return_value,
					ZEND_STRL("user"), url->user TSRMLS_CC);
		}
		if (url->pass) {
			zend_update_property_string(php_http_url_class_entry, return_value,
					ZEND_STRL("pass"), url->pass TSRMLS_CC);
		}
		if (url->host) {
			zend_update_property_string(php_http_url_class_entry, return_value,
					ZEND_STRL("host"), url->host TSRMLS_CC);
		}
		if (url->port) {
			zend_update_property_long(php_http_url_class_entry, return_value,
					ZEND_STRL("port"), url->port TSRMLS_CC);
		}
		if (url->path) {
			zend_update_property_string(php_http_url_class_entry, return_value,
					ZEND_STRL("path"), url->path TSRMLS_CC);
		}
		if (url->query) {
			zend_update_property_string(php_http_url_class_entry, return_value,
					ZEND_STRL("query"), url->query TSRMLS_CC);
		}
		if (url->fragment) {
			zend_update_property_string(php_http_url_class_entry, return_value,
					ZEND_STRL("fragment"), url->fragment TSRMLS_CC);
		}
		php_http_url_free(&url);
	}
	zend_restore_error_handling(&zeh TSRMLS_CC);
}

static zend_function_entry php_http_url_methods[] = {
	PHP_ME(HttpUrl, __construct,  ai_HttpUrl___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpUrl, mod,          ai_HttpUrl_mod, ZEND_ACC_PUBLIC)
	PHP_ME(HttpUrl, toString,     ai_HttpUrl_toString, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpUrl, __toString, toString, ai_HttpUrl_toString, ZEND_ACC_PUBLIC)
	PHP_ME(HttpUrl, toArray,      ai_HttpUrl_toArray, ZEND_ACC_PUBLIC)
	PHP_ME(HttpUrl, parse,        ai_HttpUrl_parse, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
#ifdef PHP_HTTP_HAVE_IDN
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_IDN"), PHP_HTTP_URL_PARSE_IDN TSRMLS_CC);
#endif
	zend_declare_class_constant_long(php_http_url_class_entry, ZEND_STRL("PARSE_PCTENC"), PHP_HTTP_URL_PARSE_PCTENC TSRMLS_CC);

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

