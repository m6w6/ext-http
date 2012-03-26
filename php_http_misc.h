/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_MISC_H
#define PHP_HTTP_MISC_H

/* DEFAULTS */

/* DATE FORMAT RFC1123 */
#define PHP_HTTP_DATE_FORMAT "D, d M Y H:i:s \\G\\M\\T"

/* CR LF */
#define PHP_HTTP_CRLF "\r\n"

/* default cache control */
#define PHP_HTTP_DEFAULT_CACHECONTROL "private, must-revalidate, max-age=0"

/* max URL length */
#define PHP_HTTP_URL_MAXLEN 4096

/* max request method length */
#define PHP_HTTP_REQUEST_METHOD_MAXLEN 31

/* def URL arg separator */
#define PHP_HTTP_URL_ARGSEP "&"

/* send buffer size */
#define PHP_HTTP_SENDBUF_SIZE 40960

/* CURL buffer size */
#define PHP_HTTP_CURLBUF_SIZE 16384

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

#define PHP_HTTP_CHECK_CONTENT_TYPE(ct, action) \
	if (!strchr((ct), '/')) { \
		php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, \
			"Content type \"%s\" does not seem to contain a primary and a secondary part", (ct)); \
		action; \
	}


#ifndef STR_SET
#	define STR_SET(STR, SET) \
	{ \
		STR_FREE(STR); \
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

extern int php_http_match(const char *haystack, const char *needle, int flags);
extern char *php_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen);
extern size_t php_http_boundary(char *buf, size_t len TSRMLS_DC);
extern int php_http_select_str(const char *cmp, int argc, ...);

static inline const char *php_http_locate_str(const char *h, size_t h_len, const char *n, size_t n_len)
{
	const char *p, *e;

	if (n_len && h_len) {
		e = h + h_len;
		do {
			if (*h == *n) {
				for (p = n; *p == h[p-n]; ++p) {
					if (p == n+n_len-1) {
						return h;
					}
				}
			}
		} while (h++ != e);
	}

	return NULL;
}

static inline const char *php_http_locate_body(const char *message)
{
	const char *body = NULL, *msg = message;

	while (*msg) {
		if (*msg == '\n') {
			if (*(msg+1) == '\n') {
				body = msg + 2;
				break;
			} else if (*(msg+1) == '\r' && *(msg+2) == '\n') {
				body = msg + 3;
				break;
			}
		}
		++msg;
	}
	return body;
}

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
	const char *eol;

	for (eol = bin; eol - bin < len; ++eol) {
		if (*eol == '\r' || *eol == '\n') {
			if (eol_len) {
				*eol_len = eol ? ((eol[0] == '\r' && eol[1] == '\n') ? 2 : 1) : 0;
			}
			return eol;
		}
	}

	return NULL;
}

/* ZEND */

#define INIT_PZVAL_ARRAY(zv, ht) \
	{ \
		INIT_PZVAL((zv)); \
		Z_TYPE_P(zv) = IS_ARRAY; \
		Z_ARRVAL_P(zv) = (ht); \
	}

static inline zval *php_http_ztyp(int type, zval *z)
{
	SEPARATE_ARG_IF_REF(z);
	if (Z_TYPE_P(z) != type) {
		switch (type) {
			case IS_NULL:	convert_to_null_ex(&z);		break;
			case IS_BOOL:	convert_to_boolean_ex(&z);	break;
			case IS_LONG:	convert_to_long_ex(&z);		break;
			case IS_DOUBLE:	convert_to_double_ex(&z);	break;
			case IS_STRING:	convert_to_string_ex(&z);	break;
			case IS_ARRAY:	convert_to_array_ex(&z);	break;
			case IS_OBJECT:	convert_to_object_ex(&z);	break;
		}
	}
	return z;
}

static inline zval *php_http_zsep(zend_bool add_ref, int type, zval *z) {
	if (add_ref) {
		Z_ADDREF_P(z);
	}
	if (Z_TYPE_P(z) != type) {
		switch (type) {
			case IS_NULL:	convert_to_null_ex(&z);		break;
			case IS_BOOL:	convert_to_boolean_ex(&z);	break;
			case IS_LONG:	convert_to_long_ex(&z);		break;
			case IS_DOUBLE:	convert_to_double_ex(&z);	break;
			case IS_STRING:	convert_to_string_ex(&z);	break;
			case IS_ARRAY:	convert_to_array_ex(&z);	break;
			case IS_OBJECT:	convert_to_object_ex(&z);	break;
		}
	} else {
		SEPARATE_ZVAL_IF_NOT_REF(&z);
	}
	return z;
}


/* return bool (v == SUCCESS) */
#define RETVAL_SUCCESS(v) RETVAL_BOOL(SUCCESS == (v))
#define RETURN_SUCCESS(v) RETURN_BOOL(SUCCESS == (v))
/* return object(values) */
#define RETVAL_OBJECT(o, addref) \
	RETVAL_OBJVAL((o)->value.obj, addref)
#define RETURN_OBJECT(o, addref) \
	RETVAL_OBJECT(o, addref); \
	return
#define RETVAL_OBJVAL(ov, addref) \
	ZVAL_OBJVAL(return_value, ov, addref)
#define RETURN_OBJVAL(ov, addref) \
	RETVAL_OBJVAL(ov, addref); \
	return
#define ZVAL_OBJVAL(zv, ov, addref) \
	(zv)->type = IS_OBJECT; \
	(zv)->value.obj = (ov);\
	if (addref && Z_OBJ_HT_P(zv)->add_ref) { \
		Z_OBJ_HT_P(zv)->add_ref((zv) TSRMLS_CC); \
	}
/* return property */
#define RETVAL_PROP(CE, n) RETVAL_PROP_EX(CE, getThis(), n)
#define RETURN_PROP(CE, n) RETURN_PROP_EX(CE, getThis(), n)
#define RETVAL_PROP_EX(CE, this, n) \
	{ \
		zval *__prop = zend_read_property(CE, this, ZEND_STRL(n), 0 TSRMLS_CC); \
		RETVAL_ZVAL(__prop, 1, 0); \
	}
#define RETURN_PROP_EX(CE, this, n) \
	{ \
		zval *__prop = zend_read_property(CE, this, ZEND_STRL(n), 0 TSRMLS_CC); \
		RETURN_ZVAL(__prop, 1, 0); \
	}
#define RETVAL_SPROP(CE, n) \
	{ \
		zval *__prop = zend_read_static_property(CE, ZEND_STRL(n), 0 TSRMLS_CC); \
		RETVAL_ZVAL(__prop, 1, 0); \
	}
#define RETURN_SPROP(CE, n) \
	{ \
		zval *__prop = zend_read_static_property(CE, ZEND_STRL(n), 0 TSRMLS_CC); \
		RETURN_ZVAL(__prop, 1, 0); \
	}

#define Z_OBJ_DELREF(z) \
	if (Z_OBJ_HT(z)->del_ref) { \
		Z_OBJ_HT(z)->del_ref(&(z) TSRMLS_CC); \
	}
#define Z_OBJ_ADDREF(z) \
	if (Z_OBJ_HT(z)->add_ref) { \
		Z_OBJ_HT(z)->add_ref(&(z) TSRMLS_CC); \
	}
#define Z_OBJ_DELREF_P(z) \
	if (Z_OBJ_HT_P(z)->del_ref) { \
		Z_OBJ_HT_P(z)->del_ref((z) TSRMLS_CC); \
	}
#define Z_OBJ_ADDREF_P(z) \
	if (Z_OBJ_HT_P(z)->add_ref) { \
		Z_OBJ_HT_P(z)->add_ref((z) TSRMLS_CC); \
	}
#define Z_OBJ_DELREF_PP(z) \
	if (Z_OBJ_HT_PP(z)->del_ref) { \
		Z_OBJ_HT_PP(z)->del_ref(*(z) TSRMLS_CC); \
	}
#define Z_OBJ_ADDREF_PP(z) \
	if (Z_OBJ_HT_PP(z)->add_ref) { \
		Z_OBJ_HT_PP(z)->add_ref(*(z) TSRMLS_CC); \
	}

#define PHP_HTTP_BEGIN_ARGS_EX(class, method, ret_ref, req_args)	ZEND_BEGIN_ARG_INFO_EX(args_for_ ##class## _ ##method , 0, ret_ref, req_args)
#define PHP_HTTP_BEGIN_ARGS_AR(class, method, ret_ref, req_args)	ZEND_BEGIN_ARG_INFO_EX(args_for_ ##class## _ ##method , 1, ret_ref, req_args)
#define PHP_HTTP_END_ARGS											}
#define PHP_HTTP_EMPTY_ARGS_EX(class, method, ret_ref)				PHP_HTTP_BEGIN_ARGS_EX(class, method, ret_ref, 0) PHP_HTTP_END_ARGS
#define PHP_HTTP_ARGS(class, method)								args_for_ ##class## _ ##method
#define PHP_HTTP_ARG_VAL(name, pass_ref)							ZEND_ARG_INFO(pass_ref, name)
#define PHP_HTTP_ARG_OBJ(class, name, allow_null)					ZEND_ARG_OBJ_INFO(0, name, class, allow_null)
#define PHP_HTTP_ARG_ARR(name, allow_null, pass_ref)				ZEND_ARG_ARRAY_INFO(pass_ref, name, allow_null)

#define EMPTY_FUNCTION_ENTRY {NULL, NULL, NULL, 0, 0}

#define PHP_MINIT_CALL(func) PHP_MINIT(func)(INIT_FUNC_ARGS_PASSTHRU)
#define PHP_RINIT_CALL(func) PHP_RINIT(func)(INIT_FUNC_ARGS_PASSTHRU)
#define PHP_MSHUTDOWN_CALL(func) PHP_MSHUTDOWN(func)(SHUTDOWN_FUNC_ARGS_PASSTHRU)
#define PHP_RSHUTDOWN_CALL(func) PHP_RSHUTDOWN(func)(SHUTDOWN_FUNC_ARGS_PASSTHRU)


#define PHP_HTTP_INI_ENTRY(entry, default, scope, updater, global) \
	STD_PHP_INI_ENTRY(entry, default, scope, updater, global, zend_php_http_globals, php_http_globals)
#define PHP_HTTP_INI_ENTRY_EX(entry, default, scope, updater, displayer, global) \
	STD_PHP_INI_ENTRY_EX(entry, default, scope, updater, global, zend_php_http_globals, php_http_globals, displayer)

#define PHP_HTTP_REGISTER_CLASS(ns, classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		memset(&ce, 0, sizeof(zend_class_entry)); \
		INIT_NS_CLASS_ENTRY(ce, #ns, #classname, php_ ##name## _method_entry); \
		php_ ##name## _class_entry = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		php_ ##name## _class_entry->ce_flags |= flags;  \
	}

#define PHP_HTTP_REGISTER_INTERFACE(ns, ifacename, name, flags) \
	{ \
		zend_class_entry ce; \
		memset(&ce, 0, sizeof(zend_class_entry)); \
		INIT_NS_CLASS_ENTRY(ce, #ns, #ifacename, php_ ##name## _method_entry); \
		php_ ##name## _class_entry = zend_register_internal_interface(&ce TSRMLS_CC); \
		php_ ##name## _class_entry->ce_flags |= flags; \
	}

#define PHP_HTTP_REGISTER_EXCEPTION(classname, cename, parent) \
	{ \
		zend_class_entry ce; \
		memset(&ce, 0, sizeof(zend_class_entry)); \
		INIT_NS_CLASS_ENTRY(ce, "http", #classname, NULL); \
		ce.create_object = NULL; \
		cename = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
	}

#define ACC_PROP_PRIVATE(ce, flags)		((flags & ZEND_ACC_PRIVATE) && (EG(scope) && ce == EG(scope))
#define ACC_PROP_PROTECTED(ce, flags)	((flags & ZEND_ACC_PROTECTED) && (zend_check_protected(ce, EG(scope))))
#define ACC_PROP_PUBLIC(flags)			(flags & ZEND_ACC_PUBLIC)
#define ACC_PROP(ce, flags)				(ACC_PROP_PUBLIC(flags) || ACC_PROP_PRIVATE(ce, flags) || ACC_PROP_PROTECTED(ce, flags))

#ifdef PHP_HTTP_HAVE_CURL
#		define PHP_HTTP_DECLARE_ARG_PASS_INFO() \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_2, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
 \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_3, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
 \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_4, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
 \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_5, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO();

#else
#		define PHP_HTTP_DECLARE_ARG_PASS_INFO() \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_2, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
\
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_3, 0) \
			ZEND_ARG_PASS_INFO(0) \
			ZEND_ARG_PASS_INFO(0) \
			ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
\
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_4, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO();
#endif /* PHP_HTTP_HAVE_CURL */

/* ARRAYS */

typedef struct php_http_array_hashkey {
	char *str;
	uint len;
	ulong num;
	uint dup:1;
	uint type:31;
} php_http_array_hashkey_t;
#define php_http_array_hashkey_init(dup) {NULL, 0, 0, (dup), 0}

#define FOREACH_VAL(pos, array, val) FOREACH_HASH_VAL(pos, HASH_OF(array), val)
#define FOREACH_HASH_VAL(pos, hash, val) \
	for (	zend_hash_internal_pointer_reset_ex(hash, &pos); \
			zend_hash_get_current_data_ex(hash, (void *) &val, &pos) == SUCCESS; \
			zend_hash_move_forward_ex(hash, &pos))

#define FOREACH_KEY(pos, array, key) FOREACH_HASH_KEY(pos, HASH_OF(array), key)
#define FOREACH_HASH_KEY(pos, hash, _key) \
	for (	zend_hash_internal_pointer_reset_ex(hash, &pos); \
			((_key).type = zend_hash_get_current_key_ex(hash, &(_key).str, &(_key).len, &(_key).num, (zend_bool) (_key).dup, &pos)) != HASH_KEY_NON_EXISTANT; \
			zend_hash_move_forward_ex(hash, &pos)) \

#define FOREACH_KEYVAL(pos, array, key, val) FOREACH_HASH_KEYVAL(pos, HASH_OF(array), key, val)
#define FOREACH_HASH_KEYVAL(pos, hash, _key, val) \
	for (	zend_hash_internal_pointer_reset_ex(hash, &pos); \
			((_key).type = zend_hash_get_current_key_ex(hash, &(_key).str, &(_key).len, &(_key).num, (zend_bool) (_key).dup, &pos)) != HASH_KEY_NON_EXISTANT && \
			zend_hash_get_current_data_ex(hash, (void *) &val, &pos) == SUCCESS; \
			zend_hash_move_forward_ex(hash, &pos))

#define array_copy(src, dst) zend_hash_copy(dst, src, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *))
#define ARRAY_JOIN_STRONLY 1
#define ARRAY_JOIN_PRETTIFY 2
#define array_join(src, dst, append, flags) zend_hash_apply_with_arguments(src TSRMLS_CC, (append)?php_http_array_apply_append_func:php_http_array_apply_merge_func, 2, dst, (int)flags)

extern int php_http_array_apply_append_func(void *pDest TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key);
extern int php_http_array_apply_merge_func(void *pDest TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key);

/* PASS CALLBACK */

typedef size_t (*php_http_pass_callback_t)(void *cb_arg, const char *str, size_t len);
typedef size_t (*php_http_pass_php_http_buffer_callback_t)(void *cb_arg, php_http_buffer_t *str);

typedef struct php_http_pass_callback_arg {
	size_t (*cb_zts)(void *cb_arg, const char *str, size_t len TSRMLS_DC);
	void *cb_arg;
} php_http_pass_callback_arg_t;

PHP_HTTP_API size_t php_http_pass_wrapper(php_http_pass_callback_arg_t *cb_arg, const char *str, size_t len);

typedef struct php_http_pass_fcall_arg {
	zval *fcz;
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
#ifdef ZTS
	void ***ts;
#endif
} php_http_pass_fcall_arg_t;

PHP_HTTP_API size_t php_http_pass_fcall_callback(void *cb_arg, const char *str, size_t len);

/* ERROR */

extern void php_http_error(long type TSRMLS_DC, long code, const char *format, ...);

#define with_error_handling(eh, ec) \
	{ \
		zend_error_handling __eh; \
		zend_replace_error_handling((eh), (ec), &__eh TSRMLS_CC);

#define end_error_handling() \
		zend_restore_error_handling(&__eh TSRMLS_CC); \
	}

#ifndef E_THROW
#	define E_THROW -1
#endif
#define HE_THROW	E_THROW TSRMLS_CC
#define HE_NOTICE	E_NOTICE TSRMLS_CC
#define HE_WARNING	E_WARNING TSRMLS_CC
#define HE_ERROR	E_ERROR TSRMLS_CC

typedef enum php_http_error {
	PHP_HTTP_E_UNKNOWN = 0,
	PHP_HTTP_E_RUNTIME,
	PHP_HTTP_E_INVALID_PARAM,
	PHP_HTTP_E_HEADER,
	PHP_HTTP_E_MALFORMED_HEADERS,
	PHP_HTTP_E_REQUEST_METHOD,
	PHP_HTTP_E_MESSAGE,
	PHP_HTTP_E_MESSAGE_TYPE,
	PHP_HTTP_E_MESSAGE_BODY,
	PHP_HTTP_E_ENCODING,
	PHP_HTTP_E_CLIENT,
	PHP_HTTP_E_CLIENT_POOL,
	PHP_HTTP_E_CLIENT_DATASHARE,
	PHP_HTTP_E_REQUEST_FACTORY,
	PHP_HTTP_E_SOCKET,
	PHP_HTTP_E_RESPONSE,
	PHP_HTTP_E_URL,
	PHP_HTTP_E_QUERYSTRING,
	PHP_HTTP_E_COOKIE,
} php_http_error_t;

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
