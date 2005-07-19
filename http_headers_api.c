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

#include <ctype.h>

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
		http_error(E_NOTICE, HTTP_E_HEADER, "Range header misses bytes=");
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

/* {{{ STATUS http_parse_headers(char *, HashTable *, zend_bool) */
PHP_HTTP_API STATUS _http_parse_headers_ex(const char *header, HashTable *headers, zend_bool prettify, http_parse_headers_callback_t func, void **callback_data TSRMLS_DC)
{
	const char *colon = NULL, *line = NULL, *begin = header, *crlfcrlf = NULL;
	size_t header_len;
	zval array;

	Z_ARRVAL(array) = headers;

	if (crlfcrlf = strstr(header, HTTP_CRLF HTTP_CRLF)) {
		header_len = crlfcrlf - header + lenof(HTTP_CRLF);
	} else {
		header_len = strlen(header) + 1;
	}


	if (header_len < 2 || !strchr(header, ':')) {
		http_error(E_WARNING, HTTP_E_PARSE, "Cannot parse too short or malformed HTTP headers");
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
                    if (    (!strncmp(header, "HTTP/1.", lenof("HTTP/1."))) ||
                            (!strncmp(line - lenof("HTTP/1.x" HTTP_CRLF) + value_len, "HTTP/1.", lenof("HTTP/1.")))) {
						if (func) {
							func(header, &headers, callback_data TSRMLS_CC);
							Z_ARRVAL(array) = headers;
						}
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

PHP_HTTP_API void _http_parse_headers_default_callback(const char *http_line, HashTable **headers, void **cb_data TSRMLS_DC)
{
	zval array;
	char *crlf = NULL;
	size_t line_length;
	Z_ARRVAL(array) = *headers;

	if (crlf = strstr(http_line, HTTP_CRLF)) {
		line_length = crlf - http_line;
	} else {
		line_length = strlen(http_line);
	}

	/* response */
	if (!strncmp(http_line, "HTTP/1.", lenof("HTTP/1."))) {
		char *status = estrndup(http_line + lenof("HTTP/1.x "), line_length - lenof("HTTP/1.x "));
		add_assoc_stringl(&array, "Response Status", status, line_length - lenof("HTTP/1.x "), 0);
	} else
	/* request */
	if (!strncmp(http_line + line_length - lenof("HTTP/1.x"), "HTTP/1.", lenof("HTTP/1."))) {
		char *sep = strchr(http_line, ' ');
		char *url = estrndup(sep + 1, strstr(sep, "HTTP/1.") - sep + 1 + 1);
		char *met = estrndup(http_line, sep - http_line);

		add_assoc_stringl(&array, "Request Method", met, sep - http_line, 0);
		add_assoc_stringl(&array, "Request Uri", url, strstr(sep, "HTTP/1.") - sep + 1 + 1, 0);
	}
}

/* {{{ void http_get_request_headers_ex(HashTable *, zend_bool) */
PHP_HTTP_API void _http_get_request_headers_ex(HashTable *headers, zend_bool prettify TSRMLS_DC)
{
	char *key = NULL;
	ulong idx = 0;
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

