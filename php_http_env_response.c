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

static void set_option(zval *options, const char *name_str, size_t name_len, int type, void *value_ptr, size_t value_len TSRMLS_DC)
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
				case IS_OBJECT:
					zend_update_property(Z_OBJCE_P(options), options, name, name_len, value_ptr TSRMLS_CC);
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
				case IS_OBJECT:
					Z_ADDREF_P(value_ptr);
					add_assoc_zval_ex(options, name_str, name_len + 1, value_ptr);
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
static php_http_message_body_t *get_body(zval *options TSRMLS_DC)
{
	zval *zbody;
	php_http_message_body_t *body = NULL;

	if ((zbody = get_option(options, ZEND_STRL("body") TSRMLS_CC))) {
		if ((Z_TYPE_P(zbody) == IS_OBJECT) && instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_get_class_entry() TSRMLS_CC)) {
			php_http_message_body_object_t *body_obj = zend_object_store_get_object(zbody TSRMLS_CC);

			body = body_obj->body;
		}
		zval_ptr_dtor(&zbody);
	}

	return body;
}
static php_http_message_t *get_request(zval *options TSRMLS_DC)
{
	zval *zrequest;
	php_http_message_t *request = NULL;

	if ((zrequest = get_option(options, ZEND_STRL("request") TSRMLS_CC))) {
		if (Z_TYPE_P(zrequest) == IS_OBJECT && instanceof_function(Z_OBJCE_P(zrequest), php_http_message_get_class_entry() TSRMLS_CC)) {
			php_http_message_object_t *request_obj = zend_object_store_get_object(zrequest TSRMLS_CC);

			request = request_obj->message;
		}
		zval_ptr_dtor(&zrequest);
	}

	return request;
}

PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_etag(zval *options, const char *header_str, size_t header_len, php_http_message_t *request TSRMLS_DC)
{
	php_http_cache_status_t ret = PHP_HTTP_CACHE_NO;
	int free_etag = 0;
	char *header = NULL, *etag;
	php_http_message_body_t *body;
	zval *zetag;


	if (!(body = get_body(options TSRMLS_CC))) {
		return ret;
	}

	if ((zetag = get_option(options, ZEND_STRL("etag") TSRMLS_CC))) {
		zval *zetag_copy = php_http_ztyp(IS_STRING, zetag);
		zval_ptr_dtor(&zetag);
		zetag = zetag_copy;
	}

	if (zetag && Z_STRLEN_P(zetag)) {
		etag = Z_STRVAL_P(zetag);
	} else if ((etag = php_http_message_body_etag(body))) {
		set_option(options, ZEND_STRL("etag"), IS_STRING, etag, strlen(etag) TSRMLS_CC);
		free_etag = 1;
	}

	if (zetag) {
		zval_ptr_dtor(&zetag);
	}

	if (etag && (header = php_http_env_get_request_header(header_str, header_len, NULL, request TSRMLS_CC))) {
		ret = php_http_match(header, etag, PHP_HTTP_MATCH_WORD)  ? PHP_HTTP_CACHE_HIT : PHP_HTTP_CACHE_MISS;
	}

	if (free_etag) {
		efree(etag);
	}

	STR_FREE(header);
	return ret;
}

PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_last_modified(zval *options, const char *header_str, size_t header_len, php_http_message_t *request TSRMLS_DC)
{
	php_http_cache_status_t ret = PHP_HTTP_CACHE_NO;
	char *header;
	time_t ums, lm = 0;
	php_http_message_body_t *body;
	zval *zlm;

	if (!(body = get_body(options TSRMLS_CC))) {
		return ret;
	}

	if ((zlm = get_option(options, ZEND_STRL("lastModified") TSRMLS_CC))) {
		zval *zlm_copy = php_http_ztyp(IS_LONG, zlm);
		zval_ptr_dtor(&zlm);
		zlm = zlm_copy;
	}

	if (zlm && Z_LVAL_P(zlm) > 0) {
		lm = Z_LVAL_P(zlm);
	} else {
		lm = php_http_message_body_mtime(body);
		set_option(options, ZEND_STRL("lastModified"), IS_LONG, &lm, 0 TSRMLS_CC);
	}

	if (zlm) {
		zval_ptr_dtor(&zlm);
	}

	if ((header = php_http_env_get_request_header(header_str, header_len, NULL, request TSRMLS_CC))) {
		ums = php_parse_date(header, NULL);

		if (ums > 0 && ums >= lm) {
			ret = PHP_HTTP_CACHE_HIT;
		} else {
			ret = PHP_HTTP_CACHE_MISS;
		}
	}

	STR_FREE(header);
	return ret;
}

static zend_bool php_http_env_response_is_cacheable(php_http_env_response_t *r, php_http_message_t *request)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (r->ops->get_status(r) >= 400) {
		return 0;
	}

	if (php_http_env_got_request_header(ZEND_STRL("Authorizsation"), request TSRMLS_CC)) {
		return 0;
	}

	if (-1 == php_http_select_str(php_http_env_get_request_method(request TSRMLS_CC), 2, "HEAD", "GET")) {
		return 0;
	}

	return 1;
}

static size_t output(void *context, char *buf, size_t len TSRMLS_DC)
{
	php_http_env_response_t *r = context;

	r->ops->write(r, buf, len);

	/*	we really only need to flush when throttling is enabled,
		because we push the data as fast as possible anyway if not */
	if (r->throttle.delay >= PHP_HTTP_DIFFSEC) {
		r->ops->flush(r);
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

PHP_HTTP_API php_http_env_response_t *php_http_env_response_init(php_http_env_response_t *r, zval *options, php_http_env_response_ops_t *ops, void *init_arg TSRMLS_DC)
{
	zend_bool free_r;

	if ((free_r = !r)) {
		r = emalloc(sizeof(*r));
	}
	memset(r, 0, sizeof(*r));

	if (ops) {
		r->ops = ops;
	} else {
		r->ops = php_http_env_response_get_sapi_ops();
	}

	r->buffer = php_http_buffer_init(NULL);

	Z_ADDREF_P(options);
	r->options = options;

	TSRMLS_SET_CTX(r->ts);

	if (r->ops->init && (SUCCESS != r->ops->init(r, init_arg))) {
		if (free_r) {
			php_http_env_response_free(&r);
		} else {
			php_http_env_response_dtor(r);
			r = NULL;
		}
	}

	return r;
}

PHP_HTTP_API void php_http_env_response_dtor(php_http_env_response_t *r)
{
	if (r->ops->dtor) {
		r->ops->dtor(r);
	}
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

static STATUS php_http_env_response_send_head(php_http_env_response_t *r, php_http_message_t *request)
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
			ret = r->ops->set_status(r, Z_LVAL_P(zoption_copy));
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
			ret = r->ops->set_protocol_version(r, &v);
			php_http_version_dtor(&v);
		}
		zval_ptr_dtor(&zoption_copy);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if ((zoption = get_option(options, ZEND_STRL("headers") TSRMLS_CC))) {
		if (Z_TYPE_P(zoption) == IS_ARRAY) {
			php_http_headers_to_callback(Z_ARRVAL_P(zoption), 0, r->ops->set_header, r TSRMLS_CC);
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
				if (SUCCESS == (ret = r->ops->set_header(r, "Content-Type: %.*s", Z_STRLEN_P(zoption_copy), Z_STRVAL_P(zoption_copy)))) {
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

			if (	1 == php_http_array_list(&r->range.values TSRMLS_CC, 1, &range)
				&&	2 == php_http_array_list(Z_ARRVAL_PP(range) TSRMLS_CC, 2, &begin, &end)
			) {
				if (SUCCESS == (ret = r->ops->set_status(r, 206))) {
					ret = r->ops->set_header(r, "Content-Range: bytes %ld-%ld/%zu", Z_LVAL_PP(begin), Z_LVAL_PP(end), r->content.length);
				}
			} else {
				/* this should never happen */
				zend_hash_destroy(&r->range.values);
				ret = FAILURE;
			}
		} else {
			php_http_boundary(r->range.boundary, sizeof(r->range.boundary) TSRMLS_CC);
			if (SUCCESS == (ret = r->ops->set_status(r, 206))) {
				ret = r->ops->set_header(r, "Content-Type: multipart/byteranges; boundary=%s", r->range.boundary);
			}
		}
	} else {
		if ((zoption = get_option(options, ZEND_STRL("cacheControl") TSRMLS_CC))) {
			zval *zoption_copy = php_http_ztyp(IS_STRING, zoption);

			zval_ptr_dtor(&zoption);
			if (Z_STRLEN_P(zoption_copy)) {
				ret = r->ops->set_header(r, "Cache-Control: %.*s", Z_STRLEN_P(zoption_copy), Z_STRVAL_P(zoption_copy));
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
				if (buf.used) {
					ret = r->ops->set_header(r, "Content-Disposition: %.*s", buf.used, buf.data);
				}
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

					if ((result = php_http_negotiate_encoding(Z_ARRVAL(zsupported), request TSRMLS_CC))) {
						char *key_str = NULL;
						uint key_len = 0;

						zend_hash_internal_pointer_reset(result);
						if (HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(result, &key_str, &key_len, NULL, 0, NULL)) {
							if (!strcmp(key_str, "gzip")) {
								if (!(r->content.encoder = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_deflate_ops(), PHP_HTTP_DEFLATE_TYPE_GZIP TSRMLS_CC))) {
									ret = FAILURE;
								} else if (SUCCESS == (ret = r->ops->set_header(r, "Content-Encoding: gzip"))) {
									r->content.encoding = estrndup(key_str, key_len - 1);
								}
							} else if (!strcmp(key_str, "deflate")) {
								if (!(r->content.encoder = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_deflate_ops(), PHP_HTTP_DEFLATE_TYPE_ZLIB TSRMLS_CC))) {
									ret = FAILURE;
								} else if (SUCCESS == (ret = r->ops->set_header(r, "Content-Encoding: deflate"))) {
									r->content.encoding = estrndup(key_str, key_len - 1);
								}
							} else {
								ret = r->ops->del_header(r, ZEND_STRL("Content-Encoding"));
							}

							if (SUCCESS == ret) {
								ret = r->ops->add_header(r, "Vary: Accept-Encoding");
							}
						}

						zend_hash_destroy(result);
						FREE_HASHTABLE(result);
					}

					zval_dtor(&zsupported);
					break;

				case PHP_HTTP_CONTENT_ENCODING_NONE:
				default:
					ret = r->ops->del_header(r, ZEND_STRL("Content-Encoding"));
					break;
			}
			zval_ptr_dtor(&zoption_copy);
		}

		if (SUCCESS != ret) {
			return ret;
		}

		if (php_http_env_response_is_cacheable(r, request)) {
			switch (php_http_env_is_response_cached_by_etag(options, ZEND_STRL("If-None-Match"), request TSRMLS_CC)) {
				case PHP_HTTP_CACHE_MISS:
					break;

				case PHP_HTTP_CACHE_NO:
					if (PHP_HTTP_CACHE_HIT != php_http_env_is_response_cached_by_last_modified(options, ZEND_STRL("If-Modified-Since"), request TSRMLS_CC)) {
						break;
					}
					/*  no break */

				case PHP_HTTP_CACHE_HIT:
					ret = r->ops->set_status(r, 304);
					r->done = 1;
					break;
			}

			if ((zoption = get_option(options, ZEND_STRL("etag") TSRMLS_CC))) {
				zval *zoption_copy = php_http_ztyp(IS_STRING, zoption);

				zval_ptr_dtor(&zoption);
				if (*Z_STRVAL_P(zoption_copy) != '"' &&	strncmp(Z_STRVAL_P(zoption_copy), "W/\"", 3)) {
					ret = r->ops->set_header(r, "ETag: \"%s\"", Z_STRVAL_P(zoption_copy));
				} else {
					ret = r->ops->set_header(r, "ETag: %s", Z_STRVAL_P(zoption_copy));
				}
				zval_ptr_dtor(&zoption_copy);
			}
			if ((zoption = get_option(options, ZEND_STRL("lastModified") TSRMLS_CC))) {
				zval *zoption_copy = php_http_ztyp(IS_LONG, zoption);

				zval_ptr_dtor(&zoption);
				if (Z_LVAL_P(zoption_copy)) {
					char *date = php_format_date(ZEND_STRL(PHP_HTTP_DATE_FORMAT), Z_LVAL_P(zoption_copy), 0 TSRMLS_CC);
					if (date) {
						ret = r->ops->set_header(r, "Last-Modified: %s", date);
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
	zval *zoption;
	php_http_message_body_t *body;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (r->done) {
		return ret;
	}

	if ((body = get_body(r->options TSRMLS_CC))) {
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

				if (	1 == php_http_array_list(&r->range.values TSRMLS_CC, 1, &range)
					&&	2 == php_http_array_list(Z_ARRVAL_PP(range) TSRMLS_CC, 2, &begin, &end)
				) {
					/* send chunk */
					php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_env_response_send_data, r, Z_LVAL_PP(begin), Z_LVAL_PP(end) - Z_LVAL_PP(begin) + 1);
					php_http_env_response_send_done(r);
					zend_hash_destroy(&r->range.values);
					ret = SUCCESS;
				} else {
					/* this should never happen */
					zend_hash_destroy(&r->range.values);
					r->ops->set_status(r, 500);
					ret = FAILURE;
				}

			} else {
				/* send multipart/byte-ranges message */
				HashPosition pos;
				zval **chunk;

				FOREACH_HASH_VAL(pos, &r->range.values, chunk) {
					zval **begin, **end;

					if (2 == php_http_array_list(Z_ARRVAL_PP(chunk) TSRMLS_CC, 2, &begin, &end)) {
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
						php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_env_response_send_data, r, Z_LVAL_PP(begin), Z_LVAL_PP(end) - Z_LVAL_PP(begin) + 1);
					}
				}
				php_http_buffer_appendf(r->buffer, PHP_HTTP_CRLF "--%s--", r->range.boundary);
				php_http_env_response_send_done(r);
				zend_hash_destroy(&r->range.values);
			}

		} else {
			php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_env_response_send_data, r, 0, 0);
			php_http_env_response_send_done(r);
		}
	}
	return ret;
}

PHP_HTTP_API STATUS php_http_env_response_send(php_http_env_response_t *r)
{
	php_http_message_t *request;
	php_http_message_body_t *body;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	request = get_request(r->options TSRMLS_CC);

	/* check for ranges */
	if ((body = get_body(r->options TSRMLS_CC))) {
		r->content.length = php_http_message_body_size(body);

		if (SUCCESS != r->ops->set_header(r, "Accept-Ranges: bytes")) {
			return FAILURE;
		} else {
			zend_hash_init(&r->range.values, 0, NULL, ZVAL_PTR_DTOR, 0);
			r->range.status = php_http_env_get_request_ranges(&r->range.values, r->content.length, request TSRMLS_CC);

			switch (r->range.status) {
				case PHP_HTTP_RANGE_NO:
					zend_hash_destroy(&r->range.values);
					break;

				case PHP_HTTP_RANGE_ERR:
					if (php_http_env_got_request_header(ZEND_STRL("If-Range"), request TSRMLS_CC)) {
						r->range.status = PHP_HTTP_RANGE_NO;
						zend_hash_destroy(&r->range.values);
					} else {
						r->done = 1;
						zend_hash_destroy(&r->range.values);
						if (SUCCESS != r->ops->set_status(r, 416)) {
							return FAILURE;
						}
						if (SUCCESS != r->ops->set_header(r, "Content-Range: bytes */%zu", r->content.length)) {
							return FAILURE;
						}
					}
					break;

				case PHP_HTTP_RANGE_OK:
					if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(r->options, ZEND_STRL("If-Range"), request TSRMLS_CC)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(r->options, ZEND_STRL("If-Range"), request TSRMLS_CC)
					) {
						r->range.status = PHP_HTTP_RANGE_NO;
						zend_hash_destroy(&r->range.values);
						break;
					}
					if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(r->options, ZEND_STRL("If-Match"), request TSRMLS_CC)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(r->options, ZEND_STRL("If-Unmodified-Since"), request TSRMLS_CC)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(r->options, ZEND_STRL("Unless-Modified-Since"), request TSRMLS_CC)
					) {
						r->done = 1;
						zend_hash_destroy(&r->range.values);
						if (SUCCESS != r->ops->set_status(r, 412)) {
							return FAILURE;
						}
						break;
					}

					break;
			}
		}
	}

	if (SUCCESS != php_http_env_response_send_head(r, request)) {
		return FAILURE;
	}

	if (SUCCESS != php_http_env_response_send_body(r)) {
		return FAILURE;
	}

	if (SUCCESS != r->ops->finish(r)) {
		return FAILURE;
	}

	return SUCCESS;
}

static long php_http_env_response_sapi_get_status(php_http_env_response_t *r)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

	return php_http_env_get_response_code(TSRMLS_C);
}
static STATUS php_http_env_response_sapi_set_status(php_http_env_response_t *r, long http_code)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

	return php_http_env_set_response_code(http_code TSRMLS_CC);
}
static STATUS php_http_env_response_sapi_set_protocol_version(php_http_env_response_t *r, php_http_version_t *v)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

	return php_http_env_set_response_protocol_version(v TSRMLS_CC);
}
static STATUS php_http_env_response_sapi_set_header(php_http_env_response_t *r, const char *fmt, ...)
{
	STATUS ret;
	va_list args;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	va_start(args, fmt);
	ret = php_http_env_set_response_header_va(0, 1, fmt, args TSRMLS_CC);
	va_end(args);

	return ret;
}
static STATUS php_http_env_response_sapi_add_header(php_http_env_response_t *r, const char *fmt, ...)
{
	STATUS ret;
	va_list args;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	va_start(args, fmt);
	ret = php_http_env_set_response_header_va(0, 0, fmt, args TSRMLS_CC);
	va_end(args);

	return ret;
}
static STATUS php_http_env_response_sapi_del_header(php_http_env_response_t *r, const char *header_str, size_t header_len)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

	return php_http_env_set_response_header_value(0, header_str, header_len, NULL, 1 TSRMLS_CC);
}
static STATUS php_http_env_response_sapi_write(php_http_env_response_t *r, const char *data_str, size_t data_len)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (0 < PHPWRITE(data_str, data_len)) {
		return SUCCESS;
	}
	return FAILURE;
}
static STATUS php_http_env_response_sapi_flush(php_http_env_response_t *r)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

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

	return SUCCESS;
}
static STATUS php_http_env_response_sapi_finish(php_http_env_response_t *r)
{
	return SUCCESS;
}

static php_http_env_response_ops_t php_http_env_response_sapi_ops = {
	NULL,
	NULL,
	php_http_env_response_sapi_get_status,
	php_http_env_response_sapi_set_status,
	php_http_env_response_sapi_set_protocol_version,
	php_http_env_response_sapi_set_header,
	php_http_env_response_sapi_add_header,
	php_http_env_response_sapi_del_header,
	php_http_env_response_sapi_write,
	php_http_env_response_sapi_flush,
	php_http_env_response_sapi_finish
};

PHP_HTTP_API php_http_env_response_ops_t *php_http_env_response_get_sapi_ops(void)
{
	return &php_http_env_response_sapi_ops;
}

typedef struct php_http_env_response_stream_ctx {
	HashTable header;
	php_http_version_t version;
	long status_code;

	php_stream *stream;

	unsigned started:1;
	unsigned finished:1;
} php_http_env_response_stream_ctx_t;

static STATUS php_http_env_response_stream_init(php_http_env_response_t *r, void *init_arg)
{
	php_http_env_response_stream_ctx_t *ctx;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	ctx = ecalloc(1, sizeof(*ctx));

	ctx->stream = init_arg;
	if (SUCCESS != zend_list_addref(ctx->stream->rsrc_id)) {
		efree(ctx);
		return FAILURE;
	}
	zend_hash_init(&ctx->header, 0, NULL, ZVAL_PTR_DTOR, 0);
	php_http_version_init(&ctx->version, 1, 1 TSRMLS_CC);
	ctx->status_code = 200;

	r->ctx = ctx;

	return SUCCESS;
}
static void php_http_env_response_stream_dtor(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *ctx = r->ctx;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	zend_hash_destroy(&ctx->header);
	zend_list_delete(ctx->stream->rsrc_id);
	efree(ctx);
	r->ctx = NULL;
}
static void php_http_env_response_stream_header(php_http_env_response_stream_ctx_t *ctx, HashTable *header TSRMLS_DC)
{
	HashPosition pos;
	zval **val;

	FOREACH_HASH_VAL(pos, &ctx->header, val) {
		if (Z_TYPE_PP(val) == IS_ARRAY) {
			php_http_env_response_stream_header(ctx, Z_ARRVAL_PP(val) TSRMLS_CC);
		} else {
			php_stream_write(ctx->stream, Z_STRVAL_PP(val), Z_STRLEN_PP(val));
			php_stream_write_string(ctx->stream, PHP_HTTP_CRLF);
		}
	}
}
static STATUS php_http_env_response_stream_start(php_http_env_response_stream_ctx_t *ctx TSRMLS_DC)
{
	if (ctx->started || ctx->finished) {
		return FAILURE;
	}

	php_stream_printf(ctx->stream TSRMLS_CC, "HTTP/%u.%u %ld %s" PHP_HTTP_CRLF, ctx->version.major, ctx->version.minor, ctx->status_code, php_http_env_get_response_status_for_code(ctx->status_code));
	php_http_env_response_stream_header(ctx, &ctx->header TSRMLS_CC);
	php_stream_write_string(ctx->stream, PHP_HTTP_CRLF);
	ctx->started = 1;
	return SUCCESS;
}
static long php_http_env_response_stream_get_status(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *ctx = r->ctx;

	return ctx->status_code;
}
static STATUS php_http_env_response_stream_set_status(php_http_env_response_t *r, long http_code)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->started || stream_ctx->finished) {
		return FAILURE;
	}

	stream_ctx->status_code = http_code;

	return SUCCESS;
}
static STATUS php_http_env_response_stream_set_protocol_version(php_http_env_response_t *r, php_http_version_t *v)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->started || stream_ctx->finished) {
		return FAILURE;
	}

	memcpy(&stream_ctx->version, v, sizeof(stream_ctx->version));

	return SUCCESS;
}
static STATUS php_http_env_response_stream_set_header_ex(php_http_env_response_t *r, zend_bool replace, const char *fmt, va_list argv)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;
	char *header_end, *header_str = NULL;
	size_t header_len = 0;
	zval *zheader, **zheader_ptr;

	if (stream_ctx->started || stream_ctx->finished) {
		return FAILURE;
	}

	header_len = vspprintf(&header_str, 0, fmt, argv);

	if (!(header_end = strchr(header_str, ':'))) {
		efree(header_str);
		return FAILURE;
	}

	*header_end = '\0';

	if (!replace && (SUCCESS == zend_hash_find(&stream_ctx->header, header_str, header_end - header_str + 1, (void *) &zheader_ptr))) {
		convert_to_array(*zheader_ptr);
		*header_end = ':';
		return add_next_index_stringl(*zheader_ptr, header_str, header_len, 0);
	} else {
		MAKE_STD_ZVAL(zheader);
		ZVAL_STRINGL(zheader, header_str, header_len, 0);

		if (SUCCESS != zend_hash_update(&stream_ctx->header, header_str, header_end - header_str + 1, (void *) &zheader, sizeof(zval *), NULL)) {
			zval_ptr_dtor(&zheader);
			return FAILURE;
		}

		*header_end = ':';
		return SUCCESS;
	}
}
static STATUS php_http_env_response_stream_set_header(php_http_env_response_t *r, const char *fmt, ...)
{
	STATUS ret;
	va_list argv;

	va_start(argv, fmt);
	ret = php_http_env_response_stream_set_header_ex(r, 1, fmt, argv);
	va_end(argv);

	return ret;
}
static STATUS php_http_env_response_stream_add_header(php_http_env_response_t *r, const char *fmt, ...)
{
	STATUS ret;
	va_list argv;

	va_start(argv, fmt);
	ret = php_http_env_response_stream_set_header_ex(r, 0, fmt, argv);
	va_end(argv);

	return ret;
}
static STATUS php_http_env_response_stream_del_header(php_http_env_response_t *r, const char *header_str, size_t header_len)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->started || stream_ctx->finished) {
		return FAILURE;
	}

	zend_hash_del(&stream_ctx->header, header_str, header_len + 1);
	return SUCCESS;
}
static STATUS php_http_env_response_stream_write(php_http_env_response_t *r, const char *data_str, size_t data_len)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (stream_ctx->finished) {
		return FAILURE;
	}
	if (!stream_ctx->started) {
		if (SUCCESS != php_http_env_response_stream_start(stream_ctx TSRMLS_CC)) {
			return FAILURE;
		}
	}

	php_stream_write(stream_ctx->stream, data_str, data_len);

	return SUCCESS;
}
static STATUS php_http_env_response_stream_flush(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (stream_ctx->finished) {
		return FAILURE;
	}
	if (!stream_ctx->started) {
		if (SUCCESS != php_http_env_response_stream_start(stream_ctx TSRMLS_CC)) {
			return FAILURE;
		}
	}

	return php_stream_flush(stream_ctx->stream);
}
static STATUS php_http_env_response_stream_finish(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (stream_ctx->finished) {
		return FAILURE;
	}
	if (!stream_ctx->started) {
		if (SUCCESS != php_http_env_response_stream_start(stream_ctx TSRMLS_CC)) {
			return FAILURE;
		}
	}

	stream_ctx->finished = 1;

	return SUCCESS;
}

static php_http_env_response_ops_t php_http_env_response_stream_ops = {
	php_http_env_response_stream_init,
	php_http_env_response_stream_dtor,
	php_http_env_response_stream_get_status,
	php_http_env_response_stream_set_status,
	php_http_env_response_stream_set_protocol_version,
	php_http_env_response_stream_set_header,
	php_http_env_response_stream_add_header,
	php_http_env_response_stream_del_header,
	php_http_env_response_stream_write,
	php_http_env_response_stream_flush,
	php_http_env_response_stream_finish
};

PHP_HTTP_API php_http_env_response_ops_t *php_http_env_response_get_stream_ops(void)
{
	return &php_http_env_response_stream_ops;
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

PHP_HTTP_BEGIN_ARGS(setEnvRequest, 1)
	PHP_HTTP_ARG_OBJ(http\\Message, env_request, 1)
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
	PHP_HTTP_ENV_RESPONSE_ME(setEnvRequest, ZEND_ACC_PUBLIC)
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

PHP_METHOD(HttpEnvResponse, setEnvRequest)
{
	zval *env_req = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|O", &env_req, php_http_message_get_class_entry())) {
		set_option(getThis(), ZEND_STRL("request"), IS_OBJECT, env_req, 0 TSRMLS_CC);
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

		RETURN_LONG(php_http_env_is_response_cached_by_last_modified(getThis(), header_name_str, header_name_len, get_request(getThis() TSRMLS_CC) TSRMLS_CC));
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
		RETURN_LONG(php_http_env_is_response_cached_by_etag(getThis(), header_name_str, header_name_len, get_request(getThis() TSRMLS_CC) TSRMLS_CC));
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
	zval *zstream = NULL;
	php_stream *s = NULL;

	RETVAL_FALSE;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|r", &zstream)) {
		if (zstream) {
			php_stream_from_zval_no_verify(s, &zstream);

			if (s) {
				php_http_env_response_t *r;

				if ((r = php_http_env_response_init(NULL, getThis(), php_http_env_response_get_stream_ops(), s TSRMLS_CC))) {
					RETVAL_SUCCESS(php_http_env_response_send(r));
					php_http_env_response_free(&r);
				}
			}
		} else {
			php_http_env_response_t r;

			if (php_http_env_response_init(&r, getThis(), NULL, NULL TSRMLS_CC)) {
				RETVAL_SUCCESS(php_http_env_response_send(&r));
				php_http_env_response_dtor(&r);
			}
		}
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

	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("request"), ZEND_ACC_PROTECTED TSRMLS_CC);
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
