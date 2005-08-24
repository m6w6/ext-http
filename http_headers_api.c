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
#include "php.h"

#include "ext/standard/php_string.h"
#include "ext/standard/url.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_headers_api.h"
#include "php_http_info_api.h"

#include <ctype.h>

ZEND_EXTERN_MODULE_GLOBALS(http);

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
		/* TODO: support primaries only, too */
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
		return RANGE_NO;
	}

	ptr = &begin;
	range += sizeof("bytes=") - 1;

	do {
		switch (c = *(range++))
		{
			case '0':
				/* allow 000... - shall we? */
				if (*ptr != -10) {
					*ptr *= 10;
				}
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
							if (end == -10 || length <= (size_t) end) {
								return RANGE_ERR;
							}
							begin = 0;
						break;

						/* "-12345" */
						case -1:
							/* "-", "-0" or overflow */
							if (end == -1 || end == -10 || length <= (size_t) end) {
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
									if (length <= (size_t) begin) {
										return RANGE_ERR;
									}
									end = length - 1;
								break;

								/* "12345-67890" */
								default:
									if (	(length <= (size_t) begin) ||
											(length <= (size_t) end)   ||
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

/* {{{ STATUS http_parse_headers(char *, HashTable *, zend_bool) */
PHP_HTTP_API STATUS _http_parse_headers_ex(const char *header, HashTable *headers, zend_bool prettify, 
	http_info_callback callback_func, void **callback_data TSRMLS_DC)
{
	const char *colon = NULL, *line = NULL, *begin = header;
	const char *body = http_locate_body(header);
	size_t header_len;
	zval array;

	Z_ARRVAL(array) = headers;
	header_len = body ? body - header : strlen(header) + 1;
	line = header;

	while (header_len >= (size_t) (line - begin)) {
		int value_len = 0;

		switch (*line++)
		{
			case ':':
				if (!colon) {
					colon = line - 1;
				}
			break;
			
			case 0:
				--value_len; /* we don't have CR so value length is one char less */
			case '\n':
				if ((!(*line - 1)) || ((*line != ' ') && (*line != '\t'))) {
					http_info i;
					
					/* response/request line */
					if (SUCCESS == http_info_parse(header, &i)) {
						callback_func(callback_data, &headers, &i TSRMLS_CC);
						http_info_dtor(&i);
						Z_ARRVAL(array) = headers;
					} else
					
					/* "header: value" pair */
					if (colon) {

						/* skip empty key */
						if (header != colon) {
							zval **previous = NULL;
							char *value;
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
								value = estrdup("");
								value_len = 0;
							}

							/* if we already have got such a header make an array of those */
							if (SUCCESS == zend_hash_find(headers, key, keylen + 1, (void **) &previous)) {
								/* convert to array */
								if (Z_TYPE_PP(previous) != IS_ARRAY) {
									convert_to_array(*previous);
								}
								add_next_index_stringl(*previous, value, value_len, 0);
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
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ void http_get_request_headers_ex(HashTable *, zend_bool) */
PHP_HTTP_API void _http_get_request_headers_ex(HashTable *headers, zend_bool prettify TSRMLS_DC)
{
	char *key = NULL;
	ulong idx = 0;
	zval array, **hsv;

	Z_ARRVAL(array) = headers;

	if (SUCCESS == zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &hsv)) {
		FOREACH_KEY(*hsv, key, idx) {
			if (key && !strncmp(key, "HTTP_", 5)) {
				zval **header;
	
				key += 5;
				if (prettify) {
					key = pretty_key(key, strlen(key), 1, 1);
				}
	
				zend_hash_get_current_data(Z_ARRVAL_PP(hsv), (void **) &header);
				add_assoc_stringl(&array, key, Z_STRVAL_PP(header), Z_STRLEN_PP(header), 1);
				key = NULL;
			}
		}
	}
}
/* }}} */

/* {{{ zend_bool http_match_request_header(char *, char *) */
PHP_HTTP_API zend_bool _http_match_request_header_ex(const char *header, const char *value, zend_bool match_case TSRMLS_DC)
{
	char *name, *key = NULL;
	ulong idx;
	zend_bool result = 0;
	HashTable headers;

	name = pretty_key(estrdup(header), strlen(header), 1, 1);
	zend_hash_init(&headers, 0, NULL, ZVAL_PTR_DTOR, 0);
	http_get_request_headers_ex(&headers, 1);

	FOREACH_HASH_KEY(&headers, key, idx) {
		if (key && (!strcmp(key, name))) {
			zval **data;

			if (SUCCESS == zend_hash_get_current_data(&headers, (void **) &data)) {
				result = (match_case ? strcmp(Z_STRVAL_PP(data), value) : strcasecmp(Z_STRVAL_PP(data), value)) ? 0 : 1;
			}
			break;
		}
	}

	zend_hash_destroy(&headers);
	efree(name);

	return result;
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

