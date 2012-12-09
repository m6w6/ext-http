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

#include "php_http_api.h"

static void set_option(zval *options, const char *name_str, size_t name_len, int type, const void *value_ptr, size_t value_len TSRMLS_DC)
{
	if (Z_TYPE_P(options) == IS_OBJECT) {
		/* stupid non-const api */
		char *name = estrndup(name_str, name_len);
		if (value_ptr) {
			switch (type) {
				case IS_DOUBLE:
					zend_update_property_double(Z_OBJCE_P(options), options, name, name_len, *(double *)value_ptr TSRMLS_CC);
					break;
				case IS_LONG:
					zend_update_property_long(Z_OBJCE_P(options), options, name, name_len, *(long *)value_ptr TSRMLS_CC);
					break;
				case IS_STRING:
					zend_update_property_stringl(Z_OBJCE_P(options), options, name, name_len, value_ptr, value_len TSRMLS_CC);
					break;
			}
		} else {
			zend_update_property_null(Z_OBJCE_P(options), options, name, name_len TSRMLS_CC);
		}
		efree(name);
	} else {
		convert_to_array(options);
		if (value_ptr) {
			switch (type) {
				case IS_DOUBLE:
					add_assoc_double_ex(options, name_str, name_len + 1, *(double *)value_ptr);
					break;
				case IS_LONG:
					add_assoc_long_ex(options, name_str, name_len + 1, *(long *)value_ptr);
					break;
				case IS_STRING: {
					char *value = estrndup(value_ptr, value_len);
					add_assoc_stringl_ex(options, name_str, name_len + 1, value, value_len, 0);
					break;
				}
			}
		} else {
			add_assoc_null_ex(options, name_str, name_len + 1);
		}
	}
}
static zval *get_option(zval *options, const char *name_str, size_t name_len TSRMLS_DC)
{
	zval *val, **valptr;

	if (Z_TYPE_P(options) == IS_OBJECT) {
		char *name = estrndup(name_str, name_len);
		val = zend_read_property(Z_OBJCE_P(options), options, name, name_len, 0 TSRMLS_CC);
		efree(name);
	} else {
		if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(options), name_str, name_len + 1, (void *) &valptr)) {
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

PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_etag(zval *options, const char *header_str, size_t header_len TSRMLS_DC)
{
	php_http_cache_status_t ret = PHP_HTTP_CACHE_NO;
	int free_etag = 0;
	char *header = NULL, *etag;
	zval *zetag, *zbody = NULL;

	if (	!(zbody = get_option(options, ZEND_STRL("body") TSRMLS_CC))
	|| 		!(Z_TYPE_P(zbody) == IS_OBJECT)
	||		!instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_get_class_entry() TSRMLS_CC)
	) {
		if (zbody) {
			zval_ptr_dtor(&zbody);
		}
		return ret;
	}

	if ((zetag = get_option(options, ZEND_STRL("etag") TSRMLS_CC))) {
		zval *zetag_copy = php_http_ztyp(IS_STRING, zetag);
		zval_ptr_dtor(&zetag);
		zetag = zetag_copy;
	}

	if (zetag && Z_STRLEN_P(zetag)) {
		etag = Z_STRVAL_P(zetag);
	} else if ((etag = php_http_message_body_etag(((php_http_message_body_object_t *) zend_object_store_get_object(zbody TSRMLS_CC))->body))) {
		set_option(options, ZEND_STRL("etag"), IS_STRING, etag, strlen(etag) TSRMLS_CC);
		free_etag = 1;
	}

	if (zbody) {
		zval_ptr_dtor(&zbody);
	}

	if (zetag) {
		zval_ptr_dtor(&zetag);
	}

	if (etag && (header = php_http_env_get_request_header(header_str, header_len, NULL TSRMLS_CC))) {
		ret = php_http_match(header, etag, PHP_HTTP_MATCH_WORD)  ? PHP_HTTP_CACHE_HIT : PHP_HTTP_CACHE_MISS;
	}

	if (free_etag) {
		efree(etag);
	}
	STR_FREE(header);

	return ret;
}

PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_last_modified(zval *options, const char *header_str, size_t header_len TSRMLS_DC)
{
	char *header;
	time_t ums, lm = 0;
	zval *zbody = NULL, *zlm;

	if (	!(zbody = get_option(options, ZEND_STRL("body") TSRMLS_CC))
	||		!(Z_TYPE_P(zbody) == IS_OBJECT)
	||		!instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_get_class_entry() TSRMLS_CC)
	) {
		if (zbody) {
			zval_ptr_dtor(&zbody);
		}
		return PHP_HTTP_CACHE_NO;
	}

	if ((zlm = get_option(options, ZEND_STRL("lastModified") TSRMLS_CC))) {
		zval *zlm_copy = php_http_ztyp(IS_LONG, zlm);
		zval_ptr_dtor(&zlm);
		zlm = zlm_copy;
	}

	if (zlm && Z_LVAL_P(zlm) > 0) {
		lm = Z_LVAL_P(zlm);
	} else {
		lm = php_http_message_body_mtime(((php_http_message_body_object_t *) zend_object_store_get_object(zbody TSRMLS_CC))->body);
		set_option(options, ZEND_STRL("lastModified"), IS_LONG, &lm, 0 TSRMLS_CC);
	}

	zval_ptr_dtor(&zbody);
	if (zlm) {
		zval_ptr_dtor(&zlm);
	}

	if (!(header = php_http_env_get_request_header(header_str, header_len, NULL TSRMLS_CC))) {
		return PHP_HTTP_CACHE_NO;
	}

	ums = php_parse_date(header, NULL);
	efree(header);

	if (ums > 0 && ums >= lm) {
		return PHP_HTTP_CACHE_HIT;
	} else {
		return PHP_HTTP_CACHE_MISS;
	}
}

static size_t output(void *context, char *buf, size_t len TSRMLS_DC)
{
	php_http_env_response_t *r = context;

	PHPWRITE(buf, len);

	/*	we really only need to flush when throttling is enabled,
		because we push the data as fast as possible anyway if not */
	if (r->throttle.delay >= PHP_HTTP_DIFFSEC) {
#if PHP_VERSION_ID >= 50400
		if (php_output_get_level(TSRMLS_C)) {
			php_output_flush_all(TSRMLS_C);
		}
		if (!(php_output_get_status(TSRMLS_C) & PHP_OUTPUT_IMPLICITFLUSH)) {
			sapi_flush(TSRMLS_C);
		}
#else
		php_end_ob_buffer(1, 1 TSRMLS_CC);
		sapi_flush(TSRMLS_C);
#endif
		php_http_sleep(r->throttle.delay);
	}
	return len;
}

#define php_http_env_response_send_done(r) php_http_env_response_send_data((r), NULL, 0)
static STATUS php_http_env_response_send_data(php_http_env_response_t *r, const char *buf, size_t len)
{
	size_t chunk = r->throttle.chunk ? r->throttle.chunk : PHP_HTTP_SENDBUF_SIZE;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (r->content.encoder) {
		char *enc_str = NULL;
		size_t enc_len = 0;

		if (buf) {
			if (SUCCESS != php_http_encoding_stream_update(r->content.encoder, buf, len, &enc_str, &enc_len)) {
				return FAILURE;
			}
		} else {
			if (SUCCESS != php_http_encoding_stream_finish(r->content.encoder, &enc_str, &enc_len)) {
				return FAILURE;
			}
		}

		if (enc_str) {
			php_http_buffer_chunked_output(&r->buffer, enc_str, enc_len, buf ? chunk : 0, output, r TSRMLS_CC);
			STR_FREE(enc_str);
		}
	} else {
		php_http_buffer_chunked_output(&r->buffer, buf, len, buf ? chunk : 0, output, r TSRMLS_CC);
	}

	return SUCCESS;
}

PHP_HTTP_API php_http_env_response_t *php_http_env_response_init(php_http_env_response_t *r, zval *options TSRMLS_DC)
{
	php_http_env_response_t *free_r = NULL;

	if (!r) {
		r = free_r = emalloc(sizeof(*r));
	}
	memset(r, 0, sizeof(*r));

	r->buffer = php_http_buffer_init(NULL);

	Z_ADDREF_P(options);
	r->options = options;

	TSRMLS_SET_CTX(r->ts);

	return r;
}

PHP_HTTP_API void php_http_env_response_dtor(php_http_env_response_t *r)
{
	php_http_buffer_free(&r->buffer);
	zval_ptr_dtor(&r->options);
	STR_FREE(r->content.type);
	STR_FREE(r->content.encoding);
	if (r->content.encoder) {
		php_http_encoding_stream_free(&r->content.encoder);
	}
}

PHP_HTTP_API void php_http_env_response_free(php_http_env_response_t **r)
{
	if (*r) {
		php_http_env_response_dtor(*r);
		efree(*r);
		*r = NULL;
	}
}

static STATUS php_http_env_response_send_head(php_http_env_response_t *r)
{
	STATUS ret = SUCCESS;
	zval *zoption, *options = r->options;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (r->done) {
		return ret;
	}

	if ((zoption = get_option(options, ZEND_STRL("responseCode") TSRMLS_CC))) {
		zval *zoption_copy = php_http_ztyp(IS_LONG, zoption);

		zval_ptr_dtor(&zoption);
		if (Z_LVAL_P(zoption_copy) > 0) {
			ret = php_http_env_set_response_code(Z_LVAL_P(zoption_copy) TSRMLS_CC);
		}
		zval_ptr_dtor(&zoption_copy);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if ((zoption = get_option(options, ZEND_STRL("httpVersion") TSRMLS_CC))) {
		php_http_version_t v;
		zval *zoption_copy = php_http_ztyp(IS_STRING, zoption);

		zval_ptr_dtor(&zoption);
		if (Z_STRLEN_P(zoption_copy) && php_http_version_parse(&v, Z_STRVAL_P(zoption_copy) TSRMLS_CC)) {
			ret = php_http_env_set_response_protocol_version(&v TSRMLS_CC);
			php_http_version_dtor(&v);
		}
		zval_ptr_dtor(&zoption_copy);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if ((zoption = get_option(options, ZEND_STRL("headers") TSRMLS_CC))) {
		if (Z_TYPE_P(zoption) == IS_ARRAY) {
			zval **val;
			HashPosition pos;
			php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

			FOREACH_KEYVAL(pos, zoption, key, val) {
				if (key.type == HASH_KEY_IS_STRING) {
					if (SUCCESS != (ret = php_http_env_set_response_header_value(0, key.str, key.len - 1, *val, 1 TSRMLS_CC))) {
						break;
					}
				}
			}
		}
		zval_ptr_dtor(&zoption);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if ((zoption = get_option(options, ZEND_STRL("contentType") TSRMLS_CC))) {
		zval *zoption_copy = php_http_ztyp(IS_STRING, zoption);

		zval_ptr_dtor(&zoption);
		if (Z_STRLEN_P(zoption_copy)) {
			PHP_HTTP_CHECK_CONTENT_TYPE(Z_STRVAL_P(zoption_copy), ret = FAILURE) else {
				if (SUCCESS == (ret = php_http_env_set_response_header_format(0, 1 TSRMLS_CC, "Content-Type: %.*s", Z_STRLEN_P(zoption_copy), Z_STRVAL_P(zoption_copy)))) {
					r->content.type = estrndup(Z_STRVAL_P(zoption_copy), Z_STRLEN_P(zoption_copy));
				}
			}
		}
		zval_ptr_dtor(&zoption_copy);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if (r->range.status == PHP_HTTP_RANGE_OK) {
		if (zend_hash_num_elements(&r->range.values) == 1) {
			zval **range, **begin, **end;

			if (SUCCESS != zend_hash_index_find(&r->range.values, 0, (void *) &range)
			||	SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(range), 0, (void *) &begin)
			||	SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(range), 1, (void *) &end)
			) {
				/* this should never happen */
				zend_hash_destroy(&r->range.values);
				php_http_env_set_response_code(500 TSRMLS_CC);
				ret = FAILURE;
			} else {
				ret = php_http_env_set_response_header_format(206, 1 TSRMLS_CC, "Content-Range: bytes %ld-%ld/%zu", Z_LVAL_PP(begin), Z_LVAL_PP(end), r->content.length);
			}
		} else {
			php_http_boundary(r->range.boundary, sizeof(r->range.boundary) TSRMLS_CC);
			ret = php_http_env_set_response_header_format(206, 1 TSRMLS_CC, "Content-Type: multipart/byteranges; boundary=%s", r->range.boundary);
		}
	} else {
		if ((zoption = get_option(options, ZEND_STRL("cacheControl") TSRMLS_CC))) {
			zval *zoption_copy = php_http_ztyp(IS_STRING, zoption);

			zval_ptr_dtor(&zoption);
			if (Z_STRLEN_P(zoption_copy)) {
				ret = php_http_env_set_response_header_format(0, 1 TSRMLS_CC, "Cache-Control: %.*s", Z_STRLEN_P(zoption_copy), Z_STRVAL_P(zoption_copy) TSRMLS_CC);
			}
			zval_ptr_dtor(&zoption_copy);
		}

		if (ret != SUCCESS) {
			return ret;
		}

		if ((zoption = get_option(options, ZEND_STRL("contentDisposition") TSRMLS_CC))) {
			zval *zoption_copy = php_http_ztyp(IS_ARRAY, zoption);
			php_http_buffer_t buf;

			php_http_buffer_init(&buf);
			if (php_http_params_to_string(&buf, Z_ARRVAL_P(zoption_copy), ZEND_STRL(","), ZEND_STRL(";"), ZEND_STRL("="), PHP_HTTP_PARAMS_DEFAULT TSRMLS_CC)) {
				ret = php_http_env_set_response_header_format(0, 1 TSRMLS_CC, "Content-Disposition: %s", PHP_HTTP_BUFFER_VAL(&buf));
			}

			php_http_buffer_dtor(&buf);
			zval_ptr_dtor(&zoption_copy);
			zval_ptr_dtor(&zoption);
		}

		if (ret != SUCCESS) {
			return ret;
		}

		if ((zoption = get_option(options, ZEND_STRL("contentEncoding") TSRMLS_CC))) {
			zval *zoption_copy = php_http_ztyp(IS_LONG, zoption);
			zval zsupported;
			HashTable *result = NULL;

			zval_ptr_dtor(&zoption);
			switch (Z_LVAL_P(zoption_copy)) {
				case PHP_HTTP_CONTENT_ENCODING_GZIP:
					INIT_PZVAL(&zsupported);
					array_init(&zsupported);
					add_next_index_stringl(&zsupported, ZEND_STRL("none"), 1);
					add_next_index_stringl(&zsupported, ZEND_STRL("gzip"), 1);
					add_next_index_stringl(&zsupported, ZEND_STRL("deflate"), 1);

					if ((result = php_http_negotiate_encoding(Z_ARRVAL(zsupported) TSRMLS_CC))) {
						char *key_str = NULL;
						uint key_len = 0;

						zend_hash_internal_pointer_reset(result);
						if (HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(result, &key_str, &key_len, NULL, 0, NULL)) {
							if (!strcmp(key_str, "gzip")) {
								if (!(r->content.encoder = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_deflate_ops(), PHP_HTTP_DEFLATE_TYPE_GZIP TSRMLS_CC))) {
									ret = FAILURE;
								} else if (SUCCESS == (ret = php_http_env_set_response_header(0, ZEND_STRL("Content-Encoding: gzip"), 1 TSRMLS_CC))) {
									r->content.encoding = estrndup(key_str, key_len - 1);
								}
							} else if (!strcmp(key_str, "deflate")) {
								if (!(r->content.encoder = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_deflate_ops(), PHP_HTTP_DEFLATE_TYPE_ZLIB TSRMLS_CC))) {
									ret = FAILURE;
								} else if (SUCCESS == (ret = php_http_env_set_response_header(0, ZEND_STRL("Content-Encoding: deflate"), 1 TSRMLS_CC))) {
									r->content.encoding = estrndup(key_str, key_len - 1);
								}
							} else {
								ret = php_http_env_set_response_header_value(0, ZEND_STRL("Content-Encoding"), NULL, 0 TSRMLS_CC);
							}

							if (SUCCESS == ret) {
								ret = php_http_env_set_response_header(0, ZEND_STRL("Vary: Accept-Encoding"), 0 TSRMLS_CC);
							}
						}

						zend_hash_destroy(result);
						FREE_HASHTABLE(result);
					}

					zval_dtor(&zsupported);
					break;

				case PHP_HTTP_CONTENT_ENCODING_NONE:
				default:
					ret = php_http_env_set_response_header_value(0, ZEND_STRL("Content-Encoding"), NULL, 0 TSRMLS_CC);
					break;
			}
			zval_ptr_dtor(&zoption_copy);
		}

		if (SUCCESS != ret) {
			return ret;
		}

		if (php_http_env_get_response_code(TSRMLS_C) < 400 && !php_http_env_got_request_header(ZEND_STRL("Authorization") TSRMLS_CC)
		&&	(	!SG(request_info).request_method
			||	!strcasecmp(SG(request_info).request_method, "GET")
			||	!strcasecmp(SG(request_info).request_method, "HEAD")
			)
		) {
			switch (php_http_env_is_response_cached_by_etag(options, ZEND_STRL("If-None-Match") TSRMLS_CC)) {
				case PHP_HTTP_CACHE_MISS:
					break;

				case PHP_HTTP_CACHE_NO:
					if (PHP_HTTP_CACHE_HIT != php_http_env_is_response_cached_by_last_modified(options, ZEND_STRL("If-Modified-Since") TSRMLS_CC)) {
						break;
					}
					/*  no break */

				case PHP_HTTP_CACHE_HIT:
					ret = php_http_env_set_response_code(304 TSRMLS_CC);
					r->done = 1;
					break;
			}

			if ((zoption = get_option(options, ZEND_STRL("etag") TSRMLS_CC))) {
				zval *zoption_copy = php_http_ztyp(IS_STRING, zoption);

				zval_ptr_dtor(&zoption);
				if (*Z_STRVAL_P(zoption_copy) != '"' &&	strncmp(Z_STRVAL_P(zoption_copy), "W/\"", 3)) {
					ret = php_http_env_set_response_header_format(0, 1 TSRMLS_CC, "ETag: \"%s\"", Z_STRVAL_P(zoption_copy));
				} else {
					ret = php_http_env_set_response_header_value(0, ZEND_STRL("ETag"), zoption_copy, 1 TSRMLS_CC);
				}
				zval_ptr_dtor(&zoption_copy);
			}
			if ((zoption = get_option(options, ZEND_STRL("lastModified") TSRMLS_CC))) {
				zval *zoption_copy = php_http_ztyp(IS_LONG, zoption);

				zval_ptr_dtor(&zoption);
				if (Z_LVAL_P(zoption_copy)) {
					char *date = php_format_date(ZEND_STRL(PHP_HTTP_DATE_FORMAT), Z_LVAL_P(zoption_copy), 0 TSRMLS_CC);
					if (date) {
						ret = php_http_env_set_response_header_format(0, 1 TSRMLS_CC, "Last-Modified: %s", date);
						efree(date);
					}
				}
				zval_ptr_dtor(&zoption_copy);
			}
		}
	}

	return ret;
}

static STATUS php_http_env_response_send_body(php_http_env_response_t *r)
{
	STATUS ret = SUCCESS;
	zval *zbody, *zoption;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (r->done) {
		return ret;
	}

	if (	(zbody = get_option(r->options, ZEND_STRL("body") TSRMLS_CC))
	&&		(Z_TYPE_P(zbody) == IS_OBJECT)
	&&		instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_get_class_entry() TSRMLS_CC)
	) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(zbody TSRMLS_CC);

		if ((zoption = get_option(r->options, ZEND_STRL("throttleDelay") TSRMLS_CC))) {
			if (Z_TYPE_P(zoption) == IS_DOUBLE) {
				r->throttle.delay =  Z_DVAL_P(zoption);
			}
			zval_ptr_dtor(&zoption);
		}
		if ((zoption = get_option(r->options, ZEND_STRL("throttleChunk") TSRMLS_CC))) {
			if (Z_TYPE_P(zoption) == IS_LONG) {
				r->throttle.chunk = Z_LVAL_P(zoption);
			}
			zval_ptr_dtor(&zoption);
		}

		if (r->range.status == PHP_HTTP_RANGE_OK) {
			if (zend_hash_num_elements(&r->range.values) == 1) {
				/* single range */
				zval **range, **begin, **end;

				if (SUCCESS != zend_hash_index_find(&r->range.values, 0, (void *) &range)
				||	SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(range), 0, (void *) &begin)
				||	SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(range), 1, (void *) &end)
				) {
					/* this should never happen */
					zend_hash_destroy(&r->range.values);
					php_http_env_set_response_code(500 TSRMLS_CC);
					ret = FAILURE;
				} else {
					/* send chunk */
					php_http_message_body_to_callback(obj->body, (php_http_pass_callback_t) php_http_env_response_send_data, r, Z_LVAL_PP(begin), Z_LVAL_PP(end) - Z_LVAL_PP(begin) + 1);
					php_http_env_response_send_done(r);
					zend_hash_destroy(&r->range.values);
					ret = SUCCESS;
				}
			} else {
				/* send multipart/byte-ranges message */
				HashPosition pos;
				zval **chunk;

				FOREACH_HASH_VAL(pos, &r->range.values, chunk) {
					zval **begin, **end;

					if (IS_ARRAY == Z_TYPE_PP(chunk)
					&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(chunk), 0, (void *) &begin)
					&&	IS_LONG == Z_TYPE_PP(begin)
					&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(chunk), 1, (void *) &end)
					&&	IS_LONG == Z_TYPE_PP(end)
					) {
						php_http_buffer_appendf(r->buffer,
								PHP_HTTP_CRLF
								"--%s" PHP_HTTP_CRLF
								"Content-Type: %s" PHP_HTTP_CRLF
								"Content-Range: bytes %ld-%ld/%zu" PHP_HTTP_CRLF PHP_HTTP_CRLF,
								/* - */
								r->range.boundary,
								r->content.type ? r->content.type : "application/octet-stream",
								Z_LVAL_PP(begin),
								Z_LVAL_PP(end),
								r->content.length
						);
						php_http_message_body_to_callback(obj->body, (php_http_pass_callback_t) php_http_env_response_send_data, r, Z_LVAL_PP(begin), Z_LVAL_PP(end) - Z_LVAL_PP(begin) + 1);
					}
				}
				php_http_buffer_appendf(r->buffer, PHP_HTTP_CRLF "--%s--", r->range.boundary);
				php_http_env_response_send_done(r);
				zend_hash_destroy(&r->range.values);
			}

		} else {
			php_http_message_body_to_callback(obj->body, (php_http_pass_callback_t) php_http_env_response_send_data, r, 0, 0);
			php_http_env_response_send_done(r);
		}
	}
	if (zbody) {
		zval_ptr_dtor(&zbody);
	}
	return ret;
}

PHP_HTTP_API STATUS php_http_env_response_send(php_http_env_response_t *r)
{
	zval *zbody;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	/* check for ranges */
	if (	(zbody = get_option(r->options, ZEND_STRL("body") TSRMLS_CC))
	&&		(Z_TYPE_P(zbody) == IS_OBJECT)
	&&		instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_get_class_entry() TSRMLS_CC)
	) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(zbody TSRMLS_CC);

		r->content.length = php_http_message_body_size(obj->body);
		zval_ptr_dtor(&zbody);

		if (SUCCESS != php_http_env_set_response_header(0, ZEND_STRL("Accept-Ranges: bytes"), 1 TSRMLS_CC)) {
			return FAILURE;
		} else {
			zend_hash_init(&r->range.values, 0, NULL, ZVAL_PTR_DTOR, 0);
			r->range.status = php_http_env_get_request_ranges(&r->range.values, r->content.length TSRMLS_CC);

			switch (r->range.status) {
				case PHP_HTTP_RANGE_NO:
					zend_hash_destroy(&r->range.values);
					break;

				case PHP_HTTP_RANGE_ERR:
					if (php_http_env_got_request_header(ZEND_STRL("If-Range") TSRMLS_CC)) {
						r->range.status = PHP_HTTP_RANGE_NO;
						zend_hash_destroy(&r->range.values);
					} else {
						r->done = 1;
						zend_hash_destroy(&r->range.values);
						if (SUCCESS != php_http_env_set_response_header_format(416, 1 TSRMLS_CC, "Content-Range: bytes */%zu", r->content.length)) {
							return FAILURE;
						}
					}
					break;

				case PHP_HTTP_RANGE_OK:
					if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(r->options, ZEND_STRL("If-Range") TSRMLS_CC)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(r->options, ZEND_STRL("If-Range") TSRMLS_CC)
					) {
						r->range.status = PHP_HTTP_RANGE_NO;
						zend_hash_destroy(&r->range.values);
						break;
					}
					if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(r->options, ZEND_STRL("If-Match") TSRMLS_CC)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(r->options, ZEND_STRL("If-Unmodified-Since") TSRMLS_CC)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(r->options, ZEND_STRL("Unless-Modified-Since") TSRMLS_CC)
					) {
						r->done = 1;
						zend_hash_destroy(&r->range.values);
						if (SUCCESS != php_http_env_set_response_code(412 TSRMLS_CC)) {
							return FAILURE;
						}
						break;
					}

					break;
			}
		}
	} else if (zbody) {
		zval_ptr_dtor(&zbody);
	}

	if (SUCCESS != php_http_env_response_send_head(r)) {
		return FAILURE;
	}

	if (SUCCESS != php_http_env_response_send_body(r)) {
		return FAILURE;
	}
	return SUCCESS;
}

#undef PHP_HTTP_BEGIN_ARGS
#undef PHP_HTTP_EMPTY_ARGS
#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpEnvResponse, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpEnvResponse, method, 0)
#define PHP_HTTP_ENV_RESPONSE_ME(method, visibility)	PHP_ME(HttpEnvResponse, method, PHP_HTTP_ARGS(HttpEnvResponse, method), visibility)

PHP_HTTP_EMPTY_ARGS(__construct);

PHP_HTTP_BEGIN_ARGS(__invoke, 1)
	PHP_HTTP_ARG_VAL(ob_string, 0)
	PHP_HTTP_ARG_VAL(ob_flags, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setContentType, 1)
	PHP_HTTP_ARG_VAL(content_type, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setContentEncoding, 1)
	PHP_HTTP_ARG_VAL(content_encoding, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setContentDisposition, 1)
	PHP_HTTP_ARG_ARR(disposition_params, 1, 0)
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

static zend_class_entry *php_http_env_response_class_entry;

zend_class_entry *php_http_env_response_get_class_entry(void)
{
	return php_http_env_response_class_entry;
}

static zend_function_entry php_http_env_response_method_entry[] = {
	PHP_HTTP_ENV_RESPONSE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_ENV_RESPONSE_ME(__invoke, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setContentType, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setContentDisposition, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_RESPONSE_ME(setContentEncoding, ZEND_ACC_PUBLIC)
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
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				obj->message = php_http_message_init_env(obj->message, PHP_HTTP_RESPONSE TSRMLS_CC);
			} end_error_handling();
		}
	} end_error_handling();

}

PHP_METHOD(HttpEnvResponse, __invoke)
{
	char *ob_str;
	int ob_len;
	long ob_flags = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &ob_str, &ob_len, &ob_flags)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->body.handle || SUCCESS == php_http_new(&obj->body, php_http_message_body_get_class_entry(), (php_http_new_t) php_http_message_body_object_new_ex, NULL, (void *) php_http_message_body_copy(&obj->message->body, NULL, 0), NULL TSRMLS_CC)) {
			php_http_message_body_append(&obj->message->body, ob_str, ob_len);
			RETURN_TRUE;
		}
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpEnvResponse, setContentType)
{
	char *ct_str = NULL;
	int ct_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s!", &ct_str, &ct_len)) {
		set_option(getThis(), ZEND_STRL("contentType"), IS_STRING, ct_str, ct_len TSRMLS_CC);
	}
}

PHP_METHOD(HttpEnvResponse, setContentDisposition)
{
	zval *zdisposition;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &zdisposition)) {
		zend_update_property(Z_OBJCE_P(getThis()), getThis(), ZEND_STRL("contentDisposition"), zdisposition TSRMLS_CC);
	}
}

PHP_METHOD(HttpEnvResponse, setContentEncoding)
{
	long ce;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &ce)) {
		set_option(getThis(), ZEND_STRL("contentEncoding"), IS_LONG, &ce, 0 TSRMLS_CC);
	}
}

PHP_METHOD(HttpEnvResponse, setCacheControl)
{
	char *cc_str = NULL;
	int cc_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s!", &cc_str, &cc_len)) {
		set_option(getThis(), ZEND_STRL("cacheControl"), IS_STRING, cc_str, cc_len TSRMLS_CC);
	}
}

PHP_METHOD(HttpEnvResponse, setLastModified)
{
	long last_modified;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &last_modified)) {
		set_option(getThis(), ZEND_STRL("lastModified"), IS_LONG, &last_modified, 0 TSRMLS_CC);
	}
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
	char *etag_str = NULL;
	int etag_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s!", &etag_str, &etag_len)) {
		set_option(getThis(), ZEND_STRL("etag"), IS_STRING, etag_str, etag_len TSRMLS_CC);
	}
}

PHP_METHOD(HttpEnvResponse, isCachedByEtag)
{
	char *header_name_str = NULL;
	int header_name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &header_name_str, &header_name_len)) {
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
		set_option(getThis(), ZEND_STRL("throttleDelay"), IS_DOUBLE, &delay, 0 TSRMLS_CC);
		set_option(getThis(), ZEND_STRL("throttleChunk"), IS_LONG, &chunk_size, 0 TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnvResponse, send)
{
	RETVAL_FALSE;

	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_env_response_t *r = php_http_env_response_init(NULL, getThis() TSRMLS_CC);

		if (r) {
			RETVAL_SUCCESS(php_http_env_response_send(r));
		}

		php_http_env_response_free(&r);
	}
}

PHP_MINIT_FUNCTION(http_env_response)
{
	PHP_HTTP_REGISTER_CLASS(http\\Env, Response, http_env_response, php_http_message_get_class_entry(), 0);

	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CONTENT_ENCODING_NONE"), PHP_HTTP_CONTENT_ENCODING_NONE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CONTENT_ENCODING_GZIP"), PHP_HTTP_CONTENT_ENCODING_GZIP TSRMLS_CC);

	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_NO"), PHP_HTTP_CACHE_NO TSRMLS_CC);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_HIT"), PHP_HTTP_CACHE_HIT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_MISS"), PHP_HTTP_CACHE_MISS TSRMLS_CC);

	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("contentType"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("contentDisposition"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("contentEncoding"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("cacheControl"), ZEND_ACC_PROTECTED TSRMLS_CC);
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
