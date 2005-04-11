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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <ctype.h>

#include "php.h"
#include "ext/standard/php_string.h"
#include "ext/standard/url.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_headers_api.h"

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

/* {{{ char *http_negotiate_q(char *, HashTable *, char *) */
PHP_HTTP_API char *_http_negotiate_q(const char *entry, const HashTable *supported,	const char *def TSRMLS_DC)
{
	zval *zaccept, zdelim, zarray, zentries, **zentry, **zsupp;
	char *q_ptr = NULL, *key = NULL;
	int i = 0;
	ulong idx = 0;
	double qual;

	HTTP_GSC(zaccept, entry, estrdup(def));

	array_init(&zarray);
	array_init(&zentries);

	Z_STRVAL(zdelim) = ",";
	Z_STRLEN(zdelim) = 1;

	php_explode(&zdelim, zaccept, &zarray, -1);

	FOREACH_HASH_VAL(Z_ARRVAL(zarray), zentry) {
		if (q_ptr = strrchr(Z_STRVAL_PP(zentry), ';')) {
			qual = strtod(q_ptr + 3, NULL);
			*q_ptr = 0;
			q_ptr = NULL;
		} else {
			qual = 1000.0 - i++;
		}
		FOREACH_HASH_VAL((HashTable *)supported, zsupp) {
			if (!strcasecmp(Z_STRVAL_PP(zsupp), Z_STRVAL_PP(zentry))) {
				add_assoc_double(&zentries, Z_STRVAL_PP(zsupp), qual);
				break;
			}
		}
	}
	zval_dtor(&zarray);

	zend_hash_sort(Z_ARRVAL(zentries), zend_qsort, http_sort_q, 0 TSRMLS_CC);

	FOREACH_HASH_KEY(Z_ARRVAL(zentries), key, idx) {
		if (key) {
			key = estrdup(key);
			zval_dtor(&zentries);
			return key;
		}
	}
	zval_dtor(&zentries);

	return estrdup(def);
}
/* }}} */

/* {{{ http_range_status http_get_request_ranges(HashTable *ranges, size_t) */
PHP_HTTP_API http_range_status _http_get_request_ranges(HashTable *ranges, size_t length TSRMLS_DC)
{
	zval *zrange;
	char *range, c;
	long begin = -1, end = -1, *ptr;

	HTTP_GSC(zrange, "HTTP_RANGE", RANGE_NO);
	range = Z_STRVAL_P(zrange);

	if (strncmp(range, "bytes=", sizeof("bytes=") - 1)) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Range header misses bytes=");
		return RANGE_NO;
	}

	ptr = &begin;
	range += sizeof("bytes=") - 1;

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
							/* "0-" */
							if (end == -1) {
								return RANGE_NO;
							}
							/* "0-0" or overflow */
							if (end == -10 || length <= end) {
								return RANGE_ERR;
							}
							begin = 0;
						break;

						/* "-12345" */
						case -1:
							/* "-", "-0" or overflow */
							if (end == -1 || end == -10 || length <= end) {
								return RANGE_ERR;
							}
							begin = length - end;
							end = length - 1;
						break;

						/* "12345-(xxx)" */
						default:
							switch (end)
							{
								/* "12345-0" */
								case -10:
									return RANGE_ERR;
								break;

								/* "12345-" */
								case -1:
									if (length <= begin) {
										return RANGE_ERR;
									}
									end = length - 1;
								break;

								/* "12345-67890" */
								default:
									if (	(length <= begin) ||
											(length <= end)   ||
											(end    <  begin)) {
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
					zend_hash_next_index_insert(ranges, &zentry, sizeof(zval *), NULL);

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

/* {{{ STATUS http_parse_headers(char *, size_t, HashTable *, zend_bool) */
PHP_HTTP_API STATUS _http_parse_headers_ex(char *header, size_t header_len,
	HashTable *headers, zend_bool prettify,
	http_parse_headers_callback_t func, void **callback_data TSRMLS_DC)
{
	char *colon = NULL, *line = NULL, *begin = header;
	zval array;

	Z_ARRVAL(array) = headers;

	if (header_len < 2) {
		return FAILURE;
	}

	line = header;

	while (header_len >= (line - begin)) {
		int value_len = 0;

		switch (*line++)
		{
			case 0:
				--value_len; /* we don't have CR so value length is one char less */
			case '\n':
				if ((!(*line - 1)) || ((*line != ' ') && (*line != '\t'))) {
					/* response/request line */
					if (	(!strncmp(header, "HTTP/1.", lenof("HTTP/1."))) ||
							(!strncmp(line - lenof("HTTP/1.x\r") + value_len, "HTTP/1.", lenof("HTTP/1.")))) {
						if (func) {
							func(callback_data, header, header - (line + 1), &headers TSRMLS_CC);
							Z_ARRVAL(array) = headers;
						}
					} else
				
					/* "header: value" pair */
					if (colon) {

						/* skip empty key */
						if (header != colon) {
							zval **previous = NULL;
							char *value = empty_string;
							int keylen = colon - header;
							char *key = estrndup(header, keylen);

							if (prettify) {
								key = pretty_key(key, keylen, 1, 1);
							}

							value_len += line - colon - 1;

							/* skip leading ws */
							while (isspace(*(++colon))) --value_len;
							/* skip trailing ws */
							while (isspace(colon[value_len - 1])) --value_len;

							if (value_len > 0) {
								value = estrndup(colon, value_len);
							} else {
								value_len = 0;
							}

							/* if we already have got such a header make an array of those */
							if (SUCCESS == zend_hash_find(headers, key, keylen + 1, (void **) &previous)) {
								/* already an array? - just add */
								if (Z_TYPE_PP(previous) == IS_ARRAY) {
										add_next_index_stringl(*previous, value, value_len, 0);
								} else {
									/* create the array */
									zval *new_array;
									MAKE_STD_ZVAL(new_array);
									array_init(new_array);

									add_next_index_stringl(new_array, Z_STRVAL_PP(previous), Z_STRLEN_PP(previous), 1);
									add_next_index_stringl(new_array, value, value_len, 0);
									add_assoc_zval(&array, key, new_array);
								}

								previous = NULL;
							} else {
								add_assoc_stringl(&array, key, value, value_len, 0);
							}
							efree(key);
						}
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

PHP_HTTP_API void _http_parse_headers_default_callback(void **cb_data, char *http_line, size_t line_length, HashTable **headers TSRMLS_DC)
{
	zval array;
	Z_ARRVAL(array) = *headers;
	
	/* response */
	if (!strncmp(http_line, "HTTP/1.", lenof("HTTP/1."))) {
		add_assoc_stringl(&array, "Response Status", http_line + lenof("HTTP/1.x "), line_length - lenof("HTTP/1.x \r\n"), 0);
	} else
	/* request */
	if (!strncmp(http_line + line_length - lenof("HTTP/1.x\r\n"), "HTTP/1.", lenof("HTTP/1."))) {
		char *sep = strchr(http_line, ' ');
		
		add_assoc_stringl(&array, "Request Method", http_line, sep - http_line, 1);
		add_assoc_stringl(&array, "Request Uri", sep + 1, strstr(sep, "HTTP/1.") - sep + 1 + 1, 1);
	}
}

/* {{{ */
PHP_HTTP_API STATUS _http_parse_cookie(const char *cookie, HashTable *values TSRMLS_DC)
{
	const char *key = cookie, *val = NULL;
	int vallen = 0, keylen = 0, done = 0;
	zval array;

	Z_ARRVAL(array) = values;

	if (!(val = strchr(cookie, '='))) {
		return FAILURE;
	}

#define HTTP_COOKIE_VAL(array, k, str, len) \
	{ \
		const char *encoded = str; \
		char *decoded = NULL; \
		int decoded_len = 0, encoded_len = len; \
		decoded = estrndup(encoded, encoded_len); \
		decoded_len = php_url_decode(decoded, encoded_len); \
		add_assoc_stringl(array, k, decoded, decoded_len, 0); \
	}
#define HTTP_COOKIE_FIXKEY() \
	{ \
			while (isspace(*key)) ++key; \
			keylen = val - key; \
			while (isspace(key[keylen - 1])) --keylen; \
	}
#define HTTP_COOKIE_FIXVAL() \
	{ \
			++val; \
			while (isspace(*val)) ++val; \
			vallen = key - val; \
			while (isspace(val[vallen - 1])) --vallen; \
	}

	HTTP_COOKIE_FIXKEY();
	HTTP_COOKIE_VAL(&array, "name", key, keylen);

	/* just a name=value cookie */
	if (!(key = strchr(val, ';'))) {
		key = val + strlen(val);
		HTTP_COOKIE_FIXVAL();
		HTTP_COOKIE_VAL(&array, "value", val, vallen);
	}
	/* additional info appended */
	else {
		char *keydup = NULL;

		HTTP_COOKIE_FIXVAL();
		HTTP_COOKIE_VAL(&array, "value", val, vallen);

		do {
			if (!(val = strchr(key, '='))) {
				break;
			}
			++key;
			HTTP_COOKIE_FIXKEY();
			keydup = estrndup(key, keylen);
			if (!(key = strchr(val, ';'))) {
				done = 1;
				key = val + strlen(val);
			}
			HTTP_COOKIE_FIXVAL();
			HTTP_COOKIE_VAL(&array, keydup, val, vallen);
			efree(keydup);
		} while (!done);
	}
	return SUCCESS;
}
/* }}} */

/* {{{ void http_get_request_headers_ex(HashTable *, zend_bool) */
PHP_HTTP_API void _http_get_request_headers_ex(HashTable *headers, zend_bool prettify TSRMLS_DC)
{
	char *key = NULL;
	long idx = 0;
	zval array;

	Z_ARRVAL(array) = headers;

	FOREACH_HASH_KEY(HTTP_SERVER_VARS, key, idx) {
		if (key && !strncmp(key, "HTTP_", 5)) {
			zval **header;

			key += 5;
			if (prettify) {
				key = pretty_key(key, strlen(key), 1, 1);
			}

			zend_hash_get_current_data(HTTP_SERVER_VARS, (void **) &header);
			add_assoc_stringl(&array, key, Z_STRVAL_PP(header), Z_STRLEN_PP(header), 1);
			key = NULL;
		}
	}
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

