/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_API_H
#define PHP_HTTP_API_H

#define HTTP_SUPPORT				0x01L
#define HTTP_SUPPORT_REQUESTS		0x02L
#define HTTP_SUPPORT_MAGICMIME		0x04L
#define HTTP_SUPPORT_ENCODINGS		0x08L
#define HTTP_SUPPORT_SSLREQUESTS	0x20L
#define HTTP_SUPPORT_PERSISTENCE	0x40L
#define HTTP_SUPPORT_EVENTS			0x80L

#define HTTP_PARAMS_ALLOW_COMMA		0x01
#define HTTP_PARAMS_ALLOW_FAILURE	0x02
#define HTTP_PARAMS_RAISE_ERROR		0x04
#define HTTP_PARAMS_DEFAULT	(HTTP_PARAMS_ALLOW_COMMA|HTTP_PARAMS_ALLOW_FAILURE|HTTP_PARAMS_RAISE_ERROR)
#define HTTP_PARAMS_COLON_SEPARATOR	0x10

extern PHP_MINIT_FUNCTION(http_support);

#define http_support(f) _http_support(f)
PHP_HTTP_API long _http_support(long feature);

#define pretty_key(key, key_len, uctitle, xhyphen) _http_pretty_key(key, key_len, uctitle, xhyphen)
extern char *_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen);

#define http_boundary(b, l) _http_boundary((b), (l) TSRMLS_CC)
extern size_t _http_boundary(char *buf, size_t len TSRMLS_DC);

#define http_error(type, code, string) _http_error_ex(type, code, "%s", string)
#define http_error_ex _http_error_ex
extern void _http_error_ex(long type TSRMLS_DC, long code, const char *format, ...);


#ifdef ZEND_ENGINE_2
#define http_exception_wrap(o, n, ce) _http_exception_wrap((o), (n), (ce) TSRMLS_CC)
extern zval *_http_exception_wrap(zval *old_exception, zval *new_exception, zend_class_entry *ce TSRMLS_DC);

#define http_try \
{ \
		zval *old_exception = EG(exception); \
		EG(exception) = NULL;
#define http_catch(ex_ce) \
		if (EG(exception) && old_exception) { \
			EG(exception) = http_exception_wrap(old_exception, EG(exception), ex_ce); \
		} \
}
#define http_final(ex_ce) \
	if (EG(exception)) { \
		EG(exception) = http_exception_wrap(EG(exception), NULL, ex_ce); \
	}

typedef zend_object_value (*http_object_new_t)(zend_class_entry *ce, void *, void ** TSRMLS_DC);

#define http_object_new(ov, cn, cl, co, ce, i, pp) _http_object_new((ov), (cn), (cl), (http_object_new_t) (co), (ce), (i), (void *) (pp) TSRMLS_CC)
extern STATUS _http_object_new(zend_object_value *ov, const char *cname_str, uint cname_len, http_object_new_t create, zend_class_entry *parent_ce, void *intern_ptr, void **obj_ptr TSRMLS_DC);
#endif /* ZEND_ENGINE_2 */


#define HTTP_CHECK_CURL_INIT(ch, init, action) \
	if ((!(ch)) && (!((ch) = init))) { \
		http_error(HE_WARNING, HTTP_E_REQUEST, "Could not initialize curl"); \
		action; \
	}
#define HTTP_CHECK_CONTENT_TYPE(ct, action) \
	if (!strchr((ct), '/')) { \
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, \
			"Content type \"%s\" does not seem to contain a primary and a secondary part", (ct)); \
		action; \
	}
#define HTTP_CHECK_MESSAGE_TYPE_RESPONSE(msg, action) \
		if (!HTTP_MSG_TYPE(RESPONSE, (msg))) { \
			http_error(HE_NOTICE, HTTP_E_MESSAGE_TYPE, "HttpMessage is not of type HTTP_MSG_RESPONSE"); \
			action; \
		}
#define HTTP_CHECK_MESSAGE_TYPE_REQUEST(msg, action) \
		if (!HTTP_MSG_TYPE(REQUEST, (msg))) { \
			http_error(HE_NOTICE, HTTP_E_MESSAGE_TYPE, "HttpMessage is not of type HTTP_MSG_REQUEST"); \
			action; \
		}
#define HTTP_CHECK_GZIP_LEVEL(level, action) \
	if (level < -1 || level > 9) { \
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid compression level (-1 to 9): %d", level); \
		action; \
	}
#ifndef PHP_OUTPUT_NEWAPI
#	define HTTP_GET_OUTPUT_START() \
		char *output_start_filename = php_get_output_start_filename(TSRMLS_C); \
		int output_start_lineno = php_get_output_start_lineno(TSRMLS_C)
#else
#	define HTTP_GET_OUTPUT_START() \
		char *output_start_filename = php_output_get_start_filename(TSRMLS_C); \
		int output_start_lineno = php_output_get_start_lineno(TSRMLS_C)
#endif
#define HTTP_CHECK_HEADERS_SENT(action) \
	if (SG(headers_sent) && !SG(request_info).no_headers) { \
		HTTP_GET_OUTPUT_START(); \
		if (output_start_filename) { \
			http_error_ex(HE_WARNING, HTTP_E_HEADER, "Cannot modify header information - headers already sent by (output started at %s:%d)", \
				output_start_filename, output_start_lineno); \
		} else { \
			http_error(HE_WARNING, HTTP_E_HEADER, "Cannot modify header information - headers already sent"); \
		} \
		action; \
	}

#define http_log(f, i, m) _http_log_ex((f), (i), (m) TSRMLS_CC)
extern void _http_log_ex(char *file, const char *ident, const char *message TSRMLS_DC);

#define http_exit(s, h) http_exit_ex((s), (h), NULL, 1)
#define http_exit_ex(s, h, b, e) _http_exit_ex((s), (h), (b), (e) TSRMLS_CC)
extern STATUS _http_exit_ex(int status, char *header, char *body, zend_bool send_header TSRMLS_DC);

#define http_check_method(m) http_check_method_ex((m), HTTP_KNOWN_METHODS)
#define http_check_method_ex(m, a) _http_check_method_ex((m), (a))
extern STATUS _http_check_method_ex(const char *method, const char *methods);

#define http_got_server_var(v) (NULL != http_get_server_var_ex((v), strlen(v), 1))
#define http_get_server_var(v, c) http_get_server_var_ex((v), strlen(v), (c))
#define http_get_server_var_ex(v, l, c) _http_get_server_var_ex((v), (l), (c) TSRMLS_CC)
PHP_HTTP_API zval *_http_get_server_var_ex(const char *key, size_t key_len, zend_bool check TSRMLS_DC);

#define http_get_request_body(b, l) _http_get_request_body_ex((b), (l), 1 TSRMLS_CC)
#define http_get_request_body_ex(b, l, d) _http_get_request_body_ex((b), (l), (d) TSRMLS_CC)
PHP_HTTP_API STATUS _http_get_request_body_ex(char **body, size_t *length, zend_bool dup TSRMLS_DC);

#define http_get_request_body_stream() _http_get_request_body_stream(TSRMLS_C)
PHP_HTTP_API php_stream *_http_get_request_body_stream(TSRMLS_D);


typedef void (*http_parse_params_callback)(void *cb_arg, const char *key, int keylen, const char *val, int vallen TSRMLS_DC);

#define http_parse_params_default_callback _http_parse_params_default_callback
PHP_HTTP_API void _http_parse_params_default_callback(void *ht, const char *key, int keylen, const char *val, int vallen TSRMLS_DC);

#define http_parse_params(s, f, ht) _http_parse_params_ex((s), (f), _http_parse_params_default_callback, (ht) TSRMLS_CC)
#define http_parse_params_ex(s, f, cb, a) _http_parse_params_ex((s), (f), (cb), (a) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_params_ex(const char *params, int flags, http_parse_params_callback cb, void *cb_arg TSRMLS_DC);


#define http_sleep(s) _http_sleep(s)
static inline void _http_sleep(double s)
{
#define HTTP_DIFFSEC (0.001)
#define HTTP_MLLISEC (1000)
#define HTTP_MCROSEC (1000 * 1000)
#define HTTP_NANOSEC (1000 * 1000 * 1000)
#define HTTP_MSEC(s) ((long)(s * HTTP_MLLISEC))
#define HTTP_USEC(s) ((long)(s * HTTP_MCROSEC))
#define HTTP_NSEC(s) ((long)(s * HTTP_NANOSEC))

#if defined(PHP_WIN32)
	Sleep((DWORD) HTTP_MSEC(s));
#elif defined(HAVE_USLEEP)
	usleep(HTTP_USEC(s));
#elif defined(HAVE_NANOSLEEP)
	struct timespec req, rem;

	req.tv_sec = (time_t) s;
	req.tv_nsec = HTTP_NSEC(s) % HTTP_NANOSEC;

	while (nanosleep(&req, &rem) && (errno == EINTR) && (HTTP_NSEC(rem.tv_sec) + rem.tv_nsec) > HTTP_NSEC(HTTP_DIFFSEC))) {
		req.tv_sec = rem.tv_sec;
		req.tv_nsec = rem.tv_nsec;
	}
#else
	struct timeval timeout;
 
	timeout.tv.sec = (time_t) s;
	timeout.tv_usec = HTTP_USEC(s) % HTTP_MCROSEC;
	
	select(0, NULL, NULL, NULL, &timeout);
#endif
}

#define http_locate_str _http_locate_str
static inline const char *_http_locate_str(const char *h, size_t h_len, const char *n, size_t n_len)
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

#define http_locate_body _http_locate_body
static inline const char *_http_locate_body(const char *message)
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

#define http_locate_eol _http_locate_eol
static inline const char *_http_locate_eol(const char *line, int *eol_len)
{
	const char *eol = strpbrk(line, "\r\n");
	
	if (eol_len) {
		*eol_len = eol ? ((eol[0] == '\r' && eol[1] == '\n') ? 2 : 1) : 0;
	}
	return eol;
}

#define http_zset(t, z) _http_zset((t), (z))
static inline zval *_http_zset(int type, zval *z)
{
	if (Z_TYPE_P(z) != type) {
		switch (type) {
			case IS_NULL:	convert_to_null(z);		break;
			case IS_BOOL:	convert_to_boolean(z);	break;
			case IS_LONG:	convert_to_long(z);		break;
			case IS_DOUBLE:	convert_to_double(z);	break;
			case IS_STRING:	convert_to_string(z);	break;
			case IS_ARRAY:	convert_to_array(z);	break;
			case IS_OBJECT:	convert_to_object(z);	break;
		}
	}
	return z;
}
#define http_zsep(t, z) _http_zsep_ex((t), (z), NULL)
#define http_zsep_ex(t, z, p) _http_zsep_ex((t), (z), (p))
static inline zval *_http_zsep_ex(int type, zval *z, zval **p) {
	ZVAL_ADDREF(z);
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
	if (p) {
		*p = z;
	}
	return z;
}

typedef struct _HashKey {
	char *str;
	uint len;
	ulong num;
	uint dup:1;
	uint type:31;
} HashKey;
#define initHashKey(dup) {NULL, 0, 0, (dup), 0}

#define FOREACH_VAL(pos, array, val) FOREACH_HASH_VAL(pos, Z_ARRVAL_P(array), val)
#define FOREACH_HASH_VAL(pos, hash, val) \
	for (	zend_hash_internal_pointer_reset_ex(hash, &pos); \
			zend_hash_get_current_data_ex(hash, (void *) &val, &pos) == SUCCESS; \
			zend_hash_move_forward_ex(hash, &pos))

#define FOREACH_KEY(pos, array, key) FOREACH_HASH_KEY(pos, Z_ARRVAL_P(array), key)
#define FOREACH_HASH_KEY(pos, hash, _key) \
	for (	zend_hash_internal_pointer_reset_ex(hash, &pos); \
			((_key).type = zend_hash_get_current_key_ex(hash, &(_key).str, &(_key).len, &(_key).num, (zend_bool) (_key).dup, &pos)) != HASH_KEY_NON_EXISTANT; \
			zend_hash_move_forward_ex(hash, &pos)) \

#define FOREACH_KEYVAL(pos, array, key, val) FOREACH_HASH_KEYVAL(pos, Z_ARRVAL_P(array), key, val)
#define FOREACH_HASH_KEYVAL(pos, hash, _key, val) \
	for (	zend_hash_internal_pointer_reset_ex(hash, &pos); \
			((_key).type = zend_hash_get_current_key_ex(hash, &(_key).str, &(_key).len, &(_key).num, (zend_bool) (_key).dup, &pos)) != HASH_KEY_NON_EXISTANT && \
			zend_hash_get_current_data_ex(hash, (void *) &val, &pos) == SUCCESS; \
			zend_hash_move_forward_ex(hash, &pos))

#define array_copy(src, dst) zend_hash_copy(dst, src, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *))
#define ARRAY_JOIN_STRONLY 1
#define ARRAY_JOIN_PRETTIFY 2
#define array_join(src, dst, append, flags) zend_hash_apply_with_arguments(src HTTP_ZAPI_HASH_TSRMLS_CC, (append)?apply_array_append_func:apply_array_merge_func, 2, dst, (int)flags)

extern int apply_array_append_func(void *pDest HTTP_ZAPI_HASH_TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key);
extern int apply_array_merge_func(void *pDest HTTP_ZAPI_HASH_TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

