/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#define _WINSOCKAPI_
#define ZEND_INCLUDE_FULL_WINDOWS_HEADERS

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <ctype.h>

#include "php.h"
#include "php_version.h"
#include "php_streams.h"
#include "snprintf.h"
#include "ext/standard/md5.h"
#include "ext/standard/url.h"
#include "ext/standard/base64.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_lcg.h"

#include "SAPI.h"

#ifdef ZEND_ENGINE_2
#	include "ext/standard/php_http.h"
#else
	#include "http_build_query.c"
#endif

#include "php_http.h"
#include "php_http_api.h"

#ifdef HTTP_HAVE_CURL

#	ifdef PHP_WIN32
#		include <winsock2.h>
#		include <sys/types.h>
#	endif

#	include <curl/curl.h>
#	include <curl/easy.h>

#endif


ZEND_DECLARE_MODULE_GLOBALS(http)

/* {{{ day/month names */
static const char *days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *wkdays[] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};
static const char *weekdays[] = {
	"Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday", "Sunday"
};
static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Okt", "Nov", "Dec"
};
enum assume_next {
	DATE_MDAY,
	DATE_YEAR,
	DATE_TIME
};
static const struct time_zone {
	const char *name;
	const int offset;
} time_zones[] = {
    {"GMT", 0},     /* Greenwich Mean */
    {"UTC", 0},     /* Universal (Coordinated) */
    {"WET", 0},     /* Western European */
    {"BST", 0},     /* British Summer */
    {"WAT", 60},    /* West Africa */
    {"AST", 240},   /* Atlantic Standard */
    {"ADT", 240},   /* Atlantic Daylight */
    {"EST", 300},   /* Eastern Standard */
    {"EDT", 300},   /* Eastern Daylight */
    {"CST", 360},   /* Central Standard */
    {"CDT", 360},   /* Central Daylight */
    {"MST", 420},   /* Mountain Standard */
    {"MDT", 420},   /* Mountain Daylight */
    {"PST", 480},   /* Pacific Standard */
    {"PDT", 480},   /* Pacific Daylight */
    {"YST", 540},   /* Yukon Standard */
    {"YDT", 540},   /* Yukon Daylight */
    {"HST", 600},   /* Hawaii Standard */
    {"HDT", 600},   /* Hawaii Daylight */
    {"CAT", 600},   /* Central Alaska */
    {"AHST", 600},  /* Alaska-Hawaii Standard */
    {"NT",  660},   /* Nome */
    {"IDLW", 720},  /* International Date Line West */
    {"CET", -60},   /* Central European */
    {"MET", -60},   /* Middle European */
    {"MEWT", -60},  /* Middle European Winter */
    {"MEST", -120}, /* Middle European Summer */
    {"CEST", -120}, /* Central European Summer */
    {"MESZ", -60},  /* Middle European Summer */
    {"FWT", -60},   /* French Winter */
    {"FST", -60},   /* French Summer */
    {"EET", -120},  /* Eastern Europe, USSR Zone 1 */
    {"WAST", -420}, /* West Australian Standard */
    {"WADT", -420}, /* West Australian Daylight */
    {"CCT", -480},  /* China Coast, USSR Zone 7 */
    {"JST", -540},  /* Japan Standard, USSR Zone 8 */
    {"EAST", -600}, /* Eastern Australian Standard */
    {"EADT", -600}, /* Eastern Australian Daylight */
    {"GST", -600},  /* Guam Standard, USSR Zone 9 */
    {"NZT", -720},  /* New Zealand */
    {"NZST", -720}, /* New Zealand Standard */
    {"NZDT", -720}, /* New Zealand Daylight */
    {"IDLE", -720}, /* International Date Line East */
};
/* }}} */

/* {{{ internals */

static int http_sort_q(const void *a, const void *b TSRMLS_DC);
#define http_etag(e, p, l, m) _http_etag((e), (p), (l), (m) TSRMLS_CC)
static inline char *_http_etag(char **new_etag, const void *data_ptr, const size_t data_len, const http_send_mode data_mode TSRMLS_DC);
#define http_is_range_request() _http_is_range_request(TSRMLS_C)
static inline int _http_is_range_request(TSRMLS_D);
#define http_send_chunk(d, b, e, m) _http_send_chunk((d), (b), (e), (m) TSRMLS_CC)
static STATUS _http_send_chunk(const void *data, const size_t begin, const size_t end, const http_send_mode mode TSRMLS_DC);

static int check_day(char *day, size_t len);
static int check_month(char *month);
static int check_tzone(char *tzone);

static char *pretty_key(char *key, int key_len, int uctitle, int xhyphen);

static int http_ob_stack_get(php_ob_buffer *, php_ob_buffer **);

/* {{{ HAVE_CURL */
#ifdef HTTP_HAVE_CURL
#define http_curl_initbuf(m) _http_curl_initbuf((m) TSRMLS_CC)
static inline void _http_curl_initbuf(http_curlbuf_member member TSRMLS_DC);
#define http_curl_freebuf(m) _http_curl_freebuf((m) TSRMLS_CC)
static inline void _http_curl_freebuf(http_curlbuf_member member TSRMLS_DC);
#define http_curl_sizebuf(m, l) _http_curl_sizebuf((m), (l) TSRMLS_CC)
static inline void _http_curl_sizebuf(http_curlbuf_member member, size_t len TSRMLS_DC);
#define http_curl_movebuf(m, d, l) _http_curl_movebuf((m), (d), (l) TSRMLS_CC)
static inline void _http_curl_movebuf(http_curlbuf_member member, char **data, size_t *data_len TSRMLS_DC);
#define http_curl_copybuf(m, d, l) _http_curl_copybuf((m), (d), (l) TSRMLS_CC)
static inline void _http_curl_copybuf(http_curlbuf_member member, char **data, size_t *data_len TSRMLS_DC);
#define http_curl_setopts(c, u, o) _http_curl_setopts((c), (u), (o) TSRMLS_CC)
static inline void _http_curl_setopts(CURL *ch, const char *url, HashTable *options TSRMLS_DC);

#define http_curl_getopt(o, k) _http_curl_getopt((o), (k) TSRMLS_CC, 0)
#define http_curl_getopt1(o, k, t1) _http_curl_getopt((o), (k) TSRMLS_CC, 1, (t1))
#define http_curl_getopt2(o, k, t1, t2) _http_curl_getopt((o), (k) TSRMLS_CC, 2, (t1), (t2))
static inline zval *_http_curl_getopt(HashTable *options, char *key TSRMLS_DC, int checks, ...);

static size_t http_curl_body_callback(char *, size_t, size_t, void *);
static size_t http_curl_hdrs_callback(char *, size_t, size_t, void *);

#define http_curl_getinfo(c, h) _http_curl_getinfo((c), (h) TSRMLS_CC)
static inline void _http_curl_getinfo(CURL *ch, HashTable *info TSRMLS_DC);
#define http_curl_getinfo_ex(c, i, a) _http_curl_getinfo_ex((c), (i), (a) TSRMLS_CC)
static inline void _http_curl_getinfo_ex(CURL *ch, CURLINFO i, zval *array TSRMLS_DC);
#define http_curl_getinfoname(i) _http_curl_getinfoname((i) TSRMLS_CC)
static inline char *_http_curl_getinfoname(CURLINFO i TSRMLS_DC);

#endif
/* }}} HAVE_CURL */

/* {{{ static int http_sort_q(const void *, const void *) */
static int http_sort_q(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f, *s;
	zval result, *first, *second;

	f = *((Bucket **) a);
	s = *((Bucket **) b);

	first = *((zval **) f->pData);
	second= *((zval **) s->pData);

	if (numeric_compare_function(&result, first, second TSRMLS_CC) != SUCCESS) {
		return 0;
	}
	return (Z_LVAL(result) > 0 ? -1 : (Z_LVAL(result) < 0 ? 1 : 0));
}
/* }}} */

/* {{{ static inline char *http_etag(char **, void *, size_t, http_send_mode) */
static inline char *_http_etag(char **new_etag, const void *data_ptr,
	const size_t data_len, const http_send_mode data_mode TSRMLS_DC)
{
	char ssb_buf[127];
	unsigned char digest[16];
	PHP_MD5_CTX ctx;

	PHP_MD5Init(&ctx);

	switch (data_mode)
	{
		case SEND_DATA:
			PHP_MD5Update(&ctx, data_ptr, data_len);
		break;

		case SEND_RSRC:
			snprintf(ssb_buf, 127, "%l=%l=%l",
				HTTP_G(ssb).sb.st_mtime,
				HTTP_G(ssb).sb.st_ino,
				HTTP_G(ssb).sb.st_size
			);
			PHP_MD5Update(&ctx, ssb_buf, strlen(ssb_buf));
		break;

		default:
			return NULL;
		break;
	}

	PHP_MD5Final(digest, &ctx);
	make_digest(*new_etag, digest);

	return *new_etag;
}
/* }}} */

/* {{{ static inline int http_is_range_request(void) */
static inline int _http_is_range_request(TSRMLS_D)
{
	return zend_hash_exists(Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_SERVER]),
		"HTTP_RANGE", strlen("HTTP_RANGE") + 1);
}
/* }}} */

/* {{{ static STATUS http_send_chunk(const void *, size_t, size_t,
	http_send_mode) */
static STATUS _http_send_chunk(const void *data, const size_t begin,
	const size_t end, const http_send_mode mode TSRMLS_DC)
{
	char *buf;
	size_t read = 0;
	long len = end - begin;
	php_stream *s;

	switch (mode)
	{
		case SEND_RSRC:
			s = (php_stream *) data;
			if (php_stream_seek(s, begin, SEEK_SET)) {
				return FAILURE;
			}
			buf = (char *) ecalloc(1, HTTP_BUF_SIZE);
			/* read into buf and write out */
			while ((len -= HTTP_BUF_SIZE) >= 0) {
				if (!(read = php_stream_read(s, buf, HTTP_BUF_SIZE))) {
					efree(buf);
					return FAILURE;
				}
				if (read - php_body_write(buf, read TSRMLS_CC)) {
					efree(buf);
					return FAILURE;
				}
			}

			/* read & write left over */
			if (len) {
				if (read = php_stream_read(s, buf, HTTP_BUF_SIZE + len)) {
					if (read - php_body_write(buf, read TSRMLS_CC)) {
						efree(buf);
						return FAILURE;
					}
				} else {
					efree(buf);
					return FAILURE;
				}
			}
			efree(buf);
			return SUCCESS;
		break;

		case SEND_DATA:
			return len == php_body_write(((char *)data) + begin, len TSRMLS_CC)
				? SUCCESS : FAILURE;
		break;

		default:
			return FAILURE;
		break;
	}
}
/* }}} */

/* {{{ HAVE_CURL */
#ifdef HTTP_HAVE_CURL

/* {{{ static inline void http_curl_initbuf(http_curlbuf_member) */
static inline void _http_curl_initbuf(http_curlbuf_member member TSRMLS_DC)
{
	http_curl_freebuf(member);

	if (member & CURLBUF_HDRS) {
		HTTP_G(curlbuf).hdrs.data = emalloc(HTTP_CURLBUF_HDRSSIZE);
		HTTP_G(curlbuf).hdrs.free = HTTP_CURLBUF_HDRSSIZE;
	}
	if (member & CURLBUF_BODY) {
		HTTP_G(curlbuf).body.data = emalloc(HTTP_CURLBUF_BODYSIZE);
		HTTP_G(curlbuf).body.free = HTTP_CURLBUF_BODYSIZE;
	}
}
/* }}} */

/* {{{ static inline void http_curl_freebuf(http_curlbuf_member) */
static inline void _http_curl_freebuf(http_curlbuf_member member TSRMLS_DC)
{
	if (member & CURLBUF_HDRS) {
		if (HTTP_G(curlbuf).hdrs.data) {
			efree(HTTP_G(curlbuf).hdrs.data);
			HTTP_G(curlbuf).hdrs.data = NULL;
		}
		HTTP_G(curlbuf).hdrs.used = 0;
		HTTP_G(curlbuf).hdrs.free = 0;
	}
	if (member & CURLBUF_BODY) {
		if (HTTP_G(curlbuf).body.data) {
			efree(HTTP_G(curlbuf).body.data);
			HTTP_G(curlbuf).body.data = NULL;
		}
		HTTP_G(curlbuf).body.used = 0;
		HTTP_G(curlbuf).body.free = 0;
	}
}
/* }}} */

/* {{{ static inline void http_curl_copybuf(http_curlbuf_member, char **,
	size_t *) */
static inline void _http_curl_copybuf(http_curlbuf_member member, char **data,
	size_t *data_len TSRMLS_DC)
{
	*data = NULL;
	*data_len = 0;

	if ((member & CURLBUF_HDRS) && HTTP_G(curlbuf).hdrs.used) {
		if ((member & CURLBUF_BODY) && HTTP_G(curlbuf).body.used) {
			*data = emalloc(HTTP_G(curlbuf).hdrs.used + HTTP_G(curlbuf).body.used + 1);
		} else {
			*data = emalloc(HTTP_G(curlbuf).hdrs.used + 1);
		}
		memcpy(*data, HTTP_G(curlbuf).hdrs.data, HTTP_G(curlbuf).hdrs.used);
		*data_len = HTTP_G(curlbuf).hdrs.used;
	}

	if ((member & CURLBUF_BODY) && HTTP_G(curlbuf).body.used) {
		if (*data) {
			memcpy((*data) + HTTP_G(curlbuf).hdrs.used,
				HTTP_G(curlbuf).body.data, HTTP_G(curlbuf).body.used);
			*data_len = HTTP_G(curlbuf).hdrs.used + HTTP_G(curlbuf).body.used;
		} else {
			emalloc(HTTP_G(curlbuf).body.used + 1);
			memcpy(*data, HTTP_G(curlbuf).body.data, HTTP_G(curlbuf).body.used);
			*data_len = HTTP_G(curlbuf).body.used;
		}
	}
	if (*data) {
		(*data)[*data_len] = 0;
	} else {
		*data = "";
	}
}
/* }}} */

/* {{{ static inline void http_curl_movebuf(http_curlbuf_member, char **,
	size_t *) */
static inline void _http_curl_movebuf(http_curlbuf_member member, char **data,
	size_t *data_len TSRMLS_DC)
{
	http_curl_copybuf(member, data, data_len);
	http_curl_freebuf(member);
}
/* }}} */

/* {{{ static size_t http_curl_body_callback(char *, size_t, size_t, void *) */
static size_t http_curl_body_callback(char *buf, size_t len, size_t n, void *s)
{
	TSRMLS_FETCH();

	if ((len *= n) > HTTP_G(curlbuf).body.free) {
		size_t bsize = HTTP_CURLBUF_BODYSIZE;
		while (bsize < len) {
			bsize *= 2;
		}
		HTTP_G(curlbuf).body.data = erealloc(HTTP_G(curlbuf).body.data,
			HTTP_G(curlbuf).body.used + bsize);
		HTTP_G(curlbuf).body.free += bsize;
	}

	memcpy(HTTP_G(curlbuf).body.data + HTTP_G(curlbuf).body.used, buf, len);
	HTTP_G(curlbuf).body.free -= len;
	HTTP_G(curlbuf).body.used += len;

	return len;
}
/* }}} */

/* {{{ static size_t http_curl_hdrs_callback(char*, size_t, size_t, void *) */
static size_t http_curl_hdrs_callback(char *buf, size_t len, size_t n, void *s)
{
	TSRMLS_FETCH();

	/* discard previous headers */
	if ((HTTP_G(curlbuf).hdrs.used) && (!strncmp(buf, "HTTP/1.", strlen("HTTP/1.")))) {
		http_curl_initbuf(CURLBUF_HDRS);
	}

	if ((len *= n) > HTTP_G(curlbuf).hdrs.free) {
		size_t bsize = HTTP_CURLBUF_HDRSSIZE;
		while (bsize < len) {
			bsize *= 2;
		}
		HTTP_G(curlbuf).hdrs.data = erealloc(HTTP_G(curlbuf).hdrs.data,
			HTTP_G(curlbuf).hdrs.used + bsize);
		HTTP_G(curlbuf).hdrs.free += bsize;
	}

	memcpy(HTTP_G(curlbuf).hdrs.data + HTTP_G(curlbuf).hdrs.used, buf, len);
	HTTP_G(curlbuf).hdrs.free -= len;
	HTTP_G(curlbuf).hdrs.used += len;

	return len;
}
/* }}} */

/* {{{ static inline zval *http_curl_getopt(HashTable *, char *, int, ...) */
static inline zval *_http_curl_getopt(HashTable *options, char *key TSRMLS_DC, int checks, ...)
{
	zval **zoption;
	va_list types;
	int i;

	if (SUCCESS != zend_hash_find(options, key, strlen(key) + 1, (void **) &zoption)) {
		return NULL;
	}
	if (checks < 1) {
		return *zoption;
	}

	va_start(types, checks);
	for (i = 0; i < checks; ++i) {
		if ((va_arg(types, int)) == (Z_TYPE_PP(zoption))) {
			va_end(types);
			return *zoption;
		}
	}
	va_end(types);
	return NULL;
}
/* }}} */

/* {{{ static inline void http_curl_setopts(CURL *, char *, HashTable *) */
static inline void _http_curl_setopts(CURL *ch, const char *url, HashTable *options TSRMLS_DC)
{
	zval *zoption;

	/* standard options */
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_HEADER, 0);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(ch, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, http_curl_body_callback);
	curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, http_curl_hdrs_callback);
#ifdef ZTS
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
#endif

	if ((!options) || (1 > zend_hash_num_elements(options))) {
		return;
	}

	/* redirects, defaults to 0 */
	if (zoption = http_curl_getopt1(options, "redirect", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1 : 0);
		curl_easy_setopt(ch, CURLOPT_MAXREDIRS, Z_LVAL_P(zoption));
		if (zoption = http_curl_getopt2(options, "unrestrictedauth", IS_LONG, IS_BOOL)) {
			curl_easy_setopt(ch, CURLOPT_UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
	} else {
		curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 0);
	}

	/* proxy */
	if (zoption = http_curl_getopt1(options, "proxyhost", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_PROXY, Z_STRVAL_P(zoption));
		/* port */
		if (zoption = http_curl_getopt1(options, "proxyport", IS_LONG)) {
			curl_easy_setopt(ch, CURLOPT_PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if (zoption = http_curl_getopt1(options, "proxyauth", IS_STRING)) {
			curl_easy_setopt(ch, CURLOPT_PROXYUSERPWD, Z_STRVAL_P(zoption));
		}
		/* auth method */
		if (zoption = http_curl_getopt1(options, "proxyauthtype", IS_LONG)) {
			curl_easy_setopt(ch, CURLOPT_PROXYAUTH, Z_LVAL_P(zoption));
		}
	}

	/* auth */
	if (zoption = http_curl_getopt1(options, "httpauth", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_USERPWD, Z_STRVAL_P(zoption));
	}
	if (zoption = http_curl_getopt1(options, "httpauthtype", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_HTTPAUTH, Z_LVAL_P(zoption));
	}

	/* compress, enabled by default (empty string enables deflate and gzip) */
	if (zoption = http_curl_getopt2(options, "compress", IS_LONG, IS_BOOL)) {
		if (Z_LVAL_P(zoption)) {
			curl_easy_setopt(ch, CURLOPT_ENCODING, "");
		}
	} else {
		curl_easy_setopt(ch, CURLOPT_ENCODING, "");
	}

	/* another port */
	if (zoption = http_curl_getopt1(options, "port", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_PORT, Z_LVAL_P(zoption));
	}

	/* referer */
	if (zoption = http_curl_getopt1(options, "referer", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_REFERER, Z_STRVAL_P(zoption));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if (zoption = http_curl_getopt1(options, "useragent", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_USERAGENT, Z_STRVAL_P(zoption));
	} else {
		curl_easy_setopt(ch, CURLOPT_USERAGENT,
			"PECL::HTTP/" PHP_EXT_HTTP_VERSION " (PHP/" PHP_VERSION ")");
	}

	/* cookies, array('name' => 'value') */
	if (zoption = http_curl_getopt1(options, "cookies", IS_ARRAY)) {
		char *cookie_key;
		zval **cookie_val;
		int key_type;
		smart_str qstr = {0};

		zend_hash_internal_pointer_reset(Z_ARRVAL_P(zoption));
		while (HASH_KEY_NON_EXISTANT != (key_type = zend_hash_get_current_key_type(Z_ARRVAL_P(zoption)))) {
			if (key_type == HASH_KEY_IS_STRING) {
				zend_hash_get_current_key(Z_ARRVAL_P(zoption), &cookie_key, NULL, 0);
				zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &cookie_val);
				smart_str_appends(&qstr, cookie_key);
				smart_str_appendl(&qstr, "=", 1);
				smart_str_appendl(&qstr, Z_STRVAL_PP(cookie_val), Z_STRLEN_PP(cookie_val));
				smart_str_appendl(&qstr, "; ", 2);
				zend_hash_move_forward(Z_ARRVAL_P(zoption));
			}
		}
		smart_str_0(&qstr);

		if (qstr.c) {
			curl_easy_setopt(ch, CURLOPT_COOKIE, qstr.c);
		}
	}

	/* cookiestore */
	if (zoption = http_curl_getopt1(options, "cookiestore", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_COOKIEFILE, Z_STRVAL_P(zoption));
		curl_easy_setopt(ch, CURLOPT_COOKIEJAR, Z_STRVAL_P(zoption));
	}

	/* additional headers, array('name' => 'value') */
	if (zoption = http_curl_getopt1(options, "headers", IS_ARRAY)) {
		int key_type;
		char *header_key, header[1024] = {0};
		zval **header_val;
		struct curl_slist *headers = NULL;

		zend_hash_internal_pointer_reset(Z_ARRVAL_P(zoption));
		while (HASH_KEY_NON_EXISTANT != (key_type = zend_hash_get_current_key_type(Z_ARRVAL_P(zoption)))) {
			if (key_type == HASH_KEY_IS_STRING) {
				zend_hash_get_current_key(Z_ARRVAL_P(zoption), &header_key, NULL, 0);
				zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &header_val);
				snprintf(header, 1023, "%s: %s", header_key, Z_STRVAL_PP(header_val));
				headers = curl_slist_append(headers, header);
				zend_hash_move_forward(Z_ARRVAL_P(zoption));
			}
		}
		if (headers) {
			curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
		}
	}
}
/* }}} */

/* {{{ static inline char *http_curl_getinfoname(CURLINFO) */
static inline char *_http_curl_getinfoname(CURLINFO i TSRMLS_DC)
{
#define CASE(I) case CURLINFO_ ##I : return pretty_key(estrdup( #I ), strlen(#I), 0, 0)
	switch (i)
	{
		/* CURLINFO_EFFECTIVE_URL			=	CURLINFO_STRING	+1, */
		CASE(EFFECTIVE_URL);
		/* CURLINFO_RESPONSE_CODE			=	CURLINFO_LONG	+2, */
		CASE(RESPONSE_CODE);
		/* CURLINFO_TOTAL_TIME				=	CURLINFO_DOUBLE	+3, */
		CASE(TOTAL_TIME);
		/* CURLINFO_NAMELOOKUP_TIME			=	CURLINFO_DOUBLE	+4, */
		CASE(NAMELOOKUP_TIME);
		/* CURLINFO_CONNECT_TIME			=	CURLINFO_DOUBLE	+5, */
		CASE(CONNECT_TIME);
		/* CURLINFO_PRETRANSFER_TIME		=	CURLINFO_DOUBLE	+6, */
		CASE(PRETRANSFER_TIME);
		/* CURLINFO_SIZE_UPLOAD				=	CURLINFO_DOUBLE	+7, */
		CASE(SIZE_UPLOAD);
		/* CURLINFO_SIZE_DOWNLOAD			=	CURLINFO_DOUBLE	+8, */
		CASE(SIZE_DOWNLOAD);
		/* CURLINFO_SPEED_DOWNLOAD			=	CURLINFO_DOUBLE	+9, */
		CASE(SPEED_DOWNLOAD);
		/* CURLINFO_SPEED_UPLOAD			=	CURLINFO_DOUBLE	+10, */
		CASE(SPEED_UPLOAD);
		/* CURLINFO_HEADER_SIZE				=	CURLINFO_LONG	+11, */
		CASE(HEADER_SIZE);
		/* CURLINFO_REQUEST_SIZE			=	CURLINFO_LONG	+12, */
		CASE(REQUEST_SIZE);
		/* CURLINFO_SSL_VERIFYRESULT		=	CURLINFO_LONG	+13, */
		CASE(SSL_VERIFYRESULT);
		/* CURLINFO_FILETIME				=	CURLINFO_LONG	+14, */
		CASE(FILETIME);
		/* CURLINFO_CONTENT_LENGTH_DOWNLOAD	=	CURLINFO_DOUBLE	+15, */
		CASE(CONTENT_LENGTH_DOWNLOAD);
		/* CURLINFO_CONTENT_LENGTH_UPLOAD	=	CURLINFO_DOUBLE	+16, */
		CASE(CONTENT_LENGTH_UPLOAD);
		/* CURLINFO_STARTTRANSFER_TIME		=	CURLINFO_DOUBLE	+17, */
		CASE(STARTTRANSFER_TIME);
		/* CURLINFO_CONTENT_TYPE			=	CURLINFO_STRING	+18, */
		CASE(CONTENT_TYPE);
		/* CURLINFO_REDIRECT_TIME			=	CURLINFO_DOUBLE	+19, */
		CASE(REDIRECT_TIME);
		/* CURLINFO_REDIRECT_COUNT			=	CURLINFO_LONG	+20, */
		CASE(REDIRECT_COUNT);
		/* CURLINFO_PRIVATE					=	CURLINFO_STRING	+21, */
		CASE(PRIVATE);
		/* CURLINFO_HTTP_CONNECTCODE		=	CURLINFO_LONG	+22, */
		CASE(HTTP_CONNECTCODE);
		/* CURLINFO_HTTPAUTH_AVAIL			=	CURLINFO_LONG	+23, */
		CASE(HTTPAUTH_AVAIL);
		/* CURLINFO_PROXYAUTH_AVAIL			=	CURLINFO_LONG	+24, */
		CASE(PROXYAUTH_AVAIL);
	}
#undef CASE
	return NULL;
}
/* }}} */

/* {{{ static inline void http_curl_getinfo_ex(CURL, CURLINFO, zval *) */
static inline void _http_curl_getinfo_ex(CURL *ch, CURLINFO i, zval *array TSRMLS_DC)
{
	char *key;
	if (key = http_curl_getinfoname(i)) {
		switch (i & ~CURLINFO_MASK)
		{
			case CURLINFO_STRING:
			{
				char *c;
				if (CURLE_OK == curl_easy_getinfo(ch, i, &c)) {
					add_assoc_string(array, key, c ? c : "", 1);
				}
			}
			break;

			case CURLINFO_DOUBLE:
			{
				double d;
				if (CURLE_OK == curl_easy_getinfo(ch, i, &d)) {
					add_assoc_double(array, key, d);
				}
			}
			break;

			case CURLINFO_LONG:
			{
				long l;
				if (CURLE_OK == curl_easy_getinfo(ch, i, &l)) {
					add_assoc_long(array, key, l);
				}
			}
			break;
		}
	}
}
/* }}} */

/* {{{ static inline http_curl_getinfo(CURL, HashTable *) */
static inline void _http_curl_getinfo(CURL *ch, HashTable *info TSRMLS_DC)
{
	zval array;
	Z_ARRVAL(array) = info;

#define INFO(I) http_curl_getinfo_ex(ch, CURLINFO_ ##I , &array)
	/* CURLINFO_EFFECTIVE_URL			=	CURLINFO_STRING	+1, */
	INFO(EFFECTIVE_URL);
	/* CURLINFO_RESPONSE_CODE			=	CURLINFO_LONG	+2, */
	INFO(RESPONSE_CODE);
	/* CURLINFO_TOTAL_TIME				=	CURLINFO_DOUBLE	+3, */
	INFO(TOTAL_TIME);
	/* CURLINFO_NAMELOOKUP_TIME			=	CURLINFO_DOUBLE	+4, */
	INFO(NAMELOOKUP_TIME);
	/* CURLINFO_CONNECT_TIME			=	CURLINFO_DOUBLE	+5, */
	INFO(CONNECT_TIME);
	/* CURLINFO_PRETRANSFER_TIME		=	CURLINFO_DOUBLE	+6, */
	INFO(PRETRANSFER_TIME);
	/* CURLINFO_SIZE_UPLOAD				=	CURLINFO_DOUBLE	+7, */
	INFO(SIZE_UPLOAD);
	/* CURLINFO_SIZE_DOWNLOAD			=	CURLINFO_DOUBLE	+8, */
	INFO(SIZE_DOWNLOAD);
	/* CURLINFO_SPEED_DOWNLOAD			=	CURLINFO_DOUBLE	+9, */
	INFO(SPEED_DOWNLOAD);
	/* CURLINFO_SPEED_UPLOAD			=	CURLINFO_DOUBLE	+10, */
	INFO(SPEED_UPLOAD);
	/* CURLINFO_HEADER_SIZE				=	CURLINFO_LONG	+11, */
	INFO(HEADER_SIZE);
	/* CURLINFO_REQUEST_SIZE			=	CURLINFO_LONG	+12, */
	INFO(REQUEST_SIZE);
	/* CURLINFO_SSL_VERIFYRESULT		=	CURLINFO_LONG	+13, */
	INFO(SSL_VERIFYRESULT);
	/* CURLINFO_FILETIME				=	CURLINFO_LONG	+14, */
	INFO(FILETIME);
	/* CURLINFO_CONTENT_LENGTH_DOWNLOAD	=	CURLINFO_DOUBLE	+15, */
	INFO(CONTENT_LENGTH_DOWNLOAD);
	/* CURLINFO_CONTENT_LENGTH_UPLOAD	=	CURLINFO_DOUBLE	+16, */
	INFO(CONTENT_LENGTH_UPLOAD);
	/* CURLINFO_STARTTRANSFER_TIME		=	CURLINFO_DOUBLE	+17, */
	INFO(STARTTRANSFER_TIME);
	/* CURLINFO_CONTENT_TYPE			=	CURLINFO_STRING	+18, */
	INFO(CONTENT_TYPE);
	/* CURLINFO_REDIRECT_TIME			=	CURLINFO_DOUBLE	+19, */
	INFO(REDIRECT_TIME);
	/* CURLINFO_REDIRECT_COUNT			=	CURLINFO_LONG	+20, */
	INFO(REDIRECT_COUNT);
	/* CURLINFO_PRIVATE					=	CURLINFO_STRING	+21, */
	INFO(PRIVATE);
	/* CURLINFO_HTTP_CONNECTCODE		=	CURLINFO_LONG	+22, */
	INFO(HTTP_CONNECTCODE);
	/* CURLINFO_HTTPAUTH_AVAIL			=	CURLINFO_LONG	+23, */
	INFO(HTTPAUTH_AVAIL);
	/* CURLINFO_PROXYAUTH_AVAIL			=	CURLINFO_LONG	+24, */
	INFO(PROXYAUTH_AVAIL);
#undef INFO
}
/* }}} */

#endif
/* }}} HAVE_CURL */

/* {{{ Day/Month/TZ checks for http_parse_date()
	Originally by libcurl, Copyright (C) 1998 - 2004, Daniel Stenberg, <daniel@haxx.se>, et al. */
static int check_day(char *day, size_t len)
{
	int i;
	const char * const *check = (len > 3) ? &weekdays[0] : &wkdays[0];
	for (i = 0; i < 7; i++) {
	    if (!strcmp(day, check[0])) {
	    	return i;
		}
		check++;
	}
	return -1;
}

static int check_month(char *month)
{
	int i;
	const char * const *check = &months[0];
	for (i = 0; i < 12; i++) {
		if (!strcmp(month, check[0])) {
			return i;
		}
		check++;
	}
	return -1;
}

/* return the time zone offset between GMT and the input one, in number
   of seconds or -1 if the timezone wasn't found/legal */

static int check_tzone(char *tzone)
{
	int i;
	const struct time_zone *check = time_zones;
	for (i = 0; i < sizeof(time_zones) / sizeof(time_zones[0]); i++) {
		if (!strcmp(tzone, check->name)) {
			return check->offset * 60;
		}
		check++;
	}
	return -1;
}
/* }}} */

/* static char *pretty_key(char *, int, int, int) */
static char *pretty_key(char *key, int key_len, int uctitle, int xhyphen)
{
	if (key && key_len) {
		int i, wasalpha;
		if (wasalpha = isalpha(key[0])) {
			key[0] = uctitle ? toupper(key[0]) : tolower(key[0]);
		}
		for (i = 1; i < key_len; i++) {
			if (isalpha(key[i])) {
				key[i] = ((!wasalpha) && uctitle) ? toupper(key[i]) : tolower(key[i]);
				wasalpha = 1;
			} else {
				if (xhyphen && (key[i] == '_')) {
					key[i] = '-';
				}
				wasalpha = 0;
			}
		}
	}
	return key;
}
/* }}} */

/* {{{ static STATUS http_ob_stack_get(php_ob_buffer *, php_ob_buffer **) */
static STATUS http_ob_stack_get(php_ob_buffer *o, php_ob_buffer **s)
{
	static int i = 0;
	php_ob_buffer *b = emalloc(sizeof(php_ob_buffer));
	b->handler_name = estrdup(o->handler_name);
	b->buffer = estrndup(o->buffer, o->text_length);
	b->text_length = o->text_length;
	b->chunk_size = o->chunk_size;
	b->erase = o->erase;
	s[i++] = b;
	return SUCCESS;
}
/* }}} */

/* }}} internals */

/* {{{ public API */

/* {{{ char *http_date(time_t) */
PHP_HTTP_API char *_http_date(time_t t TSRMLS_DC)
{
	struct tm *gmtime, tmbuf;
	char *date = ecalloc(31, 1);

	gmtime = php_gmtime_r(&t, &tmbuf);
	snprintf(date, 30,
		"%s, %02d %s %04d %02d:%02d:%02d GMT",
		days[gmtime->tm_wday], gmtime->tm_mday,
		months[gmtime->tm_mon], gmtime->tm_year + 1900,
		gmtime->tm_hour, gmtime->tm_min, gmtime->tm_sec
	);
	return date;
}
/* }}} */

/* {{{ time_t http_parse_date(char *)
	Originally by libcurl, Copyright (C) 1998 - 2004, Daniel Stenberg, <daniel@haxx.se>, et al. */
PHP_HTTP_API time_t _http_parse_date(const char *date)
{
	time_t t = 0;
	int tz_offset = -1, year = -1, month = -1, monthday = -1, weekday = -1,
		hours = -1, minutes = -1, seconds = -1;
	struct tm tm;
	enum assume_next dignext = DATE_MDAY;
	const char *indate = date;

	int found = 0, part = 0; /* max 6 parts */

	while (*date && (part < 6)) {
		int found = 0;

		while (*date && !isalnum(*date)) {
			date++;
		}

		if (isalpha(*date)) {
			/* a name coming up */
			char buf[32] = "";
			size_t len;
			sscanf(date, "%31[A-Za-z]", buf);
			len = strlen(buf);

			if (weekday == -1) {
				weekday = check_day(buf, len);
				if (weekday != -1) {
					found = 1;
				}
			}

			if (!found && (month == -1)) {
				month = check_month(buf);
				if (month != -1) {
					found = 1;
				}
			}

			if (!found && (tz_offset == -1)) {
				/* this just must be a time zone string */
				tz_offset = check_tzone(buf);
				if (tz_offset != -1) {
					found = 1;
				}
			}

			if (!found) {
				return -1; /* bad string */
			}
			date += len;
		}
		else if (isdigit(*date)) {
			/* a digit */
			int val;
			char *end;
			if((seconds == -1) && (3 == sscanf(date, "%02d:%02d:%02d", &hours, &minutes, &seconds))) {
				/* time stamp! */
				date += 8;
				found = 1;
			}
			else {
				val = (int) strtol(date, &end, 10);

				if ((tz_offset == -1) && ((end - date) == 4) && (val < 1300) &&	(indate < date) && ((date[-1] == '+' || date[-1] == '-'))) {
					/* four digits and a value less than 1300 and it is preceeded with
					a plus or minus. This is a time zone indication. */
					found = 1;
					tz_offset = (val / 100 * 60 + val % 100) * 60;

					/* the + and - prefix indicates the local time compared to GMT,
					this we need ther reversed math to get what we want */
					tz_offset = date[-1] == '+' ? -tz_offset : tz_offset;
				}

				if (((end - date) == 8) && (year == -1) && (month == -1) && (monthday == -1)) {
					/* 8 digits, no year, month or day yet. This is YYYYMMDD */
					found = 1;
					year = val / 10000;
					month = (val % 10000) / 100 - 1; /* month is 0 - 11 */
					monthday = val % 100;
				}

				if (!found && (dignext == DATE_MDAY) && (monthday == -1)) {
					if ((val > 0) && (val < 32)) {
						monthday = val;
						found = 1;
					}
					dignext = DATE_YEAR;
				}

				if (!found && (dignext == DATE_YEAR) && (year == -1)) {
					year = val;
					found = 1;
					if (year < 1900) {
						year += year > 70 ? 1900 : 2000;
					}
					if(monthday == -1) {
						dignext = DATE_MDAY;
					}
				}

				if (!found) {
					return -1;
				}

				date = end;
			}
		}

		part++;
	}

	if (-1 == seconds) {
		seconds = minutes = hours = 0; /* no time, make it zero */
	}

	if ((-1 == monthday) || (-1 == month) || (-1 == year)) {
		/* lacks vital info, fail */
		return -1;
	}

	if (sizeof(time_t) < 5) {
		/* 32 bit time_t can only hold dates to the beginning of 2038 */
		if (year > 2037) {
			return 0x7fffffff;
		}
	}

	tm.tm_sec = seconds;
	tm.tm_min = minutes;
	tm.tm_hour = hours;
	tm.tm_mday = monthday;
	tm.tm_mon = month;
	tm.tm_year = year - 1900;
	tm.tm_wday = 0;
	tm.tm_yday = 0;
	tm.tm_isdst = 0;

	t = mktime(&tm);

	/* time zone adjust */
	{
		struct tm *gmt, keeptime2;
		long delta;
		time_t t2;

		if(!(gmt = php_gmtime_r(&t, &keeptime2))) {
			return -1; /* illegal date/time */
		}

		t2 = mktime(gmt);

		/* Add the time zone diff (between the given timezone and GMT) and the
		diff between the local time zone and GMT. */
		delta = (tz_offset != -1 ? tz_offset : 0) + (t - t2);

		if((delta > 0) && (t + delta < t)) {
			return -1; /* time_t overflow */
		}

		t += delta;
	}

	return t;
}
/* }}} */

/* {{{ inline STATUS http_send_status(int) */
PHP_HTTP_API inline STATUS _http_send_status(const int status TSRMLS_DC)
{
	int s = status;
	return sapi_header_op(SAPI_HEADER_SET_STATUS, (void *) s TSRMLS_CC);
}
/* }}} */

/* {{{ inline STATUS http_send_header(char *) */
PHP_HTTP_API inline STATUS _http_send_header(const char *header TSRMLS_DC)
{
	return http_send_status_header(0, header);
}
/* }}} */

/* {{{ inline STATUS http_send_status_header(int, char *) */
PHP_HTTP_API inline STATUS _http_send_status_header(const int status, const char *header TSRMLS_DC)
{
	sapi_header_line h = {(char *) header, strlen(header), status};
	return sapi_header_op(SAPI_HEADER_REPLACE, &h TSRMLS_CC);
}
/* }}} */

/* {{{ inline zval *http_get_server_var(char *) */
PHP_HTTP_API inline zval *_http_get_server_var(const char *key TSRMLS_DC)
{
	zval **var;
	if (SUCCESS == zend_hash_find(
			HTTP_SERVER_VARS,
			(char *) key, strlen(key) + 1, (void **) &var)) {
		return *var;
	}
	return NULL;
}
/* }}} */

/* {{{ void http_ob_etaghandler(char *, uint, char **, uint *, int) */
PHP_HTTP_API void _http_ob_etaghandler(char *output, uint output_len,
	char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	char etag[33] = { 0 };
	unsigned char digest[16];

	if (mode & PHP_OUTPUT_HANDLER_START) {
		PHP_MD5Init(&HTTP_G(etag_md5));
	}

	PHP_MD5Update(&HTTP_G(etag_md5), output, output_len);

	if (mode & PHP_OUTPUT_HANDLER_END) {
		PHP_MD5Final(digest, &HTTP_G(etag_md5));

		/* just do that if desired */
		if (HTTP_G(etag_started)) {
			make_digest(etag, digest);

			if (http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
				http_send_status(304);
			} else {
				http_send_etag(etag, 32);
			}
		}
	}

	*handled_output_len = output_len;
	*handled_output = estrndup(output, output_len);
}
/* }}} */

/* {{{ STATUS http_start_ob_handler(php_output_handler_func_t, char *, uint, zend_bool) */
PHP_HTTP_API STATUS _http_start_ob_handler(php_output_handler_func_t handler_func,
	char *handler_name, uint chunk_size, zend_bool erase TSRMLS_DC)
{
	php_ob_buffer **stack;
	int count, i;

	if (count = OG(ob_nesting_level)) {
		stack = ecalloc(sizeof(php_ob_buffer), count);

		if (count > 1) {
			zend_stack_apply_with_argument(&OG(ob_buffers), ZEND_STACK_APPLY_BOTTOMUP,
				(int (*)(void *elem, void *)) http_ob_stack_get, stack);
		}

		if (count > 0) {
			http_ob_stack_get(&OG(active_ob_buffer), stack);
		}

		while (OG(ob_nesting_level)) {
			php_end_ob_buffer(0, 0 TSRMLS_CC);
		}
	}

	php_ob_set_internal_handler(handler_func, chunk_size, handler_name, erase TSRMLS_CC);

	for (i = 0; i < count; i++) {
		php_ob_buffer *s = stack[i];
		if (strcmp(s->handler_name, "default output handler")) {
			php_start_ob_buffer_named(s->handler_name, s->chunk_size, s->erase TSRMLS_CC);
		}
		php_body_write(s->buffer, s->text_length TSRMLS_CC);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ int http_modified_match(char *, int) */
PHP_HTTP_API int _http_modified_match(const char *entry, const time_t t TSRMLS_DC)
{
	int retval;
	zval *zmodified;
	char *modified, *chr_ptr;

	HTTP_GSC(zmodified, entry, 0);

	modified = estrndup(Z_STRVAL_P(zmodified), Z_STRLEN_P(zmodified));
	if (chr_ptr = strrchr(modified, ';')) {
		chr_ptr = 0;
	}
	retval = (t <= http_parse_date(modified));
	efree(modified);
	return retval;
}
/* }}} */

/* {{{ int http_etag_match(char *, char *) */
PHP_HTTP_API int _http_etag_match(const char *entry, const char *etag TSRMLS_DC)
{
	zval *zetag;
	char *quoted_etag;
	STATUS result;

	HTTP_GSC(zetag, entry, 0);

	if (NULL != strchr(Z_STRVAL_P(zetag), '*')) {
		return 1;
	}

	quoted_etag = (char *) emalloc(strlen(etag) + 3);
	sprintf(quoted_etag, "\"%s\"", etag);

	if (!strchr(Z_STRVAL_P(zetag), ',')) {
		result = !strcmp(Z_STRVAL_P(zetag), quoted_etag);
	} else {
		result = (NULL != strstr(Z_STRVAL_P(zetag), quoted_etag));
	}
	efree(quoted_etag);
	return result;
}
/* }}} */

/* {{{ STATUS http_send_last_modified(int) */
PHP_HTTP_API STATUS _http_send_last_modified(const time_t t TSRMLS_DC)
{
	char modified[96] = "Last-Modified: ", *date;
	date = http_date(t);
	strcat(modified, date);
	efree(date);

	/* remember */
	HTTP_G(lmod) = t;

	return http_send_header(modified);
}
/* }}} */

/* {{{ static STATUS http_send_etag(char *, int) */
PHP_HTTP_API STATUS _http_send_etag(const char *etag,
	const int etag_len TSRMLS_DC)
{
	STATUS ret;
	int header_len;
	char *etag_header;

	header_len = sizeof("ETag: \"\"") + etag_len + 1;
	etag_header = ecalloc(header_len, 1);
	sprintf(etag_header, "ETag: \"%s\"", etag);
	ret = http_send_header(etag_header);
	efree(etag_header);

	if (!etag_len){
		php_error_docref(NULL TSRMLS_CC,E_ERROR,
			"Sending empty Etag (previous: %s)\n", HTTP_G(etag));
		return FAILURE;
	}
	/* remember */
	if (HTTP_G(etag)) {
		efree(HTTP_G(etag));
	}
	HTTP_G(etag) = estrdup(etag);

	return ret;
}
/* }}} */

/* {{{ char *http_absolute_uri(char *, char *) */
PHP_HTTP_API char *_http_absolute_uri(const char *url,
	const char *proto TSRMLS_DC)
{
	char URI[HTTP_URI_MAXLEN + 1], *PTR, *proto_ptr, *host, *path;
	zval *zhost;

	if (!url || !strlen(url)) {
		if (!SG(request_info).request_uri) {
			return NULL;
		}
		url = SG(request_info).request_uri;
	}
	/* Mess around with already absolute URIs */
	else if (proto_ptr = strstr(url, "://")) {
		if (!proto || !strncmp(url, proto, strlen(proto))) {
			return estrdup(url);
		} else {
			snprintf(URI, HTTP_URI_MAXLEN, "%s%s", proto, proto_ptr + 3);
			return estrdup(URI);
		}
	}

	/* protocol defaults to http */
	if (!proto || !strlen(proto)) {
		proto = "http";
	}

	/* get host name */
	if (	(zhost = http_get_server_var("HTTP_HOST")) ||
			(zhost = http_get_server_var("SERVER_NAME"))) {
		host = Z_STRVAL_P(zhost);
	} else {
		host = "localhost";
	}


	/* glue together */
	if (url[0] == '/') {
		snprintf(URI, HTTP_URI_MAXLEN, "%s://%s%s", proto, host, url);
	} else if (SG(request_info).request_uri) {
		path = estrdup(SG(request_info).request_uri);
		php_dirname(path, strlen(path));
		snprintf(URI, HTTP_URI_MAXLEN, "%s://%s%s/%s", proto, host, path, url);
		efree(path);
	} else {
		snprintf(URI, HTTP_URI_MAXLEN, "%s://%s/%s", proto, host, url);
	}

	/* strip everything after a new line */
	PTR = URI;
	while (*PTR != 0) {
		if (*PTR == '\n' || *PTR == '\r') {
			*PTR = 0;
			break;
		}
		PTR++;
	}

	return estrdup(URI);
}
/* }}} */

/* {{{ char *http_negotiate_q(char *, zval *, char *, hash_entry_type) */
PHP_HTTP_API char *_http_negotiate_q(const char *entry, const zval *supported,
	const char *def TSRMLS_DC)
{
	zval *zaccept, *zarray, *zdelim, **zentry, *zentries, **zsupp;
	char *q_ptr, *result;
	int i, c;
	double qual;

	HTTP_GSC(zaccept, entry, estrdup(def));

	MAKE_STD_ZVAL(zarray);
	array_init(zarray);

	MAKE_STD_ZVAL(zdelim);
	ZVAL_STRING(zdelim, ",", 0);
	php_explode(zdelim, zaccept, zarray, -1);
	efree(zdelim);

	MAKE_STD_ZVAL(zentries);
	array_init(zentries);

	c = zend_hash_num_elements(Z_ARRVAL_P(zarray));
	for (i = 0; i < c; i++, zend_hash_move_forward(Z_ARRVAL_P(zarray))) {

		if (SUCCESS != zend_hash_get_current_data(
				Z_ARRVAL_P(zarray), (void **) &zentry)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Cannot parse %s header: %s", entry, Z_STRVAL_P(zaccept));
			break;
		}

		/* check for qualifier */
		if (NULL != (q_ptr = strrchr(Z_STRVAL_PP(zentry), ';'))) {
			qual = strtod(q_ptr + 3, NULL);
		} else {
			qual = 1000.0 - i;
		}

		/* walk through the supported array */
		for (	zend_hash_internal_pointer_reset(Z_ARRVAL_P(supported));
				SUCCESS == zend_hash_get_current_data(
					Z_ARRVAL_P(supported), (void **) &zsupp);
				zend_hash_move_forward(Z_ARRVAL_P(supported))) {
			if (!strcasecmp(Z_STRVAL_PP(zsupp), Z_STRVAL_PP(zentry))) {
				add_assoc_double(zentries, Z_STRVAL_PP(zsupp), qual);
				break;
			}
		}
	}

	zval_dtor(zarray);
	efree(zarray);

	zend_hash_internal_pointer_reset(Z_ARRVAL_P(zentries));

	if (	(SUCCESS != zend_hash_sort(Z_ARRVAL_P(zentries), zend_qsort,
					http_sort_q, 0 TSRMLS_CC)) ||
			(HASH_KEY_NON_EXISTANT == zend_hash_get_current_key(
					Z_ARRVAL_P(zentries), &result, 0, 1))) {
		result = estrdup(def);
	}

	zval_dtor(zentries);
	efree(zentries);

	return result;
}
/* }}} */

/* {{{ http_range_status http_get_request_ranges(zval *zranges, size_t) */
PHP_HTTP_API http_range_status _http_get_request_ranges(zval *zranges,
	const size_t length TSRMLS_DC)
{
	zval *zrange;
	char *range, c;
	long begin = -1, end = -1, *ptr;

	HTTP_GSC(zrange, "HTTP_RANGE", RANGE_NO);
	range = Z_STRVAL_P(zrange);

	if (strncmp(range, "bytes=", strlen("bytes="))) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Range header misses bytes=");
		return RANGE_NO;
	}

	ptr = &begin;
	range += strlen("bytes=");

	do {
		switch (c = *(range++))
		{
			case '0':
				*ptr *= 10;
			break;

			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				/*
				 * If the value of the pointer is already set (non-negative)
				 * then multiply its value by ten and add the current value,
				 * else initialise the pointers value with the current value
				 * --
				 * This let us recognize empty fields when validating the
				 * ranges, i.e. a "-10" for begin and "12345" for the end
				 * was the following range request: "Range: bytes=0-12345";
				 * While a "-1" for begin and "12345" for the end would
				 * have been: "Range: bytes=-12345".
				 */
				if (*ptr > 0) {
					*ptr *= 10;
					*ptr += c - '0';
				} else {
					*ptr = c - '0';
				}
			break;

			case '-':
				ptr = &end;
			break;

			case ' ':
				/* IE - ignore for now */
			break;

			case 0:
			case ',':

				if (length) {
					/* validate ranges */
					switch (begin)
					{
						/* "0-12345" */
						case -10:
							if ((length - end) < 1) {
								return RANGE_ERR;
							}
							begin = 0;
						break;

						/* "-12345" */
						case -1:
							if ((length - end) < 1) {
								return RANGE_ERR;
							}
							begin = length - end;
							end = length;
						break;

						/* "12345-(xxx)" */
						default:
							switch (end)
							{
								/* "12345-" */
								case -1:
									if ((length - begin) < 1) {
										return RANGE_ERR;
									}
									end = length - 1;
								break;

								/* "12345-67890" */
								default:
									if (	((length - begin) < 1) ||
											((length - end) < 1) ||
											((begin - end) >= 0)) {
										return RANGE_ERR;
									}
								break;
							}
						break;
					}
				}
				{
					zval *zentry;
					MAKE_STD_ZVAL(zentry);
					array_init(zentry);
					add_index_long(zentry, 0, begin);
					add_index_long(zentry, 1, end);
					add_next_index_zval(zranges, zentry);

					begin = -1;
					end = -1;
					ptr = &begin;
				}
			break;

			default:
				return RANGE_NO;
			break;
		}
	} while (c != 0);

	return RANGE_OK;
}
/* }}} */

/* {{{ STATUS http_send_ranges(zval *, void *, size_t, http_send_mode) */
PHP_HTTP_API STATUS _http_send_ranges(zval *zranges, const void *data, const size_t size, const http_send_mode mode TSRMLS_DC)
{
	int c;
	long **begin, **end;
	zval **zrange;

	/* Send HTTP 206 Partial Content */
	http_send_status(206);

	/* single range */
	if ((c = zend_hash_num_elements(Z_ARRVAL_P(zranges))) == 1) {
		char range_header[256] = {0};

		zend_hash_index_find(Z_ARRVAL_P(zranges), 0, (void **) &zrange);
		zend_hash_index_find(Z_ARRVAL_PP(zrange), 0, (void **) &begin);
		zend_hash_index_find(Z_ARRVAL_PP(zrange), 1, (void **) &end);

		/* send content range header */
		snprintf(range_header, 255, "Content-Range: bytes %d-%d/%d", **begin, **end, size);
		http_send_header(range_header);

		/* send requested chunk */
		return http_send_chunk(data, **begin, **end + 1, mode);
	}

	/* multi range */
	else {
		int i;
		char bound[23] = {0}, preface[1024] = {0},
			multi_header[68] = "Content-Type: multipart/byteranges; boundary=";

		snprintf(bound, 22, "--%d%0.9f", time(NULL), php_combined_lcg(TSRMLS_C));
		strncat(multi_header, bound + 2, 21);
		http_send_header(multi_header);

		/* send each requested chunk */
		for (	i = 0,	zend_hash_internal_pointer_reset(Z_ARRVAL_P(zranges));
				i < c;
				i++,	zend_hash_move_forward(Z_ARRVAL_P(zranges))) {
			if (	HASH_KEY_NON_EXISTANT == zend_hash_get_current_data(
						Z_ARRVAL_P(zranges), (void **) &zrange) ||
					SUCCESS != zend_hash_index_find(
						Z_ARRVAL_PP(zrange), 0, (void **) &begin) ||
					SUCCESS != zend_hash_index_find(
						Z_ARRVAL_PP(zrange), 1, (void **) &end)) {
				break;
			}

			snprintf(preface, 1023,
				HTTP_CRLF "%s"
				HTTP_CRLF "Content-Type: %s"
				HTTP_CRLF "Content-Range: bytes %ld-%ld/%ld"
				HTTP_CRLF
				HTTP_CRLF,

				bound,
				HTTP_G(ctype) ? HTTP_G(ctype) : "application/x-octetstream",
				**begin,
				**end,
				size
			);

			php_body_write(preface, strlen(preface) TSRMLS_CC);
			http_send_chunk(data, **begin, **end + 1, mode);
		}

		/* write boundary once more */
		php_body_write(HTTP_CRLF, 2 TSRMLS_CC);
		php_body_write(bound, strlen(bound) TSRMLS_CC);

		return SUCCESS;
	}
}
/* }}} */

/* {{{ STATUS http_send(void *, sizezo_t, http_send_mode) */
PHP_HTTP_API STATUS _http_send(const void *data_ptr, const size_t data_size,
	const http_send_mode data_mode TSRMLS_DC)
{
	int is_range_request = http_is_range_request();

	if (!data_ptr) {
		return FAILURE;
	}

	/* etag handling */
	if (HTTP_G(etag_started)) {
		char *etag = ecalloc(33, 1);
		/* interrupt */
		HTTP_G(etag_started) = 0;
		/* never ever use the output to compute the ETag if http_send() is used */
		php_end_ob_buffer(0, 0 TSRMLS_CC);
		if (NULL == http_etag(&etag, data_ptr, data_size, data_mode)) {
			efree(etag);
			return FAILURE;
		}

		/* send 304 Not Modified if etag matches */
		if ((!is_range_request) && http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
			efree(etag);
			return http_send_status(304);
		}

		http_send_etag(etag, 32);
		efree(etag);
	}

	/* send 304 Not Modified if last-modified matches*/
    if ((!is_range_request) && http_modified_match("HTTP_IF_MODIFIED_SINCE", HTTP_G(lmod))) {
        return http_send_status(304);
    }

	if (is_range_request) {

		/* only send ranges if entity hasn't changed */
		if (
			((!zend_hash_exists(HTTP_SERVER_VARS, "HTTP_IF_MATCH", 13)) ||
			http_etag_match("HTTP_IF_MATCH", HTTP_G(etag)))
			&&
			((!zend_hash_exists(HTTP_SERVER_VARS, "HTTP_IF_UNMODIFIED_SINCE", 25)) ||
			http_modified_match("HTTP_IF_UNMODIFIED_SINCE", HTTP_G(lmod)))
		) {

			STATUS result = FAILURE;
			zval *zranges = NULL;
			MAKE_STD_ZVAL(zranges);
			array_init(zranges);

			switch (http_get_request_ranges(zranges, data_size))
			{
				case RANGE_NO:
					zval_dtor(zranges);
					efree(zranges);
					/* go ahead and send all */
				break;

				case RANGE_OK:
					result = http_send_ranges(zranges, data_ptr, data_size, data_mode);
					zval_dtor(zranges);
					efree(zranges);
					return result;
				break;

				case RANGE_ERR:
					zval_dtor(zranges);
					efree(zranges);
					http_send_status(416);
					return FAILURE;
				break;

				default:
					return FAILURE;
				break;
			}
		}
	}
	/* send all */
	return http_send_chunk(data_ptr, 0, data_size, data_mode);
}
/* }}} */

/* {{{ STATUS http_send_data(zval *) */
PHP_HTTP_API STATUS _http_send_data(const zval *zdata TSRMLS_DC)
{
	if (!Z_STRLEN_P(zdata)) {
		return SUCCESS;
	}
	if (!Z_STRVAL_P(zdata)) {
		return FAILURE;
	}

	return http_send(Z_STRVAL_P(zdata), Z_STRLEN_P(zdata), SEND_DATA);
}
/* }}} */

/* {{{ STATUS http_send_stream(php_stream *) */
PHP_HTTP_API STATUS _http_send_stream(const php_stream *file TSRMLS_DC)
{
	if (php_stream_stat((php_stream *) file, &HTTP_G(ssb))) {
		return FAILURE;
	}

	return http_send(file, HTTP_G(ssb).sb.st_size, SEND_RSRC);
}
/* }}} */

/* {{{ STATUS http_send_file(zval *) */
PHP_HTTP_API STATUS _http_send_file(const zval *zfile TSRMLS_DC)
{
	php_stream *file;
	STATUS ret;

	if (!Z_STRLEN_P(zfile)) {
		return FAILURE;
	}

	if (!(file = php_stream_open_wrapper(Z_STRVAL_P(zfile), "rb",
			REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL))) {
		return FAILURE;
	}

	ret = http_send_stream(file);
	php_stream_close(file);
	return ret;
}
/* }}} */

/* {{{ proto STATUS http_chunked_decode(char *, size_t, char **, size_t *) */
PHP_HTTP_API STATUS _http_chunked_decode(const char *encoded,
	const size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	const char *e_ptr;
	char *d_ptr;

	*decoded_len = 0;
	*decoded = (char *) ecalloc(encoded_len, 1);
	d_ptr = *decoded;
	e_ptr = encoded;

	while (((e_ptr - encoded) - encoded_len) > 0) {
		char hex_len[9] = {0};
		size_t chunk_len = 0;
		int i = 0;

		/* read in chunk size */
		while (isxdigit(*e_ptr)) {
			if (i == 9) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Chunk size is too long: 0x%s...", hex_len);
				efree(*decoded);
				return FAILURE;
			}
			hex_len[i++] = *e_ptr++;
		}

		/* reached the end */
		if (!strcmp(hex_len, "0")) {
			break;
		}

		/* new line */
		if (strncmp(e_ptr, HTTP_CRLF, 2)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Invalid character (expected 0x0D 0x0A; got: %x %x)",
				*e_ptr, *(e_ptr + 1));
			efree(*decoded);
			return FAILURE;
		}

		/* hex to long */
		{
			char *error = NULL;
			chunk_len = strtol(hex_len, &error, 16);
			if (error == hex_len) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Invalid chunk size string: '%s'", hex_len);
				efree(*decoded);
				return FAILURE;
			}
		}

		memcpy(d_ptr, e_ptr += 2, chunk_len);
		d_ptr += chunk_len;
		e_ptr += chunk_len + 2;
		*decoded_len += chunk_len;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ proto STATUS http_split_response(zval *, zval *, zval *) */
PHP_HTTP_API STATUS _http_split_response(const zval *zresponse, zval *zheaders,
	zval *zbody TSRMLS_DC)
{
	char *header, *response, *body = NULL;
	long response_len = Z_STRLEN_P(zresponse);
	header = response = Z_STRVAL_P(zresponse);

	while ((response - Z_STRVAL_P(zresponse) + 3) < response_len) {
		if (	(*response++ == '\r') &&
				(*response++ == '\n') &&
				(*response++ == '\r') &&
				(*response++ == '\n')) {
			body = response;
			break;
		}
	}

	if (body && (response_len - (body - header))) {
		ZVAL_STRINGL(zbody, body, response_len - (body - header) - 1, 1);
	} else {
		Z_TYPE_P(zbody) = IS_NULL;
	}

	return http_parse_headers(header, body - Z_STRVAL_P(zresponse), zheaders);
}
/* }}} */

/* {{{ STATUS http_parse_headers(char *, long, zval *) */
PHP_HTTP_API STATUS _http_parse_headers(char *header, int header_len, zval *array TSRMLS_DC)
{
	char *colon = NULL, *line = NULL, *begin = header;

	if (header_len < 2) {
		return FAILURE;
	}

	/* status code */
	if (!strncmp(header, "HTTP/1.", 7)) {
		char *end = strstr(header, HTTP_CRLF);
		add_assoc_stringl(array, "Status",
			header + strlen("HTTP/1.x "),
			end - (header + strlen("HTTP/1.x ")), 1);
		header = end + 2;
	}

	line = header;

	while (header_len >= (line - begin)) {
		int value_len = 0;

		switch (*line++)
		{
			case 0:
				--value_len; /* we don't have CR so value length is one char less */
			case '\n':
				if (colon && ((!(*line - 1)) || ((*line != ' ') && (*line != '\t')))) {

					/* skip empty key */
					if (header != colon) {
						char *key = estrndup(header, colon - header);
						value_len += line - colon - 1;

						/* skip leading ws */
						while (isspace(*(++colon))) --value_len;
						/* skip trailing ws */
						while (isspace(colon[value_len - 1])) --value_len;

						if (value_len < 1) {
							/* hm, empty header? */
							add_assoc_stringl(array, key, "", 0, 1);
						} else {
							add_assoc_stringl(array, key, colon, value_len, 1);
						}
						efree(key);
					}

					colon = NULL;
					value_len = 0;
					header += line - header;
				}
			break;

			case ':':
				if (!colon) {
					colon = line - 1;
				}
			break;
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ void http_get_request_headers(zval *) */
PHP_HTTP_API void _http_get_request_headers(zval *array TSRMLS_DC)
{
    char *key;

    for (   zend_hash_internal_pointer_reset(HTTP_SERVER_VARS);
            zend_hash_get_current_key(HTTP_SERVER_VARS, &key, NULL, 0) != HASH_KEY_NON_EXISTANT;
            zend_hash_move_forward(HTTP_SERVER_VARS)) {
        if (!strncmp(key, "HTTP_", 5)) {
            zval **header;
            zend_hash_get_current_data(HTTP_SERVER_VARS, (void **) &header);
            add_assoc_stringl(array, pretty_key(key + 5, strlen(key) - 5, 1, 1), Z_STRVAL_PP(header), Z_STRLEN_PP(header), 1);
        }
    }
}
/* }}} */

/* {{{ HAVE_CURL */
#ifdef HTTP_HAVE_CURL

/* {{{ STATUS http_get(char *, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_get(const char *URL, HashTable *options,
	HashTable *info, char **data, size_t *data_len TSRMLS_DC)
{
	CURL *ch = curl_easy_init();

	if (!ch) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initialize curl");
		return FAILURE;
	}

	http_curl_initbuf(CURLBUF_EVRY);
	http_curl_setopts(ch, URL, options);

	if (CURLE_OK != curl_easy_perform(ch)) {
		curl_easy_cleanup(ch);
		http_curl_freebuf(CURLBUF_EVRY);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request");
		return FAILURE;
	}
	if (info) {
		http_curl_getinfo(ch, info);
	}
	curl_easy_cleanup(ch);

	http_curl_movebuf(CURLBUF_EVRY, data, data_len);

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_head(char *, HashTable *, HashTable *, char **data, size_t *) */
PHP_HTTP_API STATUS _http_head(const char *URL, HashTable *options,
	HashTable *info, char **data, size_t *data_len TSRMLS_DC)
{
	CURL *ch = curl_easy_init();

	if (!ch) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initialize curl");
		return FAILURE;
	}

	http_curl_initbuf(CURLBUF_HDRS);
	http_curl_setopts(ch, URL, options);
	curl_easy_setopt(ch, CURLOPT_NOBODY, 1);

	if (CURLE_OK != curl_easy_perform(ch)) {
		curl_easy_cleanup(ch);
		http_curl_freebuf(CURLBUF_HDRS);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request");
		return FAILURE;
	}
	if (info) {
		http_curl_getinfo(ch, info);
	}
	curl_easy_cleanup(ch);

	http_curl_movebuf(CURLBUF_HDRS, data, data_len);

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_post_data(char *, char *, size_t, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_post_data(const char *URL, char *postdata,
	size_t postdata_len, HashTable *options, HashTable *info, char **data,
	size_t *data_len TSRMLS_DC)
{
	CURL *ch = curl_easy_init();

	if (!ch) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initialize curl");
		return FAILURE;
	}

	http_curl_initbuf(CURLBUF_EVRY);
	http_curl_setopts(ch, URL, options);
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, postdata);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, postdata_len);

	if (CURLE_OK != curl_easy_perform(ch)) {
		curl_easy_cleanup(ch);
		http_curl_freebuf(CURLBUF_EVRY);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request");
		return FAILURE;
	}
	if (info) {
		http_curl_getinfo(ch, info);
	}
	curl_easy_cleanup(ch);

	http_curl_movebuf(CURLBUF_EVRY, data, data_len);

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_post_array(char *, HashTable *, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_post_array(const char *URL, HashTable *postarray,
	HashTable *options, HashTable *info, char **data, size_t *data_len TSRMLS_DC)
{
	smart_str qstr = {0};
	STATUS status;

	if (php_url_encode_hash_ex(postarray, &qstr, NULL,0,NULL,0,NULL,0,NULL TSRMLS_CC) != SUCCESS) {
		if (qstr.c) {
			efree(qstr.c);
		}
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not encode post data");
		return FAILURE;
	}
	smart_str_0(&qstr);

	status = http_post_data(URL, qstr.c, qstr.len, options, info, data, data_len);
	if (qstr.c) {
		efree(qstr.c);
	}
	return status;
}
/* }}} */

#endif
/* }}} HAVE_CURL */

/* {{{ STATUS http_auth_header(char *, char*) */
PHP_HTTP_API STATUS _http_auth_header(const char *type, const char *realm TSRMLS_DC)
{
	char realm_header[1024] = {0};
	snprintf(realm_header, 1023, "WWW-Authenticate: %s realm=\"%s\"", type, realm);
	return http_send_status_header(401, realm_header);
}
/* }}} */

/* {{{ STATUS http_auth_credentials(char **, char **) */
PHP_HTTP_API STATUS _http_auth_credentials(char **user, char **pass TSRMLS_DC)
{
	if (strncmp(sapi_module.name, "isapi", 5)) {
		zval *zuser, *zpass;

		HTTP_GSC(zuser, "PHP_AUTH_USER", FAILURE);
		HTTP_GSC(zpass, "PHP_AUTH_PW", FAILURE);

		*user = estrndup(Z_STRVAL_P(zuser), Z_STRLEN_P(zuser));
		*pass = estrndup(Z_STRVAL_P(zpass), Z_STRLEN_P(zpass));

		return SUCCESS;
	} else {
		zval *zauth = NULL;
		HTTP_GSC(zauth, "HTTP_AUTHORIZATION", FAILURE);
		{
			char *decoded, *colon;
			int decoded_len;
			decoded = php_base64_decode(Z_STRVAL_P(zauth), Z_STRLEN_P(zauth),
				&decoded_len);

			if (colon = strchr(decoded + 6, ':')) {
				*user = estrndup(decoded + 6, colon - decoded - 6);
				*pass = estrndup(colon + 1, decoded + decoded_len - colon - 6 - 1);

				return SUCCESS;
			} else {
				return FAILURE;
			}
		}
	}
}
/* }}} */

/* }}} public API */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
