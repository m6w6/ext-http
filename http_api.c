/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#include "php_http.h"

#include "php_output.h"
#include "ext/standard/url.h"

#include "php_http_api.h"
#include "php_http_send_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_exception_object.h"
#endif

PHP_MINIT_FUNCTION(http_support)
{
	HTTP_LONG_CONSTANT("HTTP_SUPPORT", HTTP_SUPPORT);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_REQUESTS", HTTP_SUPPORT_REQUESTS);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_MAGICMIME", HTTP_SUPPORT_MAGICMIME);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_ENCODINGS", HTTP_SUPPORT_ENCODINGS);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_SSLREQUESTS", HTTP_SUPPORT_SSLREQUESTS);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_PERSISTENCE", HTTP_SUPPORT_PERSISTENCE);
	
	HTTP_LONG_CONSTANT("HTTP_PARAMS_ALLOW_COMMA", HTTP_PARAMS_ALLOW_COMMA);
	HTTP_LONG_CONSTANT("HTTP_PARAMS_ALLOW_FAILURE", HTTP_PARAMS_ALLOW_FAILURE);
	HTTP_LONG_CONSTANT("HTTP_PARAMS_RAISE_ERROR", HTTP_PARAMS_RAISE_ERROR);
	HTTP_LONG_CONSTANT("HTTP_PARAMS_DEFAULT", HTTP_PARAMS_DEFAULT);
	
	return SUCCESS;
}

PHP_HTTP_API long _http_support(long feature)
{
	long support = HTTP_SUPPORT;
	
#ifdef HTTP_HAVE_CURL
	support |= HTTP_SUPPORT_REQUESTS;
#	ifdef HTTP_HAVE_SSL
	support |= HTTP_SUPPORT_SSLREQUESTS;
#	endif
#	ifdef HTTP_HAVE_PERSISTENT_HANDLES
	support |= HTTP_SUPPORT_PERSISTENCE;
#	endif
#endif
#ifdef HTTP_HAVE_MAGIC
	support |= HTTP_SUPPORT_MAGICMIME;
#endif
#ifdef HTTP_HAVE_ZLIB
	support |= HTTP_SUPPORT_ENCODINGS;
#endif

	if (feature) {
		return (feature == (support & feature));
	}
	return support;
}

/* char *pretty_key(char *, size_t, zend_bool, zend_bool) */
char *_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen)
{
	size_t i;
	int wasalpha;
	
	if (key && key_len) {
		if ((wasalpha = HTTP_IS_CTYPE(alpha, key[0]))) {
			key[0] = (char) (uctitle ? HTTP_TO_CTYPE(upper, key[0]) : HTTP_TO_CTYPE(lower, key[0]));
		}
		for (i = 1; i < key_len; i++) {
			if (HTTP_IS_CTYPE(alpha, key[i])) {
				key[i] = (char) (((!wasalpha) && uctitle) ? HTTP_TO_CTYPE(upper, key[i]) : HTTP_TO_CTYPE(lower, key[i]));
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

/* {{{ void http_error(long, long, char*) */
void _http_error_ex(long type TSRMLS_DC, long code, const char *format, ...)
{
	va_list args;
	
	va_start(args, format);
#ifdef ZEND_ENGINE_2
	if ((type == E_THROW) || (PG(error_handling) == EH_THROW)) {
		char *message;
		zend_class_entry *ce = http_exception_get_for_code(code);
		
		http_try {
			vspprintf(&message, 0, format, args);
			zend_throw_exception(ce, message, code TSRMLS_CC);
			efree(message);
		} http_catch(PG(exception_class) ? PG(exception_class) : HTTP_EX_DEF_CE);
	} else
#endif
	php_verror(NULL, "", type, format, args TSRMLS_CC);
	va_end(args);
}
/* }}} */

#ifdef ZEND_ENGINE_2
static inline void copy_bt_args(zval *from, zval *to TSRMLS_DC)
{
	zval **args, **trace_0, *old_trace_0, *trace = NULL;
	
	if ((trace = zend_read_property(ZEND_EXCEPTION_GET_DEFAULT(), from, "trace", lenof("trace"), 0 TSRMLS_CC))) {
		if (Z_TYPE_P(trace) == IS_ARRAY && SUCCESS == zend_hash_index_find(Z_ARRVAL_P(trace), 0, (void *) &trace_0)) {
			old_trace_0 = *trace_0;
			if (Z_TYPE_PP(trace_0) == IS_ARRAY && SUCCESS == zend_hash_find(Z_ARRVAL_PP(trace_0), "args", sizeof("args"), (void *) &args)) {
				if ((trace = zend_read_property(ZEND_EXCEPTION_GET_DEFAULT(), to, "trace", lenof("trace"), 0 TSRMLS_CC))) {
					if (Z_TYPE_P(trace) == IS_ARRAY && SUCCESS == zend_hash_index_find(Z_ARRVAL_P(trace), 0, (void *) &trace_0)) {
						ZVAL_ADDREF(*args);
						add_assoc_zval(*trace_0, "args", *args);
					}
				}
			}
		}
	}
}

/* {{{ zval *http_exception_wrap(zval *, zval *, zend_class_entry *) */
zval *_http_exception_wrap(zval *old_exception, zval *new_exception, zend_class_entry *ce TSRMLS_DC)
{
	int inner = 1;
	char *message;
	zval *sub_exception, *tmp_exception;
	
	if (!new_exception) {
		MAKE_STD_ZVAL(new_exception);
		object_init_ex(new_exception, ce);
		
		zend_update_property(ce, new_exception, "innerException", lenof("innerException"), old_exception TSRMLS_CC);
		copy_bt_args(old_exception, new_exception TSRMLS_CC);
		
		sub_exception = old_exception;
		
		while ((sub_exception = zend_read_property(Z_OBJCE_P(sub_exception), sub_exception, "innerException", lenof("innerException"), 0 TSRMLS_CC)) && Z_TYPE_P(sub_exception) == IS_OBJECT) {
			++inner;
		}
		
		spprintf(&message, 0, "Exception caused by %d inner exception(s)", inner);
		zend_update_property_string(ZEND_EXCEPTION_GET_DEFAULT(), new_exception, "message", lenof("message"), message TSRMLS_CC);
		efree(message);
	} else {
		sub_exception = new_exception;
		tmp_exception = new_exception;
		
		while ((tmp_exception = zend_read_property(Z_OBJCE_P(tmp_exception), tmp_exception, "innerException", lenof("innerException"), 0 TSRMLS_CC)) && Z_TYPE_P(tmp_exception) == IS_OBJECT) {
			sub_exception = tmp_exception;
		}
		
		zend_update_property(Z_OBJCE_P(sub_exception), sub_exception, "innerException", lenof("innerException"), old_exception TSRMLS_CC);
		copy_bt_args(old_exception, new_exception TSRMLS_CC);
		copy_bt_args(old_exception, sub_exception TSRMLS_CC);
	}
	
	zval_ptr_dtor(&old_exception);
	return new_exception;
}
/* }}} */

/* {{{ STATUS http_object_new(zend_object_value *, const char *, uint, http_object_new_t, zend_class_entry *, void *, void **) */
STATUS _http_object_new(zend_object_value *ov, const char *cname_str, uint cname_len, http_object_new_t create, zend_class_entry *parent_ce, void *intern_ptr, void **obj_ptr TSRMLS_DC)
{
	zend_class_entry *ce = parent_ce;
	
	if (cname_str && cname_len) {
		if (!(ce = zend_fetch_class((char *) cname_str, cname_len, ZEND_FETCH_CLASS_DEFAULT TSRMLS_CC))) {
			return FAILURE;
		}
		if (!instanceof_function(ce, parent_ce TSRMLS_CC)) {
			http_error_ex(HE_WARNING, HTTP_E_RUNTIME, "Class %s does not extend %s", cname_str, parent_ce->name);
			return FAILURE;
		}
	}
	
	*ov = create(ce, intern_ptr, obj_ptr TSRMLS_CC);
	return SUCCESS;
}
/* }}} */
#endif /* ZEND_ENGINE_2 */

/* {{{ void http_log(char *, char *, char *) */
void _http_log_ex(char *file, const char *ident, const char *message TSRMLS_DC)
{
	time_t now;
	struct tm nowtm;
	char datetime[20] = {0};
	
	now = HTTP_G->request.time;
	strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", php_localtime_r(&now, &nowtm));

#define HTTP_LOG_WRITE(file, type, msg) \
	if (file && *file) { \
	 	php_stream *log = php_stream_open_wrapper_ex(file, "ab", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL, HTTP_DEFAULT_STREAM_CONTEXT); \
		 \
		if (log) { \
			php_stream_printf(log TSRMLS_CC, "%s\t[%s]\t%s\t<%s>%s", datetime, type, msg, SG(request_info).request_uri, PHP_EOL); \
			php_stream_close(log); \
		} \
	 \
	}
	
	HTTP_LOG_WRITE(file, ident, message);
	HTTP_LOG_WRITE(HTTP_G->log.composite, ident, message);
}
/* }}} */

static void http_ob_blackhole(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	*handled_output = ecalloc(1,1);
	*handled_output_len = 0;
}

/* {{{ STATUS http_exit(int, char*, char*) */
STATUS _http_exit_ex(int status, char *header, char *body, zend_bool send_header TSRMLS_DC)
{
	if (	(send_header && (SUCCESS != http_send_status_header(status, header))) ||
			(status && (SUCCESS != http_send_status(status)))) {
		http_error_ex(HE_WARNING, HTTP_E_HEADER, "Failed to exit with status/header: %d - %s", status, STR_PTR(header));
		STR_FREE(header);
		STR_FREE(body);
		return FAILURE;
	}
	
	if (!OG(ob_lock)) {
		php_end_ob_buffers(0 TSRMLS_CC);
	}
	if ((SUCCESS == sapi_send_headers(TSRMLS_C)) && body) {
		PHPWRITE(body, strlen(body));
	}
	
	switch (status) {
		case 301:	http_log(HTTP_G->log.redirect, "301-REDIRECT", header);			break;
		case 302:	http_log(HTTP_G->log.redirect, "302-REDIRECT", header);			break;
		case 303:	http_log(HTTP_G->log.redirect, "303-REDIRECT", header);			break;
		case 305:	http_log(HTTP_G->log.redirect, "305-REDIRECT", header);			break;
		case 307:	http_log(HTTP_G->log.redirect, "307-REDIRECT", header);			break;
		case 304:	http_log(HTTP_G->log.cache, "304-CACHE", header);				break;
		case 404:	http_log(HTTP_G->log.not_found, "404-NOTFOUND", NULL);			break;
		case 405:	http_log(HTTP_G->log.allowed_methods, "405-ALLOWED", header);	break;
		default:	http_log(NULL, header, body);									break;
	}
	
	STR_FREE(header);
	STR_FREE(body);
	
	if (HTTP_G->force_exit) {
		zend_bailout();
	} else {
		php_ob_set_internal_handler(http_ob_blackhole, 4096, "blackhole", 0 TSRMLS_CC);
	}
	
	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_check_method(char *) */
STATUS _http_check_method_ex(const char *method, const char *methods)
{
	const char *found;

	if (	(found = strstr(methods, method)) &&
			(found == method || !HTTP_IS_CTYPE(alpha, found[-1])) &&
			(strlen(found) >= strlen(method) && !HTTP_IS_CTYPE(alpha, found[strlen(method)]))) {
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */

/* {{{ zval *http_get_server_var_ex(char *, size_t) */
PHP_HTTP_API zval *_http_get_server_var_ex(const char *key, size_t key_len, zend_bool check TSRMLS_DC)
{
	zval **hsv, **var;
	char *env;
	
	/* if available, this is a lot faster than accessing $_SERVER */
	if (sapi_module.getenv) {
		if ((!(env = sapi_module.getenv((char *) key, key_len TSRMLS_CC))) || (check && !*env)) {
			return NULL;
		}
		if (HTTP_G->server_var) {
			zval_ptr_dtor(&HTTP_G->server_var);
		}
		MAKE_STD_ZVAL(HTTP_G->server_var);
		ZVAL_STRING(HTTP_G->server_var, env, 1);
		return HTTP_G->server_var;
	}
	
#ifdef ZEND_ENGINE_2
	zend_is_auto_global("_SERVER", lenof("_SERVER") TSRMLS_CC);
#endif
	
	if ((SUCCESS != zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void *) &hsv)) || (Z_TYPE_PP(hsv) != IS_ARRAY)) {
		return NULL;
	}
	if ((SUCCESS != zend_hash_find(Z_ARRVAL_PP(hsv), (char *) key, key_len + 1, (void *) &var))) {
		return NULL;
	}
	if (check && !((Z_TYPE_PP(var) == IS_STRING) && Z_STRVAL_PP(var) && Z_STRLEN_PP(var))) {
		return NULL;
	}
	return *var;
}
/* }}} */

/* {{{ STATUS http_get_request_body(char **, size_t *) */
PHP_HTTP_API STATUS _http_get_request_body_ex(char **body, size_t *length, zend_bool dup TSRMLS_DC)
{
	*length = 0;
	*body = NULL;
	
	if (SG(request_info).raw_post_data) {
		*length = SG(request_info).raw_post_data_length;
		*body = SG(request_info).raw_post_data;
		
		if (dup) {
			*body = estrndup(*body, *length);
		}
		return SUCCESS;
	} else if (sapi_module.read_post && !HTTP_G->read_post_data) {
		char buf[4096];
		int len;
		
		HTTP_G->read_post_data = 1;
		
		while (0 < (len = sapi_module.read_post(buf, sizeof(buf) TSRMLS_CC))) {
			*body = erealloc(*body, *length + len + 1);
			memcpy(*body + *length, buf, len);
			*length += len;
			(*body)[*length] = '\0';
		}
		
		/* check for error */
		if (len < 0) {
			STR_FREE(*body);
			*length = 0;
			return FAILURE;
		}
		
		SG(request_info).raw_post_data = *body;
		SG(request_info).raw_post_data_length = *length;
		
		if (dup) {
			*body = estrndup(*body, *length);
		}
		return SUCCESS;
	}
	
	return FAILURE;
}
/* }}} */

/* {{{ php_stream *http_get_request_body_stream(void) */
PHP_HTTP_API php_stream *_http_get_request_body_stream(TSRMLS_D)
{
	php_stream *s = NULL;
	
	if (SG(request_info).raw_post_data) {
		s = php_stream_open_wrapper("php://input", "rb", 0, NULL);
	} else if (sapi_module.read_post && !HTTP_G->read_post_data) {
		HTTP_G->read_post_data = 1;
		
		if ((s = php_stream_temp_new())) {
			char buf[4096];
			int len;
			
			while (0 < (len = sapi_module.read_post(buf, sizeof(buf) TSRMLS_CC))) {
				php_stream_write(s, buf, len);
			}
			
			if (len < 0) {
				php_stream_close(s);
				s = NULL;
			} else {
				php_stream_rewind(s);
			}
		}
	}
	
	return s;
}
/* }}} */

/* {{{ void http_parse_params_default_callback(...) */
PHP_HTTP_API void _http_parse_params_default_callback(void *arg, const char *key, int keylen, const char *val, int vallen TSRMLS_DC)
{
	char *kdup;
	zval tmp, *entry;
	HashTable *ht = (HashTable *) arg;
	
	if (ht) {
		INIT_ZARR(tmp, ht);
		
		if (vallen) {
			MAKE_STD_ZVAL(entry);
			array_init(entry);
			if (keylen) {
				kdup = estrndup(key, keylen);
				add_assoc_stringl_ex(entry, kdup, keylen + 1, (char *) val, vallen, 1);
				efree(kdup);
			} else {
				add_next_index_stringl(entry, (char *) val, vallen, 1);
			}
			add_next_index_zval(&tmp, entry);
		} else {
			add_next_index_stringl(&tmp, (char *) key, keylen, 1);
		}
	}
}
/* }}} */

/* {{{ STATUS http_parse_params(const char *, HashTable *) */
PHP_HTTP_API STATUS _http_parse_params_ex(const char *param, int flags, http_parse_params_callback cb, void *cb_arg TSRMLS_DC)
{
#define ST_QUOTE	1
#define ST_VALUE	2
#define ST_KEY		3
#define ST_ASSIGN	4
#define ST_ADD		5
	
	int st = ST_KEY, keylen = 0, vallen = 0;
	char *s, *c, *key = NULL, *val = NULL;
	
	for(c = s = estrdup(param);;) {
	continued:
#if 0
	{
		char *tk = NULL, *tv = NULL;
		
		if (key) {
			if (keylen) {
				tk= estrndup(key, keylen);
			} else {
				tk = ecalloc(1, 7);
				memcpy(tk, key, 3);
				tk[3]='.'; tk[4]='.'; tk[5]='.';
			}
		}
		if (val) {
			if (vallen) {
				tv = estrndup(val, vallen);
			} else {
				tv = ecalloc(1, 7);
				memcpy(tv, val, 3);
				tv[3]='.'; tv[4]='.'; tv[5]='.';
			}
		}
		fprintf(stderr, "[%6s] %c \"%s=%s\"\n",
				(
						st == ST_QUOTE ? "QUOTE" :
						st == ST_VALUE ? "VALUE" :
						st == ST_KEY ? "KEY" :
						st == ST_ASSIGN ? "ASSIGN" :
						st == ST_ADD ? "ADD":
						"HUH?"
				), *c?*c:'0', tk, tv
		);
		STR_FREE(tk); STR_FREE(tv);
	}
#endif
		switch (st) {
			case ST_QUOTE:
			quote:
				if (*c == '"') {
					if (*(c-1) == '\\') {
						memmove(c-1, c, strlen(c)+1);
						goto quote;
					} else {
						goto add;
					}
				} else {
					if (!val) {
						val = c;
					}
					if (!*c) {
						--val;
						st = ST_ADD;
					}
				}
				break;
				
			case ST_VALUE:
				switch (*c) {
					case '"':
						if (!val) {
							st = ST_QUOTE;
						}
						break;
					
					case ' ':
						break;
					
					case ';':
					case '\0':
						goto add;
						break;
					case ',':
						if (flags & HTTP_PARAMS_ALLOW_COMMA) {
							goto add;
						}
					default:
						if (!val) {
							val = c;
						}
						break;
				}
				break;
				
			case ST_KEY:
				switch (*c) {
					case ',':
						if (flags & HTTP_PARAMS_ALLOW_COMMA) {
							goto allow_comma;
						}
					case '\r':
					case '\n':
					case '\t':
					case '\013':
					case '\014':
						goto failure;
						break;
					
					case '=':
						if (key) {
							keylen = c - key;
							st = ST_VALUE;
						} else {
							goto failure;
						}
						break;
					
					case ' ':
						if (key) {
							keylen = c - key;
							st = ST_ASSIGN;
						}
						break;
					
					case ';':
					case '\0':
					allow_comma:
						if (key) {
							keylen = c-- - key;
							st = ST_ADD;
						}
						break;
					
					default:
						if (!key) {
							key = c;
						}
						break;
				}
				break;
				
			case ST_ASSIGN:
				if (*c == '=') {
					st = ST_VALUE;
				} else if (!*c || *c == ';' || ((flags & HTTP_PARAMS_ALLOW_COMMA) && *c == ',')) {
					st = ST_ADD;
				} else if (*c != ' ') {
					goto failure;
				}
				break;
				
			case ST_ADD:
			add:
				if (val) {
					vallen = c - val;
					if (st != ST_QUOTE) {
						while (val[vallen-1] == ' ') --vallen;
					}
				} else {
					val = "";
					vallen = 0;
				}
				
				cb(cb_arg, key, keylen, val, vallen TSRMLS_CC);
				
				st = ST_KEY;
				key = val = NULL;
				keylen = vallen = 0;
				break;
		}
		if (*c) {
			++c;
		} else if (st == ST_ADD) {
			goto add;
		} else {
			break;
		}
	}
	
	efree(s);
	return SUCCESS;
	
failure:
	if (flags & HTTP_PARAMS_RAISE_ERROR) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Unexpected character (%c) at pos %tu of %zu", *c, c-s, strlen(s));
	}
	if (flags & HTTP_PARAMS_ALLOW_FAILURE) {
		if (st == ST_KEY) {
			if (key) {
				keylen = c - key;
			} else {
				key = c;
			}
		} else {
			--c;
		}
		st = ST_ADD;
		goto continued;
	}
	efree(s);
	return FAILURE;
}
/* }}} */

/* {{{ array_join */
int apply_array_append_func(void *pDest, int num_args, va_list args, zend_hash_key *hash_key)
{
	int flags;
	char *key = NULL;
	HashTable *dst;
	zval **data = NULL, **value = (zval **) pDest;
	
	dst = va_arg(args, HashTable *);
	flags = va_arg(args, int);
	
	if ((!(flags & ARRAY_JOIN_STRONLY)) || hash_key->nKeyLength) {
		if ((flags & ARRAY_JOIN_PRETTIFY) && hash_key->nKeyLength) {
			key = pretty_key(estrndup(hash_key->arKey, hash_key->nKeyLength - 1), hash_key->nKeyLength - 1, 1, 1);
			zend_hash_find(dst, key, hash_key->nKeyLength, (void *) &data);
		} else {
			zend_hash_quick_find(dst, hash_key->arKey, hash_key->nKeyLength, hash_key->h, (void *) &data);
		}
		
		ZVAL_ADDREF(*value);
		if (data) {
			if (Z_TYPE_PP(data) != IS_ARRAY) {
				convert_to_array(*data);
			}
			add_next_index_zval(*data, *value);
		} else if (key) {
			zend_hash_add(dst, key, hash_key->nKeyLength, value, sizeof(zval *), NULL);
		} else {
			zend_hash_quick_add(dst, hash_key->arKey, hash_key->nKeyLength, hash_key->h, value, sizeof(zval *), NULL);
		}
		
		if (key) {
			efree(key);
		}
	}
	
	return ZEND_HASH_APPLY_KEEP;
}

int apply_array_merge_func(void *pDest, int num_args, va_list args, zend_hash_key *hash_key)
{
	int flags;
	char *key = NULL;
	HashTable *dst;
	zval **value = (zval **) pDest;
	
	dst = va_arg(args, HashTable *);
	flags = va_arg(args, int);
	
	if ((!(flags & ARRAY_JOIN_STRONLY)) || hash_key->nKeyLength) {
		ZVAL_ADDREF(*value);
		if ((flags & ARRAY_JOIN_PRETTIFY) && hash_key->nKeyLength) {
			key = pretty_key(estrndup(hash_key->arKey, hash_key->nKeyLength - 1), hash_key->nKeyLength - 1, 1, 1);
			zend_hash_update(dst, key, hash_key->nKeyLength, (void *) value, sizeof(zval *), NULL);
			efree(key);
		} else {
			zend_hash_quick_update(dst, hash_key->arKey, hash_key->nKeyLength, hash_key->h, (void *) value, sizeof(zval *), NULL);
		}
	}
	
	return ZEND_HASH_APPLY_KEEP;
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

