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

static void set_option(zval *options, const char *name_str, size_t name_len, int type, void *value_ptr, size_t value_len)
{
	if (Z_TYPE_P(options) == IS_OBJECT) {
		if (EXPECTED(value_ptr)) {
			switch (type) {
				case IS_DOUBLE:
					zend_update_property_double(Z_OBJCE_P(options), options, name_str, name_len, *(double *)value_ptr);
					break;
				case IS_LONG:
					zend_update_property_long(Z_OBJCE_P(options), options, name_str, name_len, *(zend_long *)value_ptr);
					break;
				case IS_STRING:
					zend_update_property_stringl(Z_OBJCE_P(options), options, name_str, name_len, value_ptr, value_len);
					break;
				case IS_ARRAY:
				case IS_OBJECT:
					zend_update_property(Z_OBJCE_P(options), options, name_str, name_len, value_ptr);
					break;
			}
		} else {
			zend_update_property_null(Z_OBJCE_P(options), options, name_str, name_len);
		}
	} else {
		convert_to_array(options);
		if (EXPECTED(value_ptr)) {
			switch (type) {
				case IS_DOUBLE:
					add_assoc_double_ex(options, name_str, name_len, *(double *)value_ptr);
					break;
				case IS_LONG:
					add_assoc_long_ex(options, name_str, name_len, *(zend_long *)value_ptr);
					break;
				case IS_STRING: {
					zend_string *value = zend_string_init(value_ptr, value_len, 0);
					add_assoc_str_ex(options, name_str, name_len, value);
					break;
				case IS_ARRAY:
				case IS_OBJECT:
					Z_ADDREF_P(value_ptr);
					add_assoc_zval_ex(options, name_str, name_len, value_ptr);
					break;
				}
			}
		} else {
			add_assoc_null_ex(options, name_str, name_len);
		}
	}
}
static zval *get_option(zval *options, const char *name_str, size_t name_len, zval *tmp)
{
	zval *val = NULL;

	if (EXPECTED(Z_TYPE_P(options) == IS_OBJECT)) {
		val = zend_read_property(Z_OBJCE_P(options), options, name_str, name_len, 0, tmp);
	} else if (EXPECTED(Z_TYPE_P(options) == IS_ARRAY)) {
		val = zend_symtable_str_find(Z_ARRVAL_P(options), name_str, name_len);
	} else {
		abort();
	}
	if (val) {
		Z_TRY_ADDREF_P(val);
	}
	return val;
}
static php_http_message_body_t *get_body(zval *options)
{
	zval zbody_tmp, *zbody;
	php_http_message_body_t *body = NULL;

	if (EXPECTED(zbody = get_option(options, ZEND_STRL("body"), &zbody_tmp))) {
		if ((Z_TYPE_P(zbody) == IS_OBJECT) && instanceof_function(Z_OBJCE_P(zbody), php_http_get_message_body_class_entry())) {
			php_http_message_body_object_t *body_obj = PHP_HTTP_OBJ(NULL, zbody);

			body = body_obj->body;
		}
		Z_TRY_DELREF_P(zbody);
	}

	return body;
}
static php_http_message_t *get_request(zval *options)
{
	zval zrequest_tmp, *zrequest;
	php_http_message_t *request = NULL;

	if (EXPECTED(zrequest = get_option(options, ZEND_STRL("request"), &zrequest_tmp))) {
		if (UNEXPECTED(Z_TYPE_P(zrequest) == IS_OBJECT && instanceof_function(Z_OBJCE_P(zrequest), php_http_message_get_class_entry()))) {
			php_http_message_object_t *request_obj = PHP_HTTP_OBJ(NULL, zrequest);

			request = request_obj->message;
		}
		Z_TRY_DELREF_P(zrequest);
	}

	return request;
}
static void set_cookie(zval *options, zval *zcookie_new)
{
	zval tmp, zcookies_set_tmp, *zcookies_set;
	php_http_arrkey_t key;
	php_http_cookie_object_t *obj = PHP_HTTP_OBJ(NULL, zcookie_new);

	array_init(&tmp);
	zcookies_set = get_option(options, ZEND_STRL("cookies"), &zcookies_set_tmp);
	if (zcookies_set && Z_TYPE_P(zcookies_set) == IS_ARRAY) {
		array_copy(Z_ARRVAL_P(zcookies_set), Z_ARRVAL(tmp));
		zval_ptr_dtor(zcookies_set);
	}

	ZEND_HASH_FOREACH_KEY(&obj->list->cookies, key.h, key.key)
	{
		Z_ADDREF_P(zcookie_new);
		if (key.key) {
			add_assoc_zval_ex(&tmp, key.key->val, key.key->len, zcookie_new);
		} else {
			add_index_zval(&tmp, key.h, zcookie_new);
		}
	}
	ZEND_HASH_FOREACH_END();

	set_option(options, ZEND_STRL("cookies"), IS_ARRAY, &tmp, 0);
	zval_ptr_dtor(&tmp);
}

php_http_cache_status_t php_http_env_is_response_cached_by_etag(zval *options, const char *header_str, size_t header_len, php_http_message_t *request)
{
	php_http_cache_status_t ret = PHP_HTTP_CACHE_NO;
	char *header = NULL, *etag = NULL;
	php_http_message_body_t *body;
	zval zetag_tmp, *zetag;


	if (UNEXPECTED(!(body = get_body(options)))) {
		return ret;
	}

	if (EXPECTED(zetag = get_option(options, ZEND_STRL("etag"), &zetag_tmp)) && Z_TYPE_P(zetag) != IS_NULL) {
		zend_string *zs = zval_get_string(zetag);
		etag = estrndup(zs->val, zs->len);
		zend_string_release(zs);
		zval_ptr_dtor(zetag);
	}

	if (!etag && (etag = php_http_message_body_etag(body))) {
		set_option(options, ZEND_STRL("etag"), IS_STRING, etag, strlen(etag));
	}

	if (etag && (header = php_http_env_get_request_header(header_str, header_len, NULL, request))) {
		ret = php_http_match(header, etag, PHP_HTTP_MATCH_WORD) ? PHP_HTTP_CACHE_HIT : PHP_HTTP_CACHE_MISS;
	}

	PTR_FREE(etag);
	PTR_FREE(header);

	return ret;
}

php_http_cache_status_t php_http_env_is_response_cached_by_last_modified(zval *options, const char *header_str, size_t header_len, php_http_message_t *request)
{
	php_http_cache_status_t ret = PHP_HTTP_CACHE_NO;
	char *header;
	time_t ums, lm = 0;
	php_http_message_body_t *body;
	zval zlm_tmp, *zlm;

	if (UNEXPECTED(!(body = get_body(options)))) {
		return ret;
	}

	if (EXPECTED(zlm = get_option(options, ZEND_STRL("lastModified"), &zlm_tmp))) {
		lm = zval_get_long(zlm);
		zval_ptr_dtor(zlm);
	}

	if (EXPECTED(lm <= 0)) {
		lm = php_http_message_body_mtime(body);
		set_option(options, ZEND_STRL("lastModified"), IS_LONG, &lm, 0);
	}

	if ((header = php_http_env_get_request_header(header_str, header_len, NULL, request))) {
		ums = php_parse_date(header, NULL);

		if (ums > 0 && ums >= lm) {
			ret = PHP_HTTP_CACHE_HIT;
		} else {
			ret = PHP_HTTP_CACHE_MISS;
		}
	}

	PTR_FREE(header);
	return ret;
}

static zend_bool php_http_env_response_is_cacheable(php_http_env_response_t *r, php_http_message_t *request)
{
	long status = r->ops->get_status(r);

	if (status && status / 100 != 2) {
		return 0;
	}

	if (php_http_env_got_request_header(ZEND_STRL("Authorization"), request)) {
		return 0;
	}

	if (-1 == php_http_select_str(php_http_env_get_request_method(request), 2, "HEAD", "GET")) {
		return 0;
	}

	return 1;
}

static size_t output(void *context, char *buf, size_t len)
{
	php_http_env_response_t *r = context;

	if (UNEXPECTED(SUCCESS != r->ops->write(r, buf, len))) {
		return (size_t) -1;
	}

	/*	we really only need to flush when throttling is enabled,
		because we push the data as fast as possible anyway if not */
	if (UNEXPECTED(r->throttle.delay >= PHP_HTTP_DIFFSEC)) {
		r->ops->flush(r);
		php_http_sleep(r->throttle.delay);
	}
	return len;
}

static ZEND_RESULT_CODE php_http_env_response_send_data(php_http_env_response_t *r, const char *buf, size_t len)
{
	size_t chunks_sent, chunk = r->throttle.chunk ? r->throttle.chunk : PHP_HTTP_SENDBUF_SIZE;

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

		if (!enc_str) {
			return SUCCESS;
		}
		chunks_sent = php_http_buffer_chunked_output(&r->buffer, enc_str, enc_len, buf ? chunk : 0, output, r);
		PTR_FREE(enc_str);
	} else {
		chunks_sent = php_http_buffer_chunked_output(&r->buffer, buf, len, buf ? chunk : 0, output, r);
	}

	return chunks_sent != (size_t) -1 ? SUCCESS : FAILURE;
}

static inline ZEND_RESULT_CODE php_http_env_response_send_done(php_http_env_response_t *r)
{
	return php_http_env_response_send_data(r, NULL, 0);
}

php_http_env_response_t *php_http_env_response_init(php_http_env_response_t *r, zval *options, php_http_env_response_ops_t *ops, void *init_arg)
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

	ZVAL_COPY(&r->options, options);

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

void php_http_env_response_dtor(php_http_env_response_t *r)
{
	if (r->ops->dtor) {
		r->ops->dtor(r);
	}
	php_http_buffer_free(&r->buffer);
	zval_ptr_dtor(&r->options);
	PTR_FREE(r->content.type);
	PTR_FREE(r->content.encoding);
	if (r->content.encoder) {
		php_http_encoding_stream_free(&r->content.encoder);
	}
}

void php_http_env_response_free(php_http_env_response_t **r)
{
	if (*r) {
		php_http_env_response_dtor(*r);
		efree(*r);
		*r = NULL;
	}
}

static ZEND_RESULT_CODE php_http_env_response_send_head(php_http_env_response_t *r, php_http_message_t *request)
{
	ZEND_RESULT_CODE ret = SUCCESS;
	zval zoption_tmp, *zoption, *options = &r->options;

	if (r->done) {
		return ret;
	}

	if (EXPECTED(zoption = get_option(options, ZEND_STRL("headers"), &zoption_tmp))) {
		if (EXPECTED(Z_TYPE_P(zoption) == IS_ARRAY)) {
			php_http_header_to_callback(Z_ARRVAL_P(zoption), 0, (php_http_pass_format_callback_t) r->ops->set_header, r);
		}
		zval_ptr_dtor(zoption);
	}

	if (EXPECTED(zoption = get_option(options, ZEND_STRL("responseCode"), &zoption_tmp))) {
		zend_long rc = zval_get_long(zoption);

		zval_ptr_dtor(zoption);
		if (rc > 0) {
			ret = r->ops->set_status(r, rc);
		}
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if (EXPECTED(zoption = get_option(options, ZEND_STRL("httpVersion"), &zoption_tmp))) {
		php_http_version_t v;
		zend_string *zs = zval_get_string(zoption);

		zval_ptr_dtor(zoption);
		if (EXPECTED(zs->len && php_http_version_parse(&v, zs->val))) {
			ret = r->ops->set_protocol_version(r, &v);
			php_http_version_dtor(&v);
		}
		zend_string_release(zs);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if (EXPECTED(zoption = get_option(options, ZEND_STRL("cookies"), &zoption_tmp))) {
		if (Z_TYPE_P(zoption) == IS_ARRAY) {
			zval *zcookie;

			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(zoption), zcookie)
			{
				if (Z_TYPE_P(zcookie) == IS_OBJECT && instanceof_function(Z_OBJCE_P(zcookie), php_http_cookie_get_class_entry())) {
					php_http_cookie_object_t *obj = PHP_HTTP_OBJ(NULL, zcookie);
					char *str;
					size_t len;

					php_http_cookie_list_to_string(obj->list, &str, &len);
					if (SUCCESS != (ret = r->ops->add_header(r, "Set-Cookie: %s", str))) {
						efree(str);
						break;
					}
					efree(str);
				}
			}
			ZEND_HASH_FOREACH_END();
		}
		zval_ptr_dtor(zoption);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if (EXPECTED(zoption = get_option(options, ZEND_STRL("contentType"), &zoption_tmp))) {
		zend_string *zs = zval_get_string(zoption);

		zval_ptr_dtor(zoption);
		if (zs->len && strchr(zs->val, '/')) {
			if (SUCCESS == (ret = r->ops->set_header(r, "Content-Type: %.*s", zs->len, zs->val))) {
				r->content.type = estrndup(zs->val, zs->len);
			}
		}
		zend_string_release(zs);
	}

	if (ret != SUCCESS) {
		return ret;
	}

	if (UNEXPECTED(r->range.status == PHP_HTTP_RANGE_OK)) {
		if (zend_hash_num_elements(&r->range.values) == 1) {
			zval *range, *begin, *end;

			if (EXPECTED(	1 == php_http_array_list(&r->range.values, 1, &range)
				&&	2 == php_http_array_list(Z_ARRVAL_P(range), 2, &begin, &end))
			) {
				if (SUCCESS == (ret = r->ops->set_status(r, 206))) {
					ret = r->ops->set_header(r, "Content-Range: bytes %ld-%ld/%zu", Z_LVAL_P(begin), Z_LVAL_P(end), r->content.length);
				}
			} else {
				/* this should never happen */
				zend_hash_destroy(&r->range.values);
				ret = FAILURE;
			}
		} else {
			php_http_boundary(r->range.boundary, sizeof(r->range.boundary));
			if (SUCCESS == (ret = r->ops->set_status(r, 206))) {
				ret = r->ops->set_header(r, "Content-Type: multipart/byteranges; boundary=%s", r->range.boundary);
			}
		}
	} else {
		if (EXPECTED(zoption = get_option(options, ZEND_STRL("cacheControl"), &zoption_tmp))) {
			zend_string *zs = zval_get_string(zoption);

			zval_ptr_dtor(zoption);
			if (zs->len) {
				ret = r->ops->set_header(r, "Cache-Control: %.*s", zs->len, zs->val);
			}
			zend_string_release(zs);
		}

		if (ret != SUCCESS) {
			return ret;
		}

		if (EXPECTED(zoption = get_option(options, ZEND_STRL("contentDisposition"), &zoption_tmp))) {

			if (Z_TYPE_P(zoption) == IS_ARRAY) {
				php_http_buffer_t buf;

				php_http_buffer_init(&buf);
				if (php_http_params_to_string(&buf, Z_ARRVAL_P(zoption), ZEND_STRL(","), ZEND_STRL(";"), ZEND_STRL("="), PHP_HTTP_PARAMS_DEFAULT)) {
					if (buf.used) {
						ret = r->ops->set_header(r, "Content-Disposition: %.*s", buf.used, buf.data);
					}
				}

				php_http_buffer_dtor(&buf);
			}
			zval_ptr_dtor(zoption);
		}

		if (ret != SUCCESS) {
			return ret;
		}

		if (EXPECTED(zoption = get_option(options, ZEND_STRL("contentEncoding"), &zoption_tmp))) {
			zend_long ce = zval_get_long(zoption);
			zval zsupported;
			HashTable *result = NULL;

			zval_ptr_dtor(zoption);
			switch (ce) {
				case PHP_HTTP_CONTENT_ENCODING_GZIP:
					array_init(&zsupported);
					add_next_index_stringl(&zsupported, ZEND_STRL("none"));
					add_next_index_stringl(&zsupported, ZEND_STRL("gzip"));
					add_next_index_stringl(&zsupported, ZEND_STRL("deflate"));

					if ((result = php_http_negotiate_encoding(Z_ARRVAL(zsupported), request))) {
						zend_string *key_str = NULL;
						zend_ulong index = 0;

						zend_hash_internal_pointer_reset(result);
						if (HASH_KEY_IS_STRING == zend_hash_get_current_key(result, &key_str, &index)) {
							if (zend_string_equals_literal(key_str, "gzip")) {
								if (!(r->content.encoder = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_deflate_ops(), PHP_HTTP_DEFLATE_TYPE_GZIP))) {
									ret = FAILURE;
								} else if (SUCCESS == (ret = r->ops->set_header(r, "Content-Encoding: gzip"))) {
									r->content.encoding = estrndup(key_str->val, key_str->len);
								}
							} else if (zend_string_equals_literal(key_str, "deflate")) {
								if (!(r->content.encoder = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_deflate_ops(), PHP_HTTP_DEFLATE_TYPE_ZLIB))) {
									ret = FAILURE;
								} else if (SUCCESS == (ret = r->ops->set_header(r, "Content-Encoding: deflate"))) {
									r->content.encoding = estrndup(key_str->val, key_str->len);
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
		}

		if (SUCCESS != ret) {
			return ret;
		}

		if (php_http_env_response_is_cacheable(r, request)) {
			switch (php_http_env_is_response_cached_by_etag(options, ZEND_STRL("If-None-Match"), request)) {
				case PHP_HTTP_CACHE_MISS:
					break;

				case PHP_HTTP_CACHE_NO:
					if (PHP_HTTP_CACHE_HIT != php_http_env_is_response_cached_by_last_modified(options, ZEND_STRL("If-Modified-Since"), request)) {
						break;
					}
					/*  no break */

				case PHP_HTTP_CACHE_HIT:
					ret = r->ops->set_status(r, 304);
					r->done = 1;
					break;
			}

			if (EXPECTED(zoption = get_option(options, ZEND_STRL("etag"), &zoption_tmp))) {
				zend_string *zs = zval_get_string(zoption);

				zval_ptr_dtor(zoption);
				if (EXPECTED(*zs->val != '"' && strncmp(zs->val, "W/\"", 3))) {
					ret = r->ops->set_header(r, "ETag: \"%s\"", zs->val);
				} else {
					ret = r->ops->set_header(r, "ETag: %s", zs->val);
				}
				zend_string_release(zs);
			}
			if (EXPECTED(zoption = get_option(options, ZEND_STRL("lastModified"), &zoption_tmp))) {
				zend_long lm = zval_get_long(zoption);

				zval_ptr_dtor(zoption);
				if (EXPECTED(lm)) {
					zend_string *date = php_format_date(ZEND_STRL(PHP_HTTP_DATE_FORMAT), lm, 0);
					if (date) {
						ret = r->ops->set_header(r, "Last-Modified: %s", date->val);
						zend_string_release(date);
					}
				}
			}
		}
	}

	return ret;
}

static ZEND_RESULT_CODE php_http_env_response_send_body(php_http_env_response_t *r)
{
	ZEND_RESULT_CODE ret = SUCCESS;
	zval zoption_tmp, *zoption;
	php_http_message_body_t *body;

	if (r->done) {
		return ret;
	}

	if (EXPECTED(body = get_body(&r->options))) {
		if (EXPECTED(zoption = get_option(&r->options, ZEND_STRL("throttleDelay"), &zoption_tmp))) {
			r->throttle.delay = zval_get_double(zoption);
			zval_ptr_dtor(zoption);
		}
		if (EXPECTED(zoption = get_option(&r->options, ZEND_STRL("throttleChunk"), &zoption_tmp))) {
			r->throttle.chunk = zval_get_long(zoption);
			zval_ptr_dtor(zoption);
		}

		if (UNEXPECTED(r->range.status == PHP_HTTP_RANGE_OK)) {
			if (zend_hash_num_elements(&r->range.values) == 1) {
				/* single range */
				zval *range, *begin, *end;

				if (	1 == php_http_array_list(&r->range.values, 1, &range)
					&&	2 == php_http_array_list(Z_ARRVAL_P(range), 2, &begin, &end)
				) {
					/* send chunk */
					ret = php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_env_response_send_data, r, Z_LVAL_P(begin), Z_LVAL_P(end) - Z_LVAL_P(begin) + 1);
					if (ret == SUCCESS) {
						ret = php_http_env_response_send_done(r);
					}
					zend_hash_destroy(&r->range.values);
				} else {
					/* this should never happen */
					zend_hash_destroy(&r->range.values);
					r->ops->set_status(r, 500);
					ret = FAILURE;
				}

			} else {
				/* send multipart/byte-ranges message */
				zval *chunk;

				ZEND_HASH_FOREACH_VAL(&r->range.values, chunk)
				{
					zval *begin, *end;

					if (2 == php_http_array_list(Z_ARRVAL_P(chunk), 2, &begin, &end)) {
						php_http_buffer_appendf(r->buffer,
								PHP_HTTP_CRLF
								"--%s" PHP_HTTP_CRLF
								"Content-Type: %s" PHP_HTTP_CRLF
								"Content-Range: bytes %ld-%ld/%zu" PHP_HTTP_CRLF PHP_HTTP_CRLF,
								/* - */
								r->range.boundary,
								r->content.type ? r->content.type : "application/octet-stream",
								Z_LVAL_P(begin),
								Z_LVAL_P(end),
								r->content.length
						);
						ret = php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_env_response_send_data, r, Z_LVAL_P(begin), Z_LVAL_P(end) - Z_LVAL_P(begin) + 1);
					}
				}
				ZEND_HASH_FOREACH_END();

				if (ret == SUCCESS) {
					php_http_buffer_appendf(r->buffer, PHP_HTTP_CRLF "--%s--", r->range.boundary);
					ret = php_http_env_response_send_done(r);
				}
				zend_hash_destroy(&r->range.values);
			}

		} else {
			ret = php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_env_response_send_data, r, 0, 0);
			if (ret == SUCCESS) {
				ret = php_http_env_response_send_done(r);
			}
		}
	}
	return ret;
}

ZEND_RESULT_CODE php_http_env_response_send(php_http_env_response_t *r)
{
	php_http_message_t *request;
	php_http_message_body_t *body;

	request = get_request(&r->options);

	/* check for ranges */
	if (EXPECTED(body = get_body(&r->options))) {
		r->content.length = php_http_message_body_size(body);

		if (UNEXPECTED(SUCCESS != r->ops->set_header(r, "Accept-Ranges: bytes"))) {
			return FAILURE;
		} else {
			ZEND_INIT_SYMTABLE_EX(&r->range.values, 0, 0);
			r->range.status = php_http_env_get_request_ranges(&r->range.values, r->content.length, request);

			switch (r->range.status) {
				case PHP_HTTP_RANGE_NO:
					zend_hash_destroy(&r->range.values);
					break;

				case PHP_HTTP_RANGE_ERR:
					if (php_http_env_got_request_header(ZEND_STRL("If-Range"), request)) {
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
					if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(&r->options, ZEND_STRL("If-Range"), request)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(&r->options, ZEND_STRL("If-Range"), request)
					) {
						r->range.status = PHP_HTTP_RANGE_NO;
						zend_hash_destroy(&r->range.values);
						break;
					}
					if (PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_etag(&r->options, ZEND_STRL("If-Match"), request)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(&r->options, ZEND_STRL("If-Unmodified-Since"), request)
					||	PHP_HTTP_CACHE_MISS == php_http_env_is_response_cached_by_last_modified(&r->options, ZEND_STRL("Unless-Modified-Since"), request)
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

	if (UNEXPECTED(SUCCESS != php_http_env_response_send_head(r, request))) {
		php_error_docref(NULL, E_WARNING, "Failed to send response headers");
		return FAILURE;
	}

	if (UNEXPECTED(SUCCESS != php_http_env_response_send_body(r))) {
		php_error_docref(NULL, E_WARNING, "Failed to send response body");
		return FAILURE;
	}

	if (UNEXPECTED(SUCCESS != r->ops->finish(r))) {
		php_error_docref(NULL, E_WARNING, "Failed to finish response");
		return FAILURE;
	}

	return SUCCESS;
}

static long php_http_env_response_sapi_get_status(php_http_env_response_t *r)
{
	return php_http_env_get_response_code();
}
static ZEND_RESULT_CODE php_http_env_response_sapi_set_status(php_http_env_response_t *r, long http_code)
{
	return php_http_env_set_response_code(http_code);
}
static ZEND_RESULT_CODE php_http_env_response_sapi_set_protocol_version(php_http_env_response_t *r, php_http_version_t *v)
{

	return php_http_env_set_response_protocol_version(v);
}
static ZEND_RESULT_CODE php_http_env_response_sapi_set_header(php_http_env_response_t *r, const char *fmt, ...)
{
	ZEND_RESULT_CODE ret;
	va_list args;

	va_start(args, fmt);
	ret = php_http_env_set_response_header_va(0, 1, fmt, args);
	va_end(args);

	return ret;
}
static ZEND_RESULT_CODE php_http_env_response_sapi_add_header(php_http_env_response_t *r, const char *fmt, ...)
{
	ZEND_RESULT_CODE ret;
	va_list args;

	va_start(args, fmt);
	ret = php_http_env_set_response_header_va(0, 0, fmt, args);
	va_end(args);

	return ret;
}
static ZEND_RESULT_CODE php_http_env_response_sapi_del_header(php_http_env_response_t *r, const char *header_str, size_t header_len)
{
	return php_http_env_set_response_header_value(0, header_str, header_len, NULL, 1);
}
static ZEND_RESULT_CODE php_http_env_response_sapi_write(php_http_env_response_t *r, const char *data_str, size_t data_len)
{
	if (0 < PHPWRITE(data_str, data_len)) {
		return SUCCESS;
	}
	return FAILURE;
}
static ZEND_RESULT_CODE php_http_env_response_sapi_flush(php_http_env_response_t *r)
{
	if (php_output_get_level()) {
		php_output_flush_all();
	}
	if (!(php_output_get_status() & PHP_OUTPUT_IMPLICITFLUSH)) {
		sapi_flush();
	}

	return SUCCESS;
}
static ZEND_RESULT_CODE php_http_env_response_sapi_finish(php_http_env_response_t *r)
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

php_http_env_response_ops_t *php_http_env_response_get_sapi_ops(void)
{
	return &php_http_env_response_sapi_ops;
}

typedef struct php_http_env_response_stream_ctx {
	HashTable header;
	php_http_version_t version;
	long status_code;

	php_stream *stream;
	php_stream_filter *chunked_filter;
	php_http_message_t *request;

	unsigned started:1;
	unsigned finished:1;
	unsigned chunked:1;
} php_http_env_response_stream_ctx_t;

static ZEND_RESULT_CODE php_http_env_response_stream_init(php_http_env_response_t *r, void *init_arg)
{
	php_http_env_response_stream_ctx_t *ctx;
	size_t buffer_size = 0x1000;

	ctx = ecalloc(1, sizeof(*ctx));

	ctx->stream = init_arg;
	GC_ADDREF(ctx->stream->res);
	ZEND_INIT_SYMTABLE(&ctx->header);
	php_http_version_init(&ctx->version, 1, 1);
	php_stream_set_option(ctx->stream, PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_FULL, &buffer_size);
	ctx->status_code = 200;
	ctx->chunked = 1;
	ctx->request = get_request(&r->options);

	/* there are some limitations regarding TE:chunked, see https://tools.ietf.org/html/rfc7230#section-3.3.1 */
	if (UNEXPECTED(ctx->request && ctx->request->http.version.major == 1 && ctx->request->http.version.minor == 0)) {
		ctx->version.minor = 0;
	}

	r->ctx = ctx;

	return SUCCESS;
}
static void php_http_env_response_stream_dtor(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *ctx = r->ctx;

	if (UNEXPECTED(ctx->chunked_filter)) {
		ctx->chunked_filter = php_stream_filter_remove(ctx->chunked_filter, 1);
	}
	zend_hash_destroy(&ctx->header);
	zend_list_delete(ctx->stream->res);
	efree(ctx);
	r->ctx = NULL;
}
static void php_http_env_response_stream_header(php_http_env_response_stream_ctx_t *ctx, HashTable *header, php_http_buffer_t *buf)
{
	zval *val;

	ZEND_HASH_FOREACH_VAL(header, val)
	{
		if (UNEXPECTED(Z_TYPE_P(val) == IS_ARRAY)) {
			php_http_env_response_stream_header(ctx, Z_ARRVAL_P(val), buf);
		} else {
			zend_string *zs = zval_get_string(val);

			if (ctx->chunked) {
				/* disable chunked transfer encoding if we've got an explicit content-length */
				if (!strncasecmp(zs->val, "Content-Length:", lenof("Content-Length:"))) {
					ctx->chunked = 0;
				}
			}
			php_http_buffer_append(buf, zs->val, zs->len);
			php_http_buffer_appends(buf, PHP_HTTP_CRLF);
			zend_string_release(zs);
		}
	}
	ZEND_HASH_FOREACH_END();
}
static ZEND_RESULT_CODE php_http_env_response_stream_start(php_http_env_response_stream_ctx_t *ctx)
{
	php_http_buffer_t header_buf;

	if (ctx->started || ctx->finished) {
		return FAILURE;
	}

	php_http_buffer_init(&header_buf);
	php_http_buffer_appendf(&header_buf, "HTTP/%u.%u %ld %s" PHP_HTTP_CRLF, ctx->version.major, ctx->version.minor, ctx->status_code, php_http_env_get_response_status_for_code(ctx->status_code));

	/* there are some limitations regarding TE:chunked, see https://tools.ietf.org/html/rfc7230#section-3.3.1 */
	if (UNEXPECTED(ctx->version.major == 1 && ctx->version.minor == 0)) {
		ctx->chunked = 0;
	} else if (UNEXPECTED(ctx->status_code == 204 || ctx->status_code/100 == 1)) {
		ctx->chunked = 0;
	} else if (UNEXPECTED(ctx->request && ctx->status_code/100 == 2 && !strcasecmp(ctx->request->http.info.request.method, "CONNECT"))) {
		ctx->chunked = 0;
	}

	php_http_env_response_stream_header(ctx, &ctx->header, &header_buf);

	/* enable chunked transfer encoding */
	if (ctx->chunked) {
		php_http_buffer_appends(&header_buf, "Transfer-Encoding: chunked" PHP_HTTP_CRLF);
	}

	php_http_buffer_appends(&header_buf, PHP_HTTP_CRLF);

	if (header_buf.used == php_stream_write(ctx->stream, header_buf.data, header_buf.used)) {
		ctx->started = 1;
	}
	php_http_buffer_dtor(&header_buf);
	php_stream_flush(ctx->stream);

	if (ctx->chunked) {
		ctx->chunked_filter = php_stream_filter_create("http.chunked_encode", NULL, 0);
		if (ctx->chunked_filter) {
			php_stream_filter_append(&ctx->stream->writefilters, ctx->chunked_filter);
		}
	}

	return ctx->started ? SUCCESS : FAILURE;
}
static long php_http_env_response_stream_get_status(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *ctx = r->ctx;

	return ctx->status_code;
}
static ZEND_RESULT_CODE php_http_env_response_stream_set_status(php_http_env_response_t *r, long http_code)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->started || stream_ctx->finished) {
		return FAILURE;
	}

	stream_ctx->status_code = http_code;

	return SUCCESS;
}
static ZEND_RESULT_CODE php_http_env_response_stream_set_protocol_version(php_http_env_response_t *r, php_http_version_t *v)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->started || stream_ctx->finished) {
		return FAILURE;
	}

	memcpy(&stream_ctx->version, v, sizeof(stream_ctx->version));

	return SUCCESS;
}
static ZEND_RESULT_CODE php_http_env_response_stream_set_header_ex(php_http_env_response_t *r, zend_bool replace, const char *fmt, va_list argv)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;
	char *header_end, *header_str = NULL;
	size_t header_len = 0;
	zval zheader, *zheader_ptr;
	zend_string *header_key;
	ZEND_RESULT_CODE rv;

	if (UNEXPECTED(stream_ctx->started || stream_ctx->finished)) {
		return FAILURE;
	}

	header_len = vspprintf(&header_str, 0, fmt, argv);

	if (UNEXPECTED(!(header_end = strchr(header_str, ':')))) {
		efree(header_str);
		return FAILURE;
	}

	header_key = zend_string_init(header_str, header_end - header_str, 0);

	if (!replace && (zheader_ptr = zend_hash_find(&stream_ctx->header, header_key))) {
		convert_to_array(zheader_ptr);
		rv = add_next_index_str(zheader_ptr, php_http_cs2zs(header_str, header_len));
	} else {
		ZVAL_STR(&zheader, php_http_cs2zs(header_str, header_len));

		rv = zend_hash_update(&stream_ctx->header, header_key, &zheader)
			? SUCCESS : FAILURE;
	}

	zend_string_release(header_key);

	return rv;
}
static ZEND_RESULT_CODE php_http_env_response_stream_set_header(php_http_env_response_t *r, const char *fmt, ...)
{
	ZEND_RESULT_CODE ret;
	va_list argv;

	va_start(argv, fmt);
	ret = php_http_env_response_stream_set_header_ex(r, 1, fmt, argv);
	va_end(argv);

	return ret;
}
static ZEND_RESULT_CODE php_http_env_response_stream_add_header(php_http_env_response_t *r, const char *fmt, ...)
{
	ZEND_RESULT_CODE ret;
	va_list argv;

	va_start(argv, fmt);
	ret = php_http_env_response_stream_set_header_ex(r, 0, fmt, argv);
	va_end(argv);

	return ret;
}
static ZEND_RESULT_CODE php_http_env_response_stream_del_header(php_http_env_response_t *r, const char *header_str, size_t header_len)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->started || stream_ctx->finished) {
		return FAILURE;
	}

	zend_hash_str_del(&stream_ctx->header, header_str, header_len);
	return SUCCESS;
}
static ZEND_RESULT_CODE php_http_env_response_stream_write(php_http_env_response_t *r, const char *data_str, size_t data_len)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->finished) {
		return FAILURE;
	}
	if (!stream_ctx->started) {
		if (SUCCESS != php_http_env_response_stream_start(stream_ctx)) {
			return FAILURE;
		}
	}

	if (data_len != php_stream_write(stream_ctx->stream, data_str, data_len)) {
		return FAILURE;
	}

	return SUCCESS;
}
static ZEND_RESULT_CODE php_http_env_response_stream_flush(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *stream_ctx = r->ctx;

	if (stream_ctx->finished) {
		return FAILURE;
	}
	if (!stream_ctx->started) {
		if (SUCCESS != php_http_env_response_stream_start(stream_ctx)) {
			return FAILURE;
		}
	}

	return php_stream_flush(stream_ctx->stream);
}
static ZEND_RESULT_CODE php_http_env_response_stream_finish(php_http_env_response_t *r)
{
	php_http_env_response_stream_ctx_t *ctx = r->ctx;

	if (UNEXPECTED(ctx->finished)) {
		return FAILURE;
	}
	if (UNEXPECTED(!ctx->started)) {
		if (SUCCESS != php_http_env_response_stream_start(ctx)) {
			return FAILURE;
		}
	}

	php_stream_flush(ctx->stream);
	if (ctx->chunked && ctx->chunked_filter) {
		php_stream_filter_flush(ctx->chunked_filter, 1);
		ctx->chunked_filter = php_stream_filter_remove(ctx->chunked_filter, 1);
	}

	ctx->finished = 1;

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

php_http_env_response_ops_t *php_http_env_response_get_stream_ops(void)
{
	return &php_http_env_response_stream_ops;
}

#define PHP_HTTP_ENV_RESPONSE_OBJECT_INIT(obj) \
	do { \
		if (!obj->message) { \
			obj->message = php_http_message_init_env(NULL, PHP_HTTP_RESPONSE); \
		} \
	} while (0)

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse___construct, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, __construct)
{
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	php_http_expect(obj->message = php_http_message_init_env(obj->message, PHP_HTTP_RESPONSE), unexpected_val, return);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse___invoke, 0, 0, 1)
	ZEND_ARG_INFO(0, ob_string)
	ZEND_ARG_INFO(0, ob_flags)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, __invoke)
{
	char *ob_str;
	size_t ob_len;
	zend_long ob_flags = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &ob_str, &ob_len, &ob_flags)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_ENV_RESPONSE_OBJECT_INIT(obj);

		if (!obj->body) {
			php_http_message_object_init_body_object(obj);
		}

		if (ob_flags & PHP_OUTPUT_HANDLER_CLEAN) {
			php_stream_truncate_set_size(php_http_message_body_stream(obj->message->body), 0);
		} else {
			php_http_message_body_append(obj->message->body, ob_str, ob_len);
		}
		RETURN_TRUE;
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setEnvRequest, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, env_request, http\\Message, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setEnvRequest)
{
	zval *env_req = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|O", &env_req, php_http_message_get_class_entry()), invalid_arg, return);

	set_option(getThis(), ZEND_STRL("request"), IS_OBJECT, env_req, 0);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setContentType, 0, 0, 1)
	ZEND_ARG_INFO(0, content_type)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setContentType)
{
	char *ct_str = NULL;
	size_t ct_len = 0;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s!", &ct_str, &ct_len), invalid_arg, return);

	set_option(getThis(), ZEND_STRL("contentType"), IS_STRING, ct_str, ct_len);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setContentDisposition, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, disposition_params, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setContentDisposition)
{
	zval *zdisposition;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "a", &zdisposition), invalid_arg, return);

	zend_update_property(Z_OBJCE_P(getThis()), getThis(), ZEND_STRL("contentDisposition"), zdisposition);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setContentEncoding, 0, 0, 1)
	ZEND_ARG_INFO(0, content_encoding)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setContentEncoding)
{
	zend_long ce;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "l", &ce), invalid_arg, return);

	set_option(getThis(), ZEND_STRL("contentEncoding"), IS_LONG, &ce, 0);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setCacheControl, 0, 0, 1)
	ZEND_ARG_INFO(0, cache_control)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setCacheControl)
{
	char *cc_str = NULL;
	size_t cc_len = 0;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s!", &cc_str, &cc_len), invalid_arg, return);

	set_option(getThis(), ZEND_STRL("cacheControl"), IS_STRING, cc_str, cc_len);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setLastModified, 0, 0, 1)
	ZEND_ARG_INFO(0, last_modified)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setLastModified)
{
	zend_long last_modified;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "l", &last_modified), invalid_arg, return);

	set_option(getThis(), ZEND_STRL("lastModified"), IS_LONG, &last_modified, 0);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_isCachedByLastModified, 0, 0, 0)
	ZEND_ARG_INFO(0, header_name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, isCachedByLastModified)
{
	char *header_name_str = NULL;
	size_t header_name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s!", &header_name_str, &header_name_len)) {
		if (!header_name_str || !header_name_len) {
			header_name_str = "If-Modified-Since";
			header_name_len = lenof("If-Modified-Since");
		}

		RETURN_LONG(php_http_env_is_response_cached_by_last_modified(getThis(), header_name_str, header_name_len, get_request(getThis())));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setEtag, 0, 0, 1)
	ZEND_ARG_INFO(0, etag)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setEtag)
{
	char *etag_str = NULL;
	size_t etag_len = 0;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s!", &etag_str, &etag_len), invalid_arg, return);

	set_option(getThis(), ZEND_STRL("etag"), IS_STRING, etag_str, etag_len);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_isCachedByEtag, 0, 0, 0)
	ZEND_ARG_INFO(0, header_name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, isCachedByEtag)
{
	char *header_name_str = NULL;
	size_t header_name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s!", &header_name_str, &header_name_len)) {
		if (!header_name_str || !header_name_len) {
			header_name_str = "If-None-Match";
			header_name_len = lenof("If-None-Match");
		}
		RETURN_LONG(php_http_env_is_response_cached_by_etag(getThis(), header_name_str, header_name_len, get_request(getThis())));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setThrottleRate, 0, 0, 1)
	ZEND_ARG_INFO(0, chunk_size)
	ZEND_ARG_INFO(0, delay)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setThrottleRate)
{
	zend_long chunk_size;
	double delay = 1;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "l|d", &chunk_size, &delay), invalid_arg, return);

	set_option(getThis(), ZEND_STRL("throttleDelay"), IS_DOUBLE, &delay, 0);
	set_option(getThis(), ZEND_STRL("throttleChunk"), IS_LONG, &chunk_size, 0);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_setCookie, 0, 0, 1)
	ZEND_ARG_INFO(0, cookie)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, setCookie)
{
	zval *zcookie_new, tmp;
	zend_string *zs;
	zend_error_handling zeh;
	php_http_cookie_list_t *list = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zcookie_new), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_get_exception_unexpected_val_class_entry(), &zeh);
	switch (Z_TYPE_P(zcookie_new)) {
	case IS_OBJECT:
		if (instanceof_function(Z_OBJCE_P(zcookie_new), php_http_cookie_get_class_entry())) {
			Z_ADDREF_P(zcookie_new);
			break;
		}
		/* no break */
	case IS_ARRAY:
		list = php_http_cookie_list_from_struct(NULL, zcookie_new);
		zcookie_new = &tmp;
		ZVAL_OBJECT(zcookie_new, &php_http_cookie_object_new_ex(php_http_cookie_get_class_entry(), list)->zo, 0);
		break;

	default:
		zs = zval_get_string(zcookie_new);
		list = php_http_cookie_list_parse(NULL, zs->val, zs->len, 0, NULL);
		zend_string_release(zs);
		zcookie_new = &tmp;
		ZVAL_OBJECT(zcookie_new, &php_http_cookie_object_new_ex(php_http_cookie_get_class_entry(), list)->zo, 0);
	}
	zend_restore_error_handling(&zeh);

	set_cookie(getThis(), zcookie_new);
	zval_ptr_dtor(zcookie_new);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvResponse_send, 0, 0, 0)
	ZEND_ARG_INFO(0, stream)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvResponse, send)
{
	zval *zstream = NULL;
	php_stream *s = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &zstream)) {
		/* first flush the output layer to avoid conflicting headers and output;
		 * also, ob_start($thisEnvResponse) might have been called */
		php_output_end_all();

		if (zstream) {
			php_http_env_response_t *r;

			php_stream_from_zval(s, zstream);
			r = php_http_env_response_init(NULL, getThis(), php_http_env_response_get_stream_ops(), s);
			if (!r) {
				RETURN_FALSE;
			}

			RETVAL_BOOL(SUCCESS == php_http_env_response_send(r));
			php_http_env_response_free(&r);
		} else {
			php_http_env_response_t r;

			if (!php_http_env_response_init(&r, getThis(), NULL, NULL)) {
				RETURN_FALSE;
			}

			RETVAL_BOOL(SUCCESS == php_http_env_response_send(&r));
			php_http_env_response_dtor(&r);
		}
	}
}

static zend_function_entry php_http_env_response_methods[] = {
	PHP_ME(HttpEnvResponse, __construct,             ai_HttpEnvResponse___construct,             ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpEnvResponse, __invoke,                ai_HttpEnvResponse___invoke,                ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setEnvRequest,           ai_HttpEnvResponse_setEnvRequest,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setCookie,               ai_HttpEnvResponse_setCookie,               ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setContentType,          ai_HttpEnvResponse_setContentType,          ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setContentDisposition,   ai_HttpEnvResponse_setContentDisposition,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setContentEncoding,      ai_HttpEnvResponse_setContentEncoding,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setCacheControl,         ai_HttpEnvResponse_setCacheControl,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setLastModified,         ai_HttpEnvResponse_setLastModified,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, isCachedByLastModified,  ai_HttpEnvResponse_isCachedByLastModified,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setEtag,                 ai_HttpEnvResponse_setEtag,                 ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, isCachedByEtag,          ai_HttpEnvResponse_isCachedByEtag,          ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, setThrottleRate,         ai_HttpEnvResponse_setThrottleRate,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvResponse, send,                    ai_HttpEnvResponse_send,                    ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

static zend_class_entry *php_http_env_response_class_entry;
zend_class_entry *php_http_get_env_response_class_entry(void)
{
	return php_http_env_response_class_entry;
}

PHP_MINIT_FUNCTION(http_env_response)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Env", "Response", php_http_env_response_methods);
	php_http_env_response_class_entry = zend_register_internal_class_ex(&ce, php_http_message_get_class_entry());

	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CONTENT_ENCODING_NONE"), PHP_HTTP_CONTENT_ENCODING_NONE);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CONTENT_ENCODING_GZIP"), PHP_HTTP_CONTENT_ENCODING_GZIP);

	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_NO"), PHP_HTTP_CACHE_NO);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_HIT"), PHP_HTTP_CACHE_HIT);
	zend_declare_class_constant_long(php_http_env_response_class_entry, ZEND_STRL("CACHE_MISS"), PHP_HTTP_CACHE_MISS);

	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("request"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("cookies"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("contentType"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("contentDisposition"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("contentEncoding"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("cacheControl"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("etag"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("lastModified"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("throttleDelay"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_response_class_entry, ZEND_STRL("throttleChunk"), ZEND_ACC_PROTECTED);

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
