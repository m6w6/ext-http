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

/* $Id $ */

#include "php_http.h"

#include <main/SAPI.h>
#include <ext/date/php_date.h>
#include <ext/standard/php_string.h>

PHP_RINIT_FUNCTION(http_env)
{
	PHP_HTTP_G->env.response.last_modified = 0;
	PHP_HTTP_G->env.response.throttle_chunk = 0;
	PHP_HTTP_G->env.response.throttle_delay = 0;
	PHP_HTTP_G->env.request.time = sapi_get_request_time(TSRMLS_C);

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_env)
{
	if (PHP_HTTP_G->env.request.headers) {
		zend_hash_destroy(PHP_HTTP_G->env.request.headers);
		FREE_HASHTABLE(PHP_HTTP_G->env.request.headers);
		PHP_HTTP_G->env.request.headers = NULL;
	}
	if (PHP_HTTP_G->env.request.body) {
		php_http_message_body_free(&PHP_HTTP_G->env.request.body);
	}
	if (PHP_HTTP_G->env.response.body) {
		php_http_message_body_free(&PHP_HTTP_G->env.response.body);
	}
	STR_SET(PHP_HTTP_G->env.response.content_type, NULL);
	STR_SET(PHP_HTTP_G->env.response.etag, NULL);

	if (PHP_HTTP_G->env.server_var) {
		zval_ptr_dtor(&PHP_HTTP_G->env.server_var);
		PHP_HTTP_G->env.server_var = NULL;
	}

	return SUCCESS;
}

PHP_HTTP_API void php_http_env_get_request_headers(HashTable *headers TSRMLS_DC)
{
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **hsv, **header;
	HashPosition pos;

	if (!PHP_HTTP_G->env.request.headers) {
		ALLOC_HASHTABLE(PHP_HTTP_G->env.request.headers);
		zend_hash_init(PHP_HTTP_G->env.request.headers, 0, NULL, ZVAL_PTR_DTOR, 0);

		zend_is_auto_global("_SERVER", lenof("_SERVER") TSRMLS_CC);

		if (SUCCESS == zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void *) &hsv) && Z_TYPE_PP(hsv) == IS_ARRAY) {
			FOREACH_KEY(pos, *hsv, key) {
				if (key.type == HASH_KEY_IS_STRING && key.len > 6 && !strncmp(key.str, "HTTP_", 5)) {
					key.len -= 5;
					key.str = php_http_pretty_key(estrndup(key.str + 5, key.len - 1), key.len - 1, 1, 1);

					zend_hash_get_current_data_ex(Z_ARRVAL_PP(hsv), (void *) &header, &pos);
					Z_ADDREF_P(*header);
					zend_hash_add(PHP_HTTP_G->env.request.headers, key.str, key.len, (void *) header, sizeof(zval *), NULL);

					efree(key.str);
				}
			}
		}
	}

	if (headers) {
		zend_hash_copy(headers, PHP_HTTP_G->env.request.headers, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	}
}

PHP_HTTP_API char *php_http_env_get_request_header(const char *name_str, size_t name_len TSRMLS_DC)
{
	zval **zvalue;
	char *val = NULL, *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

	php_http_env_get_request_headers(NULL TSRMLS_CC);

	if (SUCCESS == zend_hash_find(PHP_HTTP_G->env.request.headers, key, name_len + 1, (void *) &zvalue)) {
		zval *zcopy = php_http_zsep(IS_STRING, *zvalue);

		val = estrndup(Z_STRVAL_P(zcopy), Z_STRLEN_P(zcopy));
		zval_ptr_dtor(&zcopy);
	}

	efree(key);

	return val;
}

PHP_HTTP_API int php_http_env_got_request_header(const char *name_str, size_t name_len TSRMLS_DC)
{
	char *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);
	int got;

	php_http_env_get_request_headers(NULL TSRMLS_CC);
	got = zend_hash_exists(PHP_HTTP_G->env.request.headers, key, name_len + 1);
	efree(key);

	return got;
}

PHP_HTTP_API zval *php_http_env_get_server_var(const char *key, size_t key_len, zend_bool check TSRMLS_DC)
{
	zval **hsv, **var;
	char *env;

	/* if available, this is a lot faster than accessing $_SERVER */
	if (sapi_module.getenv) {
		if ((!(env = sapi_module.getenv((char *) key, key_len TSRMLS_CC))) || (check && !*env)) {
			return NULL;
		}
		if (PHP_HTTP_G->env.server_var) {
			zval_ptr_dtor(&PHP_HTTP_G->env.server_var);
		}
		MAKE_STD_ZVAL(PHP_HTTP_G->env.server_var);
		ZVAL_STRING(PHP_HTTP_G->env.server_var, env, 1);
		return PHP_HTTP_G->env.server_var;
	}

	zend_is_auto_global(ZEND_STRL("_SERVER") TSRMLS_CC);

	if ((SUCCESS != zend_hash_find(&EG(symbol_table), ZEND_STRS("_SERVER"), (void *) &hsv)) || (Z_TYPE_PP(hsv) != IS_ARRAY)) {
		return NULL;
	}
	if ((SUCCESS != zend_hash_find(Z_ARRVAL_PP(hsv), key, key_len + 1, (void *) &var))) {
		return NULL;
	}
	if (check && !((Z_TYPE_PP(var) == IS_STRING) && Z_STRVAL_PP(var) && Z_STRLEN_PP(var))) {
		return NULL;
	}
	return *var;
}

PHP_HTTP_API php_http_message_body_t *php_http_env_get_request_body(TSRMLS_D)
{
	if (!PHP_HTTP_G->env.request.body) {
		php_stream *s = NULL;

		if (SG(request_info).post_data || SG(request_info).raw_post_data) {
			if ((s = php_stream_temp_new())) {
				/* php://input does not support seek() */
				if (SG(request_info).raw_post_data) {
					php_stream_write(s, SG(request_info).raw_post_data, SG(request_info).raw_post_data_length);
				} else {
					php_stream_write(s, SG(request_info).post_data, SG(request_info).post_data_length);
				}
				php_stream_rewind(s);
			}
		} else if (sapi_module.read_post) {
			if ((s = php_stream_temp_new())) {
				char *buf = emalloc(4096);
				int len;

				while (0 < (len = sapi_module.read_post(buf, 4096 TSRMLS_CC))) {
					php_stream_write(s, buf, len);

					if (len < 4096) {
						break;
					}
				}
				efree(buf);

				php_stream_rewind(s);
			}
		}
		PHP_HTTP_G->env.request.body = php_http_message_body_init(NULL, s TSRMLS_CC);
	}

	return PHP_HTTP_G->env.request.body;
}

PHP_HTTP_API php_http_range_status_t php_http_env_get_request_ranges(HashTable *ranges, size_t length TSRMLS_DC)
{
	zval *zentry;
	char *range, *rp, c;
	long begin = -1, end = -1, *ptr;

	if (!(range = php_http_env_get_request_header(ZEND_STRL("Range") TSRMLS_CC))) {
		return PHP_HTTP_RANGE_NO;
	}
	if (strncmp(range, "bytes=", lenof("bytes="))) {
		STR_FREE(range);
		return PHP_HTTP_RANGE_NO;
	}

	rp  = range + lenof("bytes=");
	ptr = &begin;

	do {
		switch (c = *(rp++)) {
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
				break;

			case 0:
			case ',':

				if (length) {
					/* validate ranges */
					switch (begin) {
						/* "0-12345" */
						case -10:
							switch (end) {
								/* "0-" */
								case -1:
									STR_FREE(range);
									return PHP_HTTP_RANGE_NO;

								/* "0-0" */
								case -10:
									end = 0;
									break;

								default:
									if (length <= (size_t) end) {
										end = length - 1;
									}
									break;
							}
							begin = 0;
							break;

						/* "-12345" */
						case -1:
							/* "-", "-0" */
							if (end == -1 || end == -10) {
								STR_FREE(range);
								return PHP_HTTP_RANGE_ERR;
							}
							begin = length - end;
							end = length - 1;
							break;

						/* "12345-(NNN)" */
						default:
							if (length <= (size_t) begin) {
								STR_FREE(range);
								return PHP_HTTP_RANGE_ERR;
							}
							switch (end) {
								/* "12345-0" */
								case -10:
									STR_FREE(range);
									return PHP_HTTP_RANGE_ERR;

								/* "12345-" */
								case -1:
									end = length - 1;
									break;

								/* "12345-67890" */
								default:
									if (length <= (size_t) end) {
										end = length - 1;
									} else if (end <  begin) {
										STR_FREE(range);
										return PHP_HTTP_RANGE_ERR;
									}
									break;
							}
							break;
					}
				}

				MAKE_STD_ZVAL(zentry);
				array_init(zentry);
				add_index_long(zentry, 0, begin);
				add_index_long(zentry, 1, end);
				zend_hash_next_index_insert(ranges, &zentry, sizeof(zval *), NULL);

				begin = -1;
				end = -1;
				ptr = &begin;

				break;

			default:
				STR_FREE(range);
				return PHP_HTTP_RANGE_NO;
		}
	} while (c != 0);

	STR_FREE(range);
	return PHP_HTTP_RANGE_OK;
}

static void grab_headers(void *data, void *arg TSRMLS_DC)
{
	php_http_buffer_appendl(PHP_HTTP_BUFFER(arg), ((sapi_header_struct *)data)->header);
	php_http_buffer_appends(PHP_HTTP_BUFFER(arg), PHP_HTTP_CRLF);
}

PHP_HTTP_API STATUS php_http_env_get_response_headers(HashTable *headers_ht TSRMLS_DC)
{
	STATUS status;
	php_http_buffer_t headers;

	php_http_buffer_init(&headers);
	zend_llist_apply_with_argument(&SG(sapi_headers).headers, grab_headers, &headers TSRMLS_CC);
	php_http_buffer_fix(&headers);

	status = php_http_headers_parse(PHP_HTTP_BUFFER_VAL(&headers), PHP_HTTP_BUFFER_LEN(&headers), headers_ht, NULL, NULL TSRMLS_CC);
	php_http_buffer_dtor(&headers);

	return status;
}

PHP_HTTP_API char *php_http_env_get_response_header(const char *name_str, size_t name_len TSRMLS_DC)
{
	char *val = NULL;
	HashTable headers;

	zend_hash_init(&headers, 0, NULL, NULL, 0);
	if (SUCCESS == php_http_env_get_response_headers(&headers TSRMLS_CC)) {
		zval **zvalue;
		char *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

		if (SUCCESS == zend_hash_find(&headers, key, name_len + 1, (void *) &zvalue)) {
			zval *zcopy = php_http_zsep(IS_STRING, *zvalue);

			val = estrndup(Z_STRVAL_P(zcopy), Z_STRLEN_P(zcopy));
			zval_ptr_dtor(&zcopy);
		}

		efree(key);
	}
	zend_hash_destroy(&headers);

	return val;
}

PHP_HTTP_API long php_http_env_get_response_code(TSRMLS_D)
{
	long code = SG(sapi_headers).http_response_code;
	return code ? code : 200;
}

PHP_HTTP_API STATUS php_http_env_set_response_code(long http_code TSRMLS_DC)
{
	return sapi_header_op(SAPI_HEADER_SET_STATUS, (void *) http_code TSRMLS_CC);
}

PHP_HTTP_API STATUS php_http_env_set_response_status_line(long code, php_http_version_t *v TSRMLS_DC)
{
	sapi_header_line h = {0};
	STATUS ret;

	h.line_len = spprintf(&h.line, 0, "HTTP/%u.%u %ld %s", v->major, v->minor, code, php_http_env_get_response_status_for_code(code));
	ret = sapi_header_op(SAPI_HEADER_REPLACE, (void *) &h TSRMLS_CC);
	efree(h.line);

	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_protocol_version(php_http_version_t *v TSRMLS_DC)
{
	return php_http_env_set_response_status_line(php_http_env_get_response_code(TSRMLS_C), v TSRMLS_CC);
}

PHP_HTTP_API STATUS php_http_env_set_response_header(long http_code, const char *header_str, size_t header_len, zend_bool replace TSRMLS_DC)
{
	sapi_header_line h = {estrndup(header_str, header_len), header_len, http_code};
	STATUS ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h TSRMLS_CC);
	efree(h.line);
	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_header_value(long http_code, const char *name_str, size_t name_len, zval *value, zend_bool replace TSRMLS_DC)
{
	if (!value) {
		sapi_header_line h = {(char *) name_str, name_len, http_code};

		return sapi_header_op(SAPI_HEADER_DELETE, (void *) &h TSRMLS_CC);
	}

	if(Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) {
		HashPosition pos;
		int first = replace;
		zval **data_ptr;

		FOREACH_HASH_VAL(pos, HASH_OF(value), data_ptr) {
			if (SUCCESS != php_http_env_set_response_header_value(http_code, name_str, name_len, *data_ptr, first TSRMLS_CC)) {
				return FAILURE;
			}
			first = 0;
		}

		return SUCCESS;
	} else {
		zval *data = php_http_zsep(IS_STRING, value);

		if (!Z_STRLEN_P(data)) {
			zval_ptr_dtor(&data);
			return php_http_env_set_response_header_value(http_code, name_str, name_len, NULL, replace TSRMLS_CC);
		} else {
			sapi_header_line h;
			STATUS ret;

			if (name_len > INT_MAX) {
				name_len = INT_MAX;
			}
			h.response_code = http_code;
			h.line_len = spprintf(&h.line, 0, "%.*s: %.*s", (int) name_len, name_str, Z_STRLEN_P(data), Z_STRVAL_P(data));

			ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h TSRMLS_CC);

			zval_ptr_dtor(&data);
			STR_FREE(h.line);

			return ret;
		}
	}
}

PHP_HTTP_API void php_http_env_set_response_throttle_rate(zval *container, size_t chunk_size, double delay TSRMLS_CC)
{
	if (Z_TYPE_P(container) == IS_OBJECT) {
		zend_update_property_double(Z_OBJCE_P(container), container, ZEND_STRL("throttleDelay"), delay TSRMLS_CC);
		zend_update_property_long(Z_OBJCE_P(container), container, ZEND_STRL("throttleChunk"), chunk_size TSRMLS_CC);
	} else {
		convert_to_array(container);
		add_assoc_double_ex(container, ZEND_STRS("throttleDelay"), delay);
		add_assoc_long_ex(container, ZEND_STRS("throttleChunk"), chunk_size);
	}
}

static void set_container_value(zval *container, const char *name_str, size_t name_len, int type, const void *value_ptr, size_t value_len TSRMLS_DC)
{
	if (Z_TYPE_P(container) == IS_OBJECT) {
		/* stupid non-const api */
		char *name = estrndup(name_str, name_len);
		switch (type) {
			case IS_LONG:
				zend_update_property_long(Z_OBJCE_P(container), container, name, name_len, *(long *)value_ptr TSRMLS_CC);
				break;
			case IS_STRING:
				zend_update_property_stringl(Z_OBJCE_P(container), container, name, name_len, value_ptr, value_len TSRMLS_CC);
				break;
		}
		efree(name);
	} else {
		convert_to_array(container);
		switch (type) {
			case IS_LONG:
				add_assoc_long_ex(container, name_str, name_len + 1, *(long *)value_ptr);
				break;
			case IS_STRING: {
				char *value = estrndup(value_ptr, value_len);
				add_assoc_stringl_ex(container, name_str, name_len + 1, value, value_len, 0);
				break;
			}
		}
	}
}

PHP_HTTP_API STATUS php_http_env_set_response_last_modified(zval *container, time_t t, char **sent_header TSRMLS_DC)
{
	STATUS ret;
	char *lm_header_str, *date;
	size_t lm_header_len;

	if (t) {
		if (!(date = php_format_date(ZEND_STRL(PHP_HTTP_DATE_FORMAT), t, 0 TSRMLS_CC))) {
			return FAILURE;
		}

		lm_header_len = spprintf(&lm_header_str, 0, "Last-Modified: %s", date);
		STR_FREE(date);
	} else {
		lm_header_str = "Last-Modified:";
		lm_header_len = lenof("Last-Modified:");
	}

	if (SUCCESS == (ret = php_http_env_set_response_header(0, lm_header_str, lm_header_len, 1 TSRMLS_CC))) {
		set_container_value(container, ZEND_STRL("lastModified"), IS_LONG, &t, 0 TSRMLS_CC);
	}

	if (sent_header) {
		*sent_header = lm_header_str;
	} else if (t) {
		STR_FREE(lm_header_str);
	}

	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_etag(zval *container, const char *etag_str, size_t etag_len, char **sent_header TSRMLS_DC)
{
	STATUS ret;
	char *etag = NULL, *etag_header_str;
	size_t etag_header_len;

	if (etag_len){
		etag_header_len = spprintf(&etag_header_str, 0, "ETag: \"%s\"", etag_str);
	} else {
		etag_header_str = "ETag:";
		etag_header_len = lenof("ETag:");
	}

	if (SUCCESS == (ret = php_http_env_set_response_header(0, etag_header_str, etag_header_len, 1 TSRMLS_CC))) {
		set_container_value(container, ZEND_STRL(etag), IS_STRING, etag_str, etag_len TSRMLS_CC);
	}

	if (sent_header) {
		*sent_header = etag_header_str;
	} else if (etag_len) {
		STR_FREE(etag_header_str);
	}

	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_content_type(zval *container, const char *ct_str, size_t ct_len, char **sent_header TSRMLS_DC)
{
	STATUS ret;
	char *ct_header_str;
	size_t ct_header_len;

	if (ct_len) {
		PHP_HTTP_CHECK_CONTENT_TYPE(ct_str, return FAILURE);
		ct_header_len = spprintf(&ct_header_str, 0, "Content-Type: %s", ct_str);
	} else {
		ct_header_str = "Content-Type:";
		ct_header_len = lenof("Content-Type:");
	}

	if (SUCCESS == (ret = php_http_env_set_response_header(0, ct_header_str, ct_header_len, 1 TSRMLS_CC))) {
		set_container_value(container, ZEND_STRL("contentType"), IS_STRING, ct_str, ct_len TSRMLS_CC);
	}

	if (sent_header) {
		*sent_header = ct_header_str;
	} else if (ct_len) {
		STR_FREE(ct_header_str);
	}

	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_content_disposition(zval *container, php_http_content_disposition_t d, const char *f_str, size_t f_len, char **sent_header TSRMLS_DC)
{
	STATUS ret;
	char *tmp, *cd_header_str, *new_f_str;
	int new_f_len;
	size_t cd_header_len;

	switch (d) {
		case PHP_HTTP_CONTENT_DISPOSITION_NONE:
			break;
		case PHP_HTTP_CONTENT_DISPOSITION_INLINE:
			tmp = "inline";
			break;
		case PHP_HTTP_CONTENT_DISPOSITION_ATTACHMENT:
			tmp = "attachment";
			break;
		default:
			php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Unknown content disposition (%d)", (int) d);
			return FAILURE;
	}

	if (f_len) {
		new_f_str = php_addslashes(estrndup(f_str, f_len), f_len, &new_f_len, 0 TSRMLS_CC);
		cd_header_len = spprintf(&cd_header_str, 0, "Content-Disposition: %s; filename=\"%.*s\"", tmp, new_f_len, new_f_str);
		STR_FREE(new_f_str);
	} else if (d) {
		cd_header_len = spprintf(&cd_header_str, 0, "Content-Disposition: %s", tmp);
	} else {
		cd_header_str = "Content-Disposition:";
		cd_header_len = lenof("Content-Disposition:");
	}
	
	ret = php_http_env_set_response_header(0, cd_header_str, cd_header_len, 1 TSRMLS_CC);

	if (sent_header) {
		*sent_header = cd_header_str;
	} else if (f_len || d){
		STR_FREE(cd_header_str);
	}

	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_cache_control(zval *container, const char *cc_str, size_t cc_len, char **sent_header TSRMLS_DC)
{
	STATUS ret;
	char *cc_header_str;
	size_t cc_header_len;

	if (cc_len) {
		cc_header_len = spprintf(&cc_header_str, 0, "Cache-Control: %s", cc_str);
	} else {
		cc_header_str = "Content-Disposition:";
		cc_header_len = lenof("Content-Disposition:");
	}

	ret = php_http_env_set_response_header(0, cc_header_str, cc_header_len, 1 TSRMLS_CC);

	if (sent_header) {
		*sent_header = cc_header_str;
	} else if (cc_len) {
		STR_FREE(cc_header_str);
	}

	return ret;
}

static zval *get_container_value(zval *container, const char *name_str, size_t name_len TSRMLS_CC)
{
	zval *val, **valptr;

	if (Z_TYPE_P(container) == IS_OBJECT) {
		char *name = estrndup(name_str, name_len);
		val = zend_read_property(Z_OBJCE_P(container), container, name, name_len, 0 TSRMLS_CC);
		efree(name);
	} else {
		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(container), name_str, name_len + 1, (void *) &valptr)) {
			val = *valptr;
		} else {
			val = NULL;
		}
	}
	if (val) {
		Z_ADDREF_P(val);
	}
	return val;
}

PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_etag(zval *container, const char *header_str, size_t header_len TSRMLS_DC)
{
	int ret, free_etag = 0;
	char *header, *etag;
	zval *zetag, *zbody = NULL;

	if (	!(header = php_http_env_get_request_header(header_str, header_len TSRMLS_CC))
	||		!(zbody = get_container_value(container, ZEND_STRL("body") TSRMLS_CC))
	|| 		!(Z_TYPE_P(zbody) == IS_OBJECT)
	||		!instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_class_entry TSRMLS_CC)
	) {
		STR_FREE(header);
		if (zbody) {
			zval_ptr_dtor(&zbody);
		}
		return PHP_HTTP_CACHE_NO;
	}

	if ((zetag = get_container_value(container, ZEND_STRL("etag") TSRMLS_CC))) {
		zval *zetag_copy = php_http_zsep(IS_STRING, zetag);
		zval_ptr_dtor(&zetag);
		zetag = zetag_copy;
	}

	if (zetag && Z_STRLEN_P(zetag)) {
		etag = Z_STRVAL_P(zetag);
	} else {
		etag = php_http_message_body_etag(((php_http_message_body_object_t *) zend_object_store_get_object(zbody TSRMLS_CC))->body);
		php_http_env_set_response_etag(container, etag, strlen(etag), NULL TSRMLS_CC);
		free_etag = 1;
	}

	if (zetag) {
		zval_ptr_dtor(&zetag);
	}

	ret = php_http_match(header, etag, PHP_HTTP_MATCH_WORD);

	if (free_etag) {
		efree(etag);
	}
	efree(header);

	return ret ? PHP_HTTP_CACHE_HIT : PHP_HTTP_CACHE_MISS;
}

PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_last_modified(zval *container, const char *header_str, size_t header_len TSRMLS_DC)
{
	char *header;
	time_t ums, lm = 0;
	zval *zbody = NULL, *zlm;

	if (	!(header = php_http_env_get_request_header(header_str, header_len TSRMLS_CC))
	||		!(zbody = get_container_value(container, ZEND_STRL("body") TSRMLS_CC))
	||		!(Z_TYPE_P(zbody) == IS_OBJECT)
	||		!instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_class_entry TSRMLS_CC)
	) {
		STR_FREE(header);
		if (zbody) {
			zval_ptr_dtor(&zbody);
		}
		return PHP_HTTP_CACHE_NO;
	}

	if ((zlm = get_container_value(container, ZEND_STRL("lastModified") TSRMLS_CC))) {
		zval *zlm_copy = php_http_zsep(IS_LONG, zlm);
		zval_ptr_dtor(&zlm);
		zlm = zlm_copy;
	}

	if (zlm && Z_LVAL_P(zlm) > 0) {
		lm = Z_LVAL_P(zlm);
	} else {
		lm = php_http_message_body_mtime(((php_http_message_body_object_t *) zend_object_store_get_object(zbody TSRMLS_CC))->body);
		php_http_env_set_response_last_modified(container, lm, NULL TSRMLS_CC);
	}

	if (zlm) {
		zval_ptr_dtor(&zlm);
	}

	ums = php_parse_date(header, NULL TSRMLS_CC);
	efree(header);

	if (ums > 0 && ums <= lm) {
		return PHP_HTTP_CACHE_HIT;
	} else {
		return PHP_HTTP_CACHE_MISS;
	}
}

PHP_HTTP_API void php_http_env_set_response_body(zval *container, php_http_message_body_t *body)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	zend_object_value ov = php_http_message_body_object_new_ex(php_http_message_body_class_entry, php_http_message_body_copy(body, NULL, 0), NULL TSRMLS_CC);

	set_container_value(container, ZEND_STRL("body"), IS_OBJECT, &ov, 0 TSRMLS_CC);
}

struct output_ctx {
	php_http_buffer_t *buf;
	zval *container;
};

static size_t output(void *context, const char *buf, size_t len TSRMLS_DC)
{
	struct output_ctx *ctx = context;

	if (ctx->buf) {
		zval *zcs;
		size_t chunk_size = PHP_HTTP_SENDBUF_SIZE;

		if ((zcs = get_container_value(ctx->container, ZEND_STRL("throttleChunk") TSRMLS_CC))) {
			zval *zcs_copy = php_http_zsep(IS_LONG, zcs);

			zval_ptr_dtor(&zcs);
			chunk_size = Z_LVAL_P(zcs_copy);
			zval_ptr_dtor(&zcs_copy);
		}
		php_http_buffer_chunked_output(&ctx->buf, buf, len, buf ? chunk_size : 0, output, NULL TSRMLS_CC);
	} else {
		zval *ztd;


		PHPWRITE(buf, len);

		/*	we really only need to flush when throttling is enabled,
			because we push the data as fast as possible anyway if not */
		if ((ztd = get_container_value(ctx->container, ZEND_STRL("throttleDelay") TSRMLS_CC))) {
			double delay;
			zval *ztd_copy = php_http_zsep(IS_DOUBLE, ztd);

			zval_ptr_dtor(&ztd);
			delay = Z_DVAL_P(ztd_copy);
			zval_ptr_dtor(&ztd_copy);

			if (delay >= PHP_HTTP_DIFFSEC) {
				if (php_output_get_level(TSRMLS_C)) {
					php_output_flush_all(TSRMLS_C);
				}
				if (!(php_output_get_status(TSRMLS_C) & PHP_OUTPUT_IMPLICITFLUSH)) {
					sapi_flush(TSRMLS_C);
				}
				php_http_sleep(delay);
			}
		}
	}
	return len;
}

PHP_HTTP_API STATUS php_http_env_send_response(zval *container TSRMLS_DC)
{
	struct output_ctx ctx = {NULL, container};
	zval *zbody, *zheader, *zrcode, *zversion;
	HashTable ranges;
	php_http_range_status_t range_status;
	php_http_message_body_t *body;
	size_t body_size;

	if (	!(zbody = get_container_value(container, ZEND_STRL("body") TSRMLS_CC))
	||		!(Z_TYPE_P(zbody) == IS_OBJECT)
	||		!instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_class_entry TSRMLS_CC)
	) {
		if (zbody) {
			zval_ptr_dtor(&zbody);
		}
		return FAILURE;
	}

	if ((zrcode = get_container_value(container, ZEND_STRL("responseCode") TSRMLS_CC))) {
		zval *zrcode_copy = php_http_zsep(IS_LONG, zrcode);

		zval_ptr_dtor(&zrcode);
		if (Z_LVAL_P(zrcode_copy) > 0) {
			php_http_env_set_response_code(Z_LVAL_P(zrcode_copy) TSRMLS_CC);
		}
		zval_ptr_dtor(&zrcode_copy);
	}

	if ((zversion = get_container_value(container, ZEND_STRL("httpVersion") TSRMLS_CC))) {
		php_http_version_t v;
		zval *zversion_copy = php_http_zsep(IS_STRING, zversion);

		zval_ptr_dtor(&zversion);
		if (Z_STRLEN_P(zversion_copy) && php_http_version_parse(&v, Z_STRVAL_P(zversion_copy) TSRMLS_CC)) {
			php_http_env_set_response_protocol_version(&v TSRMLS_CC);
			php_http_version_dtor(&v);
		}
		zval_ptr_dtor(&zversion_copy);
	}

	if ((zheader = get_container_value(container, ZEND_STRL("headers") TSRMLS_CC))) {
		if (Z_TYPE_P(zheader) == IS_ARRAY) {
			zval **val;
			HashPosition pos;
			php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

			FOREACH_KEYVAL(pos, zheader, key, val) {
				if (key.type == HASH_KEY_IS_STRING) {
					php_http_env_set_response_header_value(0, key.str, key.len - 1, *val, 1 TSRMLS_CC);
				}
			}
		}
		zval_ptr_dtor(&zheader);
	}

	body = ((php_http_message_body_object_t *) zend_object_store_get_object(zbody TSRMLS_CC))->body;
	body_size = php_http_message_body_size(body);
	php_http_env_set_response_header(0, ZEND_STRL("Accept-Ranges: bytes"), 1 TSRMLS_CC);
	zend_hash_init(&ranges, 0, NULL, ZVAL_PTR_DTOR, 0);
	range_status = php_http_env_get_request_ranges(&ranges, body_size TSRMLS_CC);

	switch (range_status) {
		case PHP_HTTP_RANGE_ERR:
			zend_hash_destroy(&ranges);
			if (!php_http_env_got_request_header(ZEND_STRL("If-Range") TSRMLS_CC)) {
				char *cr_header_str;
				size_t cr_header_len;

				cr_header_len = spprintf(&cr_header_str, 0, "Content-Range: bytes */%zu", body_size);
				php_http_env_set_response_header(416, cr_header_str, cr_header_len, 1 TSRMLS_CC);
				efree(cr_header_str);
				if (zbody) {
					zval_ptr_dtor(&zbody);
				}
				return SUCCESS;
			}
			break;

		case PHP_HTTP_RANGE_NO:
			/* send full entity */
			zend_hash_destroy(&ranges);
			break;

		case PHP_HTTP_RANGE_OK:
			/* send content-range response */
			if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(container, ZEND_STRL("If-Range") TSRMLS_CC)
			||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(container, ZEND_STRL("If-Range") TSRMLS_CC)
			) {
				/* send full entity */
				zend_hash_destroy(&ranges);
				break;
			}
			if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(container, ZEND_STRL("If-Match") TSRMLS_CC)
			||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(container, ZEND_STRL("If-Unmodified-Since") TSRMLS_CC)
			||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(container, ZEND_STRL("Unless-Modified-Since") TSRMLS_CC)
			) {
				zend_hash_destroy(&ranges);
				php_http_env_set_response_code(412 TSRMLS_CC);
				if (zbody) {
					zval_ptr_dtor(&zbody);
				}
				return SUCCESS;
			}
			if (zend_hash_num_elements(&ranges) == 1) {
				/* single range */
				zval **range, **begin, **end;

				if (SUCCESS != zend_hash_index_find(&ranges, 0, (void *) &range)
				||	SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(range), 0, (void *) &begin)
				||	SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(range), 1, (void *) &end)
				) {
					/* this should never happen */
					zend_hash_destroy(&ranges);
					php_http_env_set_response_code(500 TSRMLS_CC);
					if (zbody) {
						zval_ptr_dtor(&zbody);
					}
					return FAILURE;
				} else {
					char *cr_header_str;
					size_t cr_header_len;

					cr_header_len = spprintf(&cr_header_str, 0, "Content-Range: bytes %ld-%ld/%zu", Z_LVAL_PP(begin), Z_LVAL_PP(end), body_size);
					php_http_env_set_response_header(206, cr_header_str, cr_header_len, 1 TSRMLS_CC);
					efree(cr_header_str);

					/* send chunk */
					php_http_message_body_to_callback(body, output, &ctx, Z_LVAL_PP(begin), Z_LVAL_PP(end) - Z_LVAL_PP(begin) + 1);
					output(&ctx, NULL, 0 TSRMLS_CC);
					if (zbody) {
						zval_ptr_dtor(&zbody);
					}
					return SUCCESS;
				}
			} else {
				/* send multipart/byte-ranges message */
				HashPosition pos;
				zval **chunk, *zct;
				php_http_buffer_t preface;
				int free_ct = 0;
				char *content_type = "application/octet-stream";
				char boundary[32], *ct_header_str = "Content-Type: multipart/byteranges; boundary=                                ";

				if ((zct = get_container_value(container, ZEND_STRL("contentType") TSRMLS_CC))) {
					zval *zct_copy = php_http_zsep(IS_STRING, zct);

					zval_ptr_dtor(&zct);
					if (Z_STRLEN_P(zct_copy)) {
						content_type = estrndup(Z_STRVAL_P(zct_copy), Z_STRLEN_P(zct_copy));
						free_ct = 1;
					}

					zval_ptr_dtor(&zct);
				}

				php_http_boundary(boundary, sizeof(boundary));
				strlcpy(&ct_header_str[45], boundary, 32);

				php_http_env_set_response_header(206, ct_header_str, strlen(ct_header_str), 1 TSRMLS_CC);

				php_http_buffer_init(&preface);
				FOREACH_HASH_VAL(pos, &ranges, chunk) {
					zval **begin, **end;

					if (IS_ARRAY == Z_TYPE_PP(chunk)
					&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(chunk), 0, (void *) &begin)
					&&	IS_LONG == Z_TYPE_PP(begin)
					&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(chunk), 1, (void *) &end)
					&&	IS_LONG == Z_TYPE_PP(end)
					) {
						php_http_buffer_appendf(&preface,
								PHP_HTTP_CRLF
								"--%s" PHP_HTTP_CRLF
								"Content-Type: %s" PHP_HTTP_CRLF
								"Content-Range: bytes %ld-%ld/%zu" PHP_HTTP_CRLF,
								/* - */
								boundary,
								content_type,
								Z_LVAL_PP(begin),
								Z_LVAL_PP(end),
								body_size
						);
						php_http_buffer_fix(&preface);
						output(&ctx, PHP_HTTP_BUFFER_VAL(&preface), PHP_HTTP_BUFFER_LEN(&preface) TSRMLS_CC);
						php_http_buffer_reset(&preface);

						php_http_message_body_to_callback(body, output, &ctx, Z_LVAL_PP(begin), Z_LVAL_PP(end) - Z_LVAL_PP(begin) + 1);
					}
				}
				php_http_buffer_appendf(&preface, PHP_HTTP_CRLF "--%s--", boundary);
				php_http_buffer_fix(&preface);
				output(&ctx, PHP_HTTP_BUFFER_VAL(&preface), PHP_HTTP_BUFFER_LEN(&preface) TSRMLS_CC);
				php_http_buffer_dtor(&preface);
				output(&ctx, NULL, 0 TSRMLS_CC);
				if (zbody) {
					zval_ptr_dtor(&zbody);
				}
				return SUCCESS;
			}
			break;
	}

	switch (php_http_env_is_response_cached_by_etag(container, ZEND_STRL("If-None-Match"))) {
		case PHP_HTTP_CACHE_MISS:
			break;

		case PHP_HTTP_CACHE_NO:
			if (PHP_HTTP_CACHE_HIT != php_http_env_is_response_cached_by_last_modified(container, ZEND_STRL("If-Modified-Since"))) {
				break;
			}

		case PHP_HTTP_CACHE_HIT:
			php_http_env_set_response_code(304 TSRMLS_CC);
			if (zbody) {
				zval_ptr_dtor(&zbody);
			}
			return SUCCESS;
	}

	php_http_message_body_to_callback(body, output, &ctx, 0, 0);
	output(&ctx, NULL, 0 TSRMLS_CC);

	if (zbody) {
		zval_ptr_dtor(&zbody);
	}
	return SUCCESS;
}

static PHP_HTTP_STRLIST(php_http_env_response_status) =
	PHP_HTTP_STRLIST_ITEM("Continue")
	PHP_HTTP_STRLIST_ITEM("Switching Protocols")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("OK")
	PHP_HTTP_STRLIST_ITEM("Created")
	PHP_HTTP_STRLIST_ITEM("Accepted")
	PHP_HTTP_STRLIST_ITEM("Non-Authoritative Information")
	PHP_HTTP_STRLIST_ITEM("No Content")
	PHP_HTTP_STRLIST_ITEM("Reset Content")
	PHP_HTTP_STRLIST_ITEM("Partial Content")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("Multiple Choices")
	PHP_HTTP_STRLIST_ITEM("Moved Permanently")
	PHP_HTTP_STRLIST_ITEM("Found")
	PHP_HTTP_STRLIST_ITEM("See Other")
	PHP_HTTP_STRLIST_ITEM("Not Modified")
	PHP_HTTP_STRLIST_ITEM("Use Proxy")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("Temporary Redirect")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("Bad Request")
	PHP_HTTP_STRLIST_ITEM("Unauthorized")
	PHP_HTTP_STRLIST_ITEM("Payment Required")
	PHP_HTTP_STRLIST_ITEM("Forbidden")
	PHP_HTTP_STRLIST_ITEM("Not Found")
	PHP_HTTP_STRLIST_ITEM("Method Not Allowed")
	PHP_HTTP_STRLIST_ITEM("Not Acceptable")
	PHP_HTTP_STRLIST_ITEM("Proxy Authentication Required")
	PHP_HTTP_STRLIST_ITEM("Request Timeout")
	PHP_HTTP_STRLIST_ITEM("Conflict")
	PHP_HTTP_STRLIST_ITEM("Gone")
	PHP_HTTP_STRLIST_ITEM("Length Required")
	PHP_HTTP_STRLIST_ITEM("Precondition Failed")
	PHP_HTTP_STRLIST_ITEM("Request Entity Too Large")
	PHP_HTTP_STRLIST_ITEM("Request URI Too Long")
	PHP_HTTP_STRLIST_ITEM("Unsupported Media Type")
	PHP_HTTP_STRLIST_ITEM("Requested Range Not Satisfiable")
	PHP_HTTP_STRLIST_ITEM("Expectation Failed")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("Internal Server Error")
	PHP_HTTP_STRLIST_ITEM("Not Implemented")
	PHP_HTTP_STRLIST_ITEM("Bad Gateway")
	PHP_HTTP_STRLIST_ITEM("Service Unavailable")
	PHP_HTTP_STRLIST_ITEM("Gateway Timeout")
	PHP_HTTP_STRLIST_ITEM("HTTP Version Not Supported")
	PHP_HTTP_STRLIST_STOP
;

PHP_HTTP_API const char *php_http_env_get_response_status_for_code(unsigned code)
{
	return php_http_strlist_find(php_http_env_response_status, 100, code);
}

zend_class_entry *php_http_env_class_entry;

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpEnv, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpEnv, method, 0)
#define PHP_HTTP_ENV_ME(method)					PHP_ME(HttpEnv, method, PHP_HTTP_ARGS(HttpEnv, method), ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

PHP_HTTP_BEGIN_ARGS(getRequestHeader, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getRequestBody, 0)
	PHP_HTTP_ARG_VAL(body_class_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getResponseStatusForCode, 1)
	PHP_HTTP_ARG_VAL(code, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getResponseHeader, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseCode);

PHP_HTTP_BEGIN_ARGS(setResponseHeader, 1)
	PHP_HTTP_ARG_VAL(header_name, 0)
	PHP_HTTP_ARG_VAL(header_value, 0)
	PHP_HTTP_ARG_VAL(response_code, 0)
	PHP_HTTP_ARG_VAL(replace_header, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setResponseCode, 1)
	PHP_HTTP_ARG_VAL(code, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiateLanguage, 0)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiateContentType, 0)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiateCharset, 0)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiate, 0)
	PHP_HTTP_ARG_VAL(value, 0)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(persistentHandlesStat);

PHP_HTTP_BEGIN_ARGS(persistentHandlesClean, 0)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(persistentHandlesIdent, 0)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;

zend_function_entry php_http_env_method_entry[] = {
	PHP_HTTP_ENV_ME(getRequestHeader)
	PHP_HTTP_ENV_ME(getRequestBody)

	PHP_HTTP_ENV_ME(getResponseStatusForCode)

	PHP_HTTP_ENV_ME(getResponseHeader)
	PHP_HTTP_ENV_ME(getResponseCode)
	PHP_HTTP_ENV_ME(setResponseHeader)
	PHP_HTTP_ENV_ME(setResponseCode)

	PHP_HTTP_ENV_ME(negotiateLanguage)
	PHP_HTTP_ENV_ME(negotiateContentType)
	PHP_HTTP_ENV_ME(negotiateCharset)
	PHP_HTTP_ENV_ME(negotiate)

	PHP_HTTP_ENV_ME(persistentHandlesStat)
	PHP_HTTP_ENV_ME(persistentHandlesClean)
	PHP_HTTP_ENV_ME(persistentHandlesIdent)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpEnv, getRequestHeader)
{
	char *header_name_str;
	int header_name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &header_name_str, &header_name_len)) {
		if (header_name_str && header_name_len) {
			char *header_value = php_http_env_get_request_header(header_name_str, header_name_len TSRMLS_CC);

			if (header_value) {
				RETURN_STRING(header_value, 0);
			}
			RETURN_NULL();
		} else {
			array_init(return_value);
			php_http_env_get_request_headers(Z_ARRVAL_P(return_value) TSRMLS_CC);
			return;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, getRequestBody)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		zend_class_entry *class_entry = php_http_message_body_class_entry;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|C", &class_entry)) {
			zend_object_value ov;
			php_http_message_body_t *body = php_http_env_get_request_body(TSRMLS_C);

			if (SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_message_body_object_new_ex, php_http_message_body_class_entry, body, NULL TSRMLS_CC)) {
				RETURN_OBJVAL(ov, 0);
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpEnv, getResponseStatusForCode)
{
	long code;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_STRING(php_http_env_get_response_status_for_code(code), 1);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, getResponseHeader)
{
	char *header_name_str;
	int header_name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &header_name_str, &header_name_len)) {
		if (header_name_str && header_name_len) {
			char *header_value = php_http_env_get_response_header(header_name_str, header_name_len TSRMLS_CC);

			if (header_value) {
				RETURN_STRING(header_value, 0);
			}
			RETURN_NULL();
		} else {
			array_init(return_value);
			php_http_env_get_response_headers(Z_ARRVAL_P(return_value) TSRMLS_CC);
			return;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, getResponseCode)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_LONG(php_http_env_get_response_code(TSRMLS_C));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, setResponseHeader)
{
	char *header_name_str;
	int header_name_len;
	zval *header_value;
	long code = 0;
	zend_bool replace_header = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z!lb", &header_name_str, &header_name_len, &header_value, &code, &replace_header)) {
		RETURN_SUCCESS(php_http_env_set_response_header_value(code, header_name_str, header_name_len, header_value, replace_header TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, setResponseCode)
{
	long code;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_SUCCESS(php_http_env_set_response_code(code TSRMLS_CC));
	}
	RETURN_FALSE;
}


#define PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported) \
	{ \
		zval **value; \
		 \
		zend_hash_internal_pointer_reset((supported)); \
		if (SUCCESS == zend_hash_get_current_data((supported), (void *) &value)) { \
			RETVAL_ZVAL(*value, 1, 0); \
		} else { \
			RETVAL_NULL(); \
		} \
	}

#define PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array) \
	PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported); \
	if (rs_array) { \
		HashPosition pos; \
		zval **value_ptr; \
		 \
		FOREACH_HASH_VAL(pos, supported, value_ptr) { \
			zval *value = php_http_zsep(IS_STRING, *value_ptr); \
			add_assoc_double(rs_array, Z_STRVAL_P(value), 1.0); \
			zval_ptr_dtor(&value); \
		} \
	}

#define PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(result, supported, rs_array) \
	{ \
		char *key; \
		uint key_len; \
		ulong idx; \
		 \
		if (zend_hash_num_elements(result) && HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(result, &key, &key_len, &idx, 1, NULL)) { \
			RETVAL_STRINGL(key, key_len-1, 0); \
		} else { \
			PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported); \
		} \
		\
		if (rs_array) { \
			zend_hash_copy(Z_ARRVAL_P(rs_array), result, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *)); \
		} \
		\
		zend_hash_destroy(result); \
		FREE_HASHTABLE(result); \
	}

#define PHP_HTTP_DO_NEGOTIATE(type, supported, rs_array) \
	{ \
		HashTable *result; \
		if ((result = php_http_negotiate_ ##type(supported))) { \
			PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(result, supported, rs_array); \
		} else { \
			PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array); \
		} \
	}

PHP_METHOD(HttpEnv, negotiateLanguage)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}

		PHP_HTTP_DO_NEGOTIATE(language, supported, rs_array);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, negotiateCharset)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}
		PHP_HTTP_DO_NEGOTIATE(charset, supported, rs_array);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, negotiateContentType)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}
		PHP_HTTP_DO_NEGOTIATE(content_type, supported, rs_array);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, negotiate)
{
	HashTable *supported;
	zval *rs_array = NULL;
	char *value_str;
	int value_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sH|z", &value_str, &value_len, &supported, &rs_array)) {
		HashTable *rs;

		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}

		if ((rs = php_http_negotiate(value_str, supported, php_http_negotiate_default_func))) {
			PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(rs, supported, rs_array);
		} else {
			PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array);
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, persistentHandlesStat)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		object_init(return_value);
		if (php_http_persistent_handle_statall(HASH_OF(return_value))) {
			return;
		}
		zval_dtor(return_value);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, persistentHandlesClean)
{
	char *name_str = NULL;
	int name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &name_str, &name_len)) {
		php_http_persistent_handle_cleanup(name_str, name_len, 1 TSRMLS_CC);
	}
}

PHP_METHOD(HttpEnv, persistentHandlesIdent)
{
	char *ident_str = NULL;
	int ident_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ident_str, &ident_len)) {
		RETVAL_STRING(zend_ini_string(ZEND_STRS("http.persistent.handles.ident"), 0), 1);
		if (ident_str && ident_len) {
			zend_alter_ini_entry(ZEND_STRS("http.persistent.handles.ident"), ident_str, ident_len, ZEND_INI_USER, PHP_INI_STAGE_RUNTIME);
		}
		return;
	}
	RETURN_FALSE;
}

zend_class_entry *php_http_env_request_class_entry;

#undef PHP_HTTP_BEGIN_ARGS
#undef PHP_HTTP_EMPTY_ARGS
#define PHP_HTTP_BEGIN_ARGS(method, req_args) 		PHP_HTTP_BEGIN_ARGS_EX(HttpEnvRequest, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)					PHP_HTTP_EMPTY_ARGS_EX(HttpEnvRequest, method, 0)
#define PHP_HTTP_ENV_REQUEST_ME(method, visibility)	PHP_ME(HttpEnvRequest, method, PHP_HTTP_ARGS(HttpEnvRequest, method), visibility)

PHP_HTTP_EMPTY_ARGS(__construct);

zend_function_entry php_http_env_request_method_entry[] = {
	PHP_HTTP_ENV_REQUEST_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpEnvRequest, __construct)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			with_error_handling(EH_THROW, PHP_HTTP_EX_CE(message)) {
				obj->message = php_http_message_init_env(obj->message, PHP_HTTP_REQUEST TSRMLS_CC);
			} end_error_handling();
		}
	} end_error_handling();
}


zend_class_entry *php_http_env_response_class_entry;

#undef PHP_HTTP_BEGIN_ARGS
#undef PHP_HTTP_EMPTY_ARGS
#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpEnvResponse, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpEnvResponse, method, 0)
#define PHP_HTTP_ENV_RESPONSE_ME(method, visibility)	PHP_ME(HttpEnvResponse, method, PHP_HTTP_ARGS(HttpEnvResponse, method), visibility)

PHP_HTTP_EMPTY_ARGS(__construct);

PHP_HTTP_BEGIN_ARGS(setContentType, 1)
	PHP_HTTP_ARG_VAL(content_type, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setContentDisposition, 1)
	PHP_HTTP_ARG_VAL(content_disposition, 0)
	PHP_HTTP_ARG_VAL(filename, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setCacheControl, 1)
	PHP_HTTP_ARG_VAL(cache_control, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setLastModified, 1)
	PHP_HTTP_ARG_VAL(last_modified, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(isCachedByLastModified, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setEtag, 1)
	PHP_HTTP_ARG_VAL(etag, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(isCachedByEtag, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setThrottleRate, 1)
	PHP_HTTP_ARG_VAL(chunk_size, 0)
	PHP_HTTP_ARG_VAL(delay, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(send);


zend_function_entry php_http_env_response_method_entry[] = {
	PHP_HTTP_ENV_RESPONSE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_ENV_RESPONSE_ME(setContentType, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setContentDisposition, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setCacheControl, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setLastModified, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(isCachedByLastModified, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setEtag, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(isCachedByEtag, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setThrottleRate, ZEND_ACC_PUBLIC)

	PHP_HTTP_ENV_RESPONSE_ME(send, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};


PHP_METHOD(HttpEnvResponse, __construct)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			with_error_handling(EH_THROW, PHP_HTTP_EX_CE(message)) {
				obj->message = php_http_message_init_env(obj->message, PHP_HTTP_RESPONSE TSRMLS_CC);
			} end_error_handling();
		}
	} end_error_handling();

}

PHP_METHOD(HttpEnvResponse, setContentType)
{
	char *ct_str;
	int ct_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ct_str, &ct_len)) {
		RETURN_SUCCESS(php_http_env_set_response_content_type(getThis(), ct_str, ct_len, NULL TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, setContentDisposition)
{
	long cd;
	char *file_str = NULL;
	int file_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|s!", &cd, &file_str, &file_len)) {
		RETURN_SUCCESS(php_http_env_set_response_content_disposition(getThis(), cd, file_str, file_len, NULL TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, setCacheControl)
{
	char *cc_str;
	int cc_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &cc_str, &cc_len)) {
		RETURN_SUCCESS(php_http_env_set_response_cache_control(getThis(), cc_str, cc_len, NULL TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, setLastModified)
{
	long last_modified;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &last_modified)) {
		RETURN_SUCCESS(php_http_env_set_response_last_modified(getThis(), last_modified, NULL TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, isCachedByLastModified)
{
	char *header_name_str = NULL;
	int header_name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &header_name_str, &header_name_len)) {
		if (!header_name_str || !header_name_len) {
			header_name_str = "If-Modified-Since";
			header_name_len = lenof("If-Modified-Since");
		}
		RETURN_LONG(php_http_env_is_response_cached_by_last_modified(getThis(), header_name_str, header_name_len TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, setEtag)
{
	char *etag_str;
	int etag_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s!", &etag_str, &etag_len)) {
		RETURN_SUCCESS(php_http_env_set_response_etag(getThis(), etag_str, etag_len, NULL TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, isCachedByEtag)
{
	char *header_name_str = NULL;
	int header_name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &header_name_str, &header_name_len)) {
		if (!header_name_str || !header_name_len) {
			header_name_str = "If-None-Match";
			header_name_len = lenof("If-None-Match");
		}
		RETURN_LONG(php_http_env_is_response_cached_by_etag(getThis(), header_name_str, header_name_len TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, setThrottleRate)
{
	long chunk_size;
	double delay = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|d", &chunk_size, &delay)) {
		php_http_env_set_response_throttle_rate(getThis(), chunk_size, delay TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, send)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_SUCCESS(php_http_env_send_response(getThis() TSRMLS_CC));
	}
	RETURN_FALSE;
}


PHP_MINIT_FUNCTION(http_env)
{
	PHP_HTTP_REGISTER_CLASS(http, Env, http_env, NULL, 0);
	PHP_HTTP_REGISTER_CLASS(http\\env, Request, http_env_request, php_http_message_class_entry, 0);
	PHP_HTTP_REGISTER_CLASS(http\\env, Response, http_env_response, php_http_message_class_entry, 0);

	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CONTENT_DISPOSITION_INLINE"), PHP_HTTP_CONTENT_DISPOSITION_INLINE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CONTENT_DISPOSITION_ATTACHMENT"), PHP_HTTP_CONTENT_DISPOSITION_ATTACHMENT TSRMLS_CC);

	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_NO"), PHP_HTTP_CACHE_NO TSRMLS_CC);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_HIT"), PHP_HTTP_CACHE_HIT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_MISS"), PHP_HTTP_CACHE_MISS TSRMLS_CC);

	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("contentType"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("etag"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("lastModified"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("throttleDelay"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("throttleChunk"), ZEND_ACC_PROTECTED TSRMLS_CC);

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
