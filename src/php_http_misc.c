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

#include "ext/standard/php_lcg.h"
#include "zend_exceptions.h"

/* SLEEP */

void php_http_sleep(double s)
{
#if PHP_WIN32
	Sleep((DWORD) PHP_HTTP_MSEC(s));
#elif HAVE_USLEEP
	usleep(PHP_HTTP_USEC(s));
#elif HAVE_NANOSLEEP
	struct timespec req, rem;

	req.tv_sec = (time_t) s;
	req.tv_nsec = PHP_HTTP_NSEC(s) % PHP_HTTP_NANOSEC;

	while (nanosleep(&req, &rem) && (errno == EINTR) && (PHP_HTTP_NSEC(rem.tv_sec) + rem.tv_nsec) > PHP_HTTP_NSEC(PHP_HTTP_DIFFSEC))) {
		req.tv_sec = rem.tv_sec;
		req.tv_nsec = rem.tv_nsec;
	}
#else
	struct timeval timeout;

	timeout.tv_sec = (time_t) s;
	timeout.tv_usec = PHP_HTTP_USEC(s) % PHP_HTTP_MCROSEC;

	select(0, NULL, NULL, NULL, &timeout);
#endif
}


/* STRING UTILITIES */

int php_http_match(const char *haystack_str, const char *needle_str, int flags)
{
	int result = 0;

	if (UNEXPECTED(!haystack_str || !needle_str)) {
		return result;
	}

	if (UNEXPECTED(flags & PHP_HTTP_MATCH_FULL)) {
		if (flags & PHP_HTTP_MATCH_CASE) {
			result = !strcmp(haystack_str, needle_str);
		} else {
			result = !strcasecmp(haystack_str, needle_str);
		}
	} else {
		const char *found;
		char *haystack = estrdup(haystack_str), *needle = estrdup(needle_str);

		if (UNEXPECTED(flags & PHP_HTTP_MATCH_CASE)) {
			found = zend_memnstr(haystack, needle, strlen(needle), haystack+strlen(haystack));
		} else {
			found = php_stristr(haystack, needle, strlen(haystack), strlen(needle));
		}

		if (found) {
			if (!(flags & PHP_HTTP_MATCH_WORD)
			||	(	(found == haystack || !PHP_HTTP_IS_CTYPE(alnum, *(found - 1)))
				&&	EXPECTED(!*(found + strlen(needle)) || !PHP_HTTP_IS_CTYPE(alnum, *(found + strlen(needle))))
				)
			) {
				result = 1;
			}
		}

		PTR_FREE(haystack);
		PTR_FREE(needle);
	}

	return result;
}

char *php_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen)
{
	size_t i;
	int wasalpha;

	if (key && key_len) {
		if (EXPECTED(wasalpha = PHP_HTTP_IS_CTYPE(alpha, key[0]))) {
			if (EXPECTED(uctitle)) {
				key[0] = PHP_HTTP_TO_CTYPE(upper, key[0]);
			} else {
				key[0] = PHP_HTTP_TO_CTYPE(lower, key[0]);
			}
		}
		for (i = 1; i < key_len; ++i) {
			if (EXPECTED(PHP_HTTP_IS_CTYPE(alpha, key[i]))) {
				if (EXPECTED(wasalpha)) {
					key[i] = PHP_HTTP_TO_CTYPE(lower, key[i]);
				} else if (EXPECTED(uctitle)) {
					key[i] = PHP_HTTP_TO_CTYPE(upper, key[i]);
					wasalpha = 1;
				} else {
					key[i] = PHP_HTTP_TO_CTYPE(lower, key[i]);
					wasalpha = 1;
				}
			} else {
				if (EXPECTED(xhyphen && (key[i] == '_'))) {
					key[i] = '-';
				}
				wasalpha = 0;
			}
		}
	}
	return key;
}


size_t php_http_boundary(char *buf, size_t buf_len)
{
	return snprintf(buf, buf_len, "%15.15F", sapi_get_request_time() * php_combined_lcg());
}

int php_http_select_str(const char *cmp, int argc, ...)
{
	va_list argv;
	int match = -1;

	if (cmp && argc > 0) {
		int i;

		va_start(argv, argc);
		for (i = 0; i < argc; ++i) {
			const char *test = va_arg(argv, const char *);

			if (!strcasecmp(cmp, test)) {
				match = i;
				break;
			}
		}
		va_end(argv);
	}

	return match;
}


/* ARRAYS */

unsigned php_http_array_list(HashTable *ht, unsigned argc, ...)
{
	unsigned argl = 0;
	va_list argv;
	zval *data;

	va_start(argv, argc);
	ZEND_HASH_FOREACH_VAL(ht, data) {
		zval **argp = (zval **) va_arg(argv, zval **);
		*argp = data;
		++argl;
	} ZEND_HASH_FOREACH_END();
	va_end(argv);

	return argl;
}

void php_http_array_copy_strings(zval *zp)
{
	Z_TRY_ADDREF_P(zp);
	convert_to_string_ex(zp);
}

int php_http_array_apply_append_func(zval *value, int num_args, va_list args, zend_hash_key *hash_key)
{
	int flags;
	char *key = NULL;
	HashTable *dst;
	zval *data = NULL;

	dst = va_arg(args, HashTable *);
	flags = va_arg(args, int);

	if ((!(flags & ARRAY_JOIN_STRONLY)) || hash_key->key) {
		if ((flags & ARRAY_JOIN_PRETTIFY) && hash_key->key) {
			key = php_http_pretty_key(estrndup(hash_key->key->val, hash_key->key->len), hash_key->key->len, 1, 1);
			data = zend_hash_str_find(dst, key, hash_key->key->len);
		} else if (hash_key->key) {
			data = zend_hash_find(dst, hash_key->key);
		} else {
			data = zend_hash_index_find(dst, hash_key->h);
		}

		if (flags & ARRAY_JOIN_STRINGIFY) {
			convert_to_string_ex(value);
		}
		Z_TRY_ADDREF_P(value);

		if (data) {
			if (Z_TYPE_P(data) != IS_ARRAY) {
				convert_to_array(data);
			}
			add_next_index_zval(data, value);
		} else if (key) {
			zend_hash_str_update(dst, key, hash_key->key->len, value);
		} else if (hash_key->key) {
			zend_hash_update(dst, hash_key->key, value);
		} else {
			zend_hash_index_update(dst, hash_key->h, value);
		}

		if (key) {
			efree(key);
		}
	}

	return ZEND_HASH_APPLY_KEEP;
}

int php_http_array_apply_merge_func(zval *value, int num_args, va_list args, zend_hash_key *hash_key)
{
	int flags;
	char *key = NULL;
	HashTable *dst;

	dst = va_arg(args, HashTable *);
	flags = va_arg(args, int);

	if ((!(flags & ARRAY_JOIN_STRONLY)) || hash_key->key) {
		if (flags & ARRAY_JOIN_STRINGIFY) {
			convert_to_string_ex(value);
		}

		Z_TRY_ADDREF_P(value);

		if ((flags & ARRAY_JOIN_PRETTIFY) && hash_key->key) {
			key = php_http_pretty_key(estrndup(hash_key->key->val, hash_key->key->len), hash_key->key->len, 1, 1);
			zend_hash_str_update(dst, key, hash_key->key->len, value);
			efree(key);
		} else if (hash_key->key) {
			zend_hash_update(dst, hash_key->key, value);
		} else {
			zend_hash_index_update(dst, hash_key->h, value);
		}
	}

	return ZEND_HASH_APPLY_KEEP;
}

/* PASS CALLBACK */

size_t php_http_pass_fcall_callback(void *cb_arg, const char *str, size_t len)
{
	php_http_pass_fcall_arg_t *fcd = cb_arg;
	zval zdata;

	ZVAL_STRINGL(&zdata, str, len);
	if (SUCCESS == zend_fcall_info_argn(&fcd->fci, 2, &fcd->fcz, &zdata)) {
		zend_fcall_info_call(&fcd->fci, &fcd->fcc, NULL, NULL);
		zend_fcall_info_args_clear(&fcd->fci, 0);
	}
	zval_ptr_dtor(&zdata);
	return len;
}


/* ZEND */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
