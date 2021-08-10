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

#ifndef PHP_HTTP_MISC_H
#define PHP_HTTP_MISC_H

/* DEFAULTS */

/* DATE FORMAT RFC1123 */
#define PHP_HTTP_DATE_FORMAT "D, d M Y H:i:s \\G\\M\\T"

/* CR LF */
#define PHP_HTTP_CRLF "\r\n"

/* def URL arg separator */
#define PHP_HTTP_URL_ARGSEP "&"

/* send buffer size */
#define PHP_HTTP_SENDBUF_SIZE 40960

/* allowed characters of header field names */
#define PHP_HTTP_HEADER_NAME_CHARS "!#$%&'*+-.^_`|~1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* SLEEP */

#define PHP_HTTP_DIFFSEC (0.001)
#define PHP_HTTP_MLLISEC (1000)
#define PHP_HTTP_MCROSEC (1000 * 1000)
#define PHP_HTTP_NANOSEC (1000 * 1000 * 1000)
#define PHP_HTTP_MSEC(s) ((long)(s * PHP_HTTP_MLLISEC))
#define PHP_HTTP_USEC(s) ((long)(s * PHP_HTTP_MCROSEC))
#define PHP_HTTP_NSEC(s) ((long)(s * PHP_HTTP_NANOSEC))

PHP_HTTP_API void php_http_sleep(double s);

/* STRING UTILITIES */

#ifndef PTR_SET
#	define PTR_SET(STR, SET) \
	{ \
		PTR_FREE(STR); \
		STR = SET; \
	}
#endif

#define STR_PTR(s) (s?s:"")

#define lenof(S) (sizeof(S) - 1)

#define PHP_HTTP_MATCH_LOOSE	0
#define PHP_HTTP_MATCH_CASE		0x01
#define PHP_HTTP_MATCH_WORD		0x10
#define PHP_HTTP_MATCH_FULL		0x20
#define PHP_HTTP_MATCH_STRICT	(PHP_HTTP_MATCH_CASE|PHP_HTTP_MATCH_FULL)

int php_http_match(const char *haystack, const char *needle, int flags);
char *php_http_pretty_key(register char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen);
size_t php_http_boundary(char *buf, size_t len);
int php_http_select_str(const char *cmp, int argc, ...);

#define php_http_locate_str(h, h_len, n, n_len) zend_memnstr((h), (n), (n_len), (h)+(h_len))

static inline const char *php_http_locate_eol(const char *line, int *eol_len)
{
	const char *eol = strpbrk(line, "\r\n");

	if (eol_len) {
		*eol_len = eol ? ((eol[0] == '\r' && eol[1] == '\n') ? 2 : 1) : 0;
	}
	return eol;
}

static inline const char *php_http_locate_bin_eol(const char *bin, size_t len, int *eol_len)
{
	register const char *eol = bin;

	while (len--) {
		if (UNEXPECTED(*eol == '\r' || *eol == '\n')) {
			if (EXPECTED(eol_len)) {
				*eol_len = (EXPECTED(eol[0] == '\r' && eol[1] == '\n') ? 2 : 1);
			}
			return eol;
		}
		++eol;
	}
	return NULL;
}

/* ZEND */

#if PHP_DEBUG
#	undef  HASH_OF
#	define HASH_OF(p) ((HashTable*)(Z_TYPE_P(p)==IS_ARRAY ? Z_ARRVAL_P(p) : ((Z_TYPE_P(p)==IS_OBJECT ? Z_OBJ_HT_P(p)->get_properties(Z_OBJ_P(p)) : NULL))))
#endif

#if PHP_VERSION_ID >= 80100
# define php_http_mem_stream_open(type, zstr) php_stream_memory_open((type), (zstr))
#else
# define php_http_mem_stream_open(type, zstr) php_stream_memory_open((type), (zstr)->val, (zstr)->len)
# define ZEND_BEGIN_ARG_WITH_TENTATIVE_RETURN_TYPE_INFO_EX(name, return_reference, required_num_args, type, allow_null) \
		 ZEND_BEGIN_ARG_INFO_EX(name, 0, return_reference, required_num_args)
# define ZEND_BEGIN_ARG_WITH_TENTATIVE_RETURN_OBJ_INFO_EX(name, return_reference, required_num_args, type, allow_null) \
		 ZEND_BEGIN_ARG_INFO_EX(name, 0, return_reference, required_num_args)
#endif

#define HT_IS_RECURSIVE(ht) GC_IS_RECURSIVE(ht)
#define HT_PROTECT_RECURSION(ht) GC_PROTECT_RECURSION(ht)
#define HT_UNPROTECT_RECURSION(ht) GC_UNPROTECT_RECURSION(ht)

#ifndef convert_to_explicit_type
# define convert_to_explicit_type(pzv, type) \
	do { \
		switch (type) { \
			case IS_NULL: \
				convert_to_null(pzv); \
				break; \
			case IS_LONG: \
				convert_to_long(pzv); \
				break; \
			case IS_DOUBLE: \
				convert_to_double(pzv); \
				break; \
			case _IS_BOOL: \
				convert_to_boolean(pzv); \
				break; \
			case IS_ARRAY: \
				convert_to_array(pzv); \
				break; \
			case IS_OBJECT: \
				convert_to_object(pzv); \
				break; \
			case IS_STRING: \
				convert_to_string(pzv); \
				break; \
			default: \
				assert(0); \
				break; \
		} \
	} while (0);

#endif
static inline void *PHP_HTTP_OBJ(zend_object *zo, zval *zv)
{
	if (!zo) {
		zo = Z_OBJ_P(zv);
	}
	return (char *) zo - zo->handlers->offset;
}

static inline zend_string *php_http_cs2zs(char *s, size_t l)
{
	zend_string *str = zend_string_init(s, l, 0);
	efree(s);
	return str;
}

static inline ZEND_RESULT_CODE php_http_ini_entry(const char *name_str, size_t name_len, const char **val_str, size_t *val_len, zend_bool orig)
{
	zend_ini_entry *ini_entry;

	if ((ini_entry = zend_hash_str_find_ptr(EG(ini_directives), name_str, name_len))) {
		if (orig && ini_entry->modified) {
			*val_str = ini_entry->orig_value->val;
			*val_len = ini_entry->orig_value->len;
		} else {
			*val_str = ini_entry->value->val;
			*val_len = ini_entry->value->len;
		}
		return SUCCESS;
	}
	return FAILURE;
}

/* return object(values) */
#define ZVAL_OBJECT(z, o, addref) \
	ZVAL_OBJ(z, o); \
	if (addref) { \
		Z_ADDREF_P(z); \
	}
#define RETVAL_OBJECT(o, addref) \
	ZVAL_OBJECT(return_value, o, addref)
#define RETURN_OBJECT(o, addref) \
	RETVAL_OBJECT(o, addref); \
	return

#define EMPTY_FUNCTION_ENTRY {NULL, NULL, NULL, 0, 0}

#define PHP_MINIT_CALL(func) PHP_MINIT(func)(INIT_FUNC_ARGS_PASSTHRU)
#define PHP_RINIT_CALL(func) PHP_RINIT(func)(INIT_FUNC_ARGS_PASSTHRU)
#define PHP_MSHUTDOWN_CALL(func) PHP_MSHUTDOWN(func)(SHUTDOWN_FUNC_ARGS_PASSTHRU)
#define PHP_RSHUTDOWN_CALL(func) PHP_RSHUTDOWN(func)(SHUTDOWN_FUNC_ARGS_PASSTHRU)

/* ARRAYS */

PHP_HTTP_API unsigned php_http_array_list(HashTable *ht, unsigned argc, ...);

typedef struct php_http_arrkey {
	zend_ulong h;
	zend_string *key;
	unsigned allocated:1;
	unsigned stringified:1;
} php_http_arrkey_t;

static inline void *php_http_arrkey_stringify(php_http_arrkey_t *arrkey, zend_hash_key *key)
{
	if (arrkey) {
		arrkey->allocated = 0;
	} else {
		arrkey = emalloc(sizeof(*arrkey));
		arrkey->allocated = 1;
	}

	if (key) {
		memcpy(arrkey, key, sizeof(*key));
	}
	if ((arrkey->stringified = !arrkey->key)) {
		arrkey->key = zend_long_to_str(arrkey->h);
	}
	return arrkey;
}

static inline void php_http_arrkey_dtor(php_http_arrkey_t *arrkey)
{
	if (arrkey->stringified) {
		zend_string_release(arrkey->key);
	}
	if (arrkey->allocated) {
		efree(arrkey);
	}
}

#define array_copy(src, dst) zend_hash_copy(dst, src, (copy_ctor_func_t) zval_add_ref)
#define array_copy_strings(src, dst) zend_hash_copy(dst, src, php_http_array_copy_strings)
#define ARRAY_JOIN_STRONLY   0x01
#define ARRAY_JOIN_PRETTIFY  0x02
#define ARRAY_JOIN_STRINGIFY 0x04
#define array_join(src, dst, append, flags) zend_hash_apply_with_arguments(src, (append)?php_http_array_apply_append_func:php_http_array_apply_merge_func, 2, dst, (int)flags)

void php_http_array_copy_strings(zval *zp);
int php_http_array_apply_append_func(zval *pDest, int num_args, va_list args, zend_hash_key *hash_key);
int php_http_array_apply_merge_func(zval *pDest, int num_args, va_list args, zend_hash_key *hash_key);

/* PASS CALLBACK */

typedef size_t (*php_http_pass_callback_t)(void *cb_arg, const char *str, size_t len);
typedef size_t (*php_http_pass_php_http_buffer_callback_t)(void *cb_arg, php_http_buffer_t *str);
typedef size_t (*php_http_pass_format_callback_t)(void *cb_arg, const char *fmt, ...);

typedef struct php_http_pass_fcall_arg {
	zval fcz;
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
} php_http_pass_fcall_arg_t;

PHP_HTTP_API size_t php_http_pass_fcall_callback(void *cb_arg, const char *str, size_t len);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
