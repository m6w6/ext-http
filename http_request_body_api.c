/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_CURL
#include "php_http.h"

#ifdef HTTP_HAVE_CURL

#include "php_http_api.h"
#include "php_http_url_api.h"
#include "php_http_request_body_api.h"

/* {{{ http_request_body *http_request_body_new() */
PHP_HTTP_API http_request_body *_http_request_body_init_ex(http_request_body *body, int type, void *data, size_t size, zend_bool free ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	if (!body) {
		body = emalloc_rel(sizeof(http_request_body));
	}
	
	body->type = type;
	body->free = free;
	body->priv = 0;
	body->data = data;
	body->size = size;
	
	return body;
}
/* }}} */

/* {{{ http_request_body *http_request_body_fill(http_request_body *body, HashTable *, HashTable *) */
PHP_HTTP_API http_request_body *_http_request_body_fill(http_request_body *body, HashTable *fields, HashTable *files ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	if (files && (zend_hash_num_elements(files) > 0)) {
		HashKey key = initHashKey(0);
		zval **data_ptr;
		HashPosition pos;
		struct curl_httppost *http_post_data[2] = {NULL, NULL};

		/* normal data */
		if (fields) {
			FOREACH_HASH_KEYVAL(pos, fields, key, data_ptr) {
				if (key.type == HASH_KEY_IS_STRING) {
					CURLcode err;
					zval *data = http_zsep(IS_STRING, *data_ptr);
					
					err = curl_formadd(&http_post_data[0], &http_post_data[1],
						CURLFORM_COPYNAME,			key.str,
						CURLFORM_COPYCONTENTS,		Z_STRVAL_P(data),
						CURLFORM_CONTENTSLENGTH,	(long) Z_STRLEN_P(data),
						CURLFORM_END
					);
					
					zval_ptr_dtor(&data);
					
					if (CURLE_OK != err) {
						http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not encode post fields: %s", curl_easy_strerror(err));
						curl_formfree(http_post_data[0]);
						return NULL;
					}
				}
			}
		}

		/* file data */
		FOREACH_HASH_VAL(pos, files, data_ptr) {
			zval **file_ptr, **type_ptr, **name_ptr;
			
			if (Z_TYPE_PP(data_ptr) != IS_ARRAY) {
				http_error(HE_NOTICE, HTTP_E_INVALID_PARAM, "Unrecognized type of post file array entry");
			} else if (	SUCCESS != zend_hash_find(Z_ARRVAL_PP(data_ptr), "name", sizeof("name"), (void *) &name_ptr) ||
						SUCCESS != zend_hash_find(Z_ARRVAL_PP(data_ptr), "type", sizeof("type"), (void *) &type_ptr) ||
						SUCCESS != zend_hash_find(Z_ARRVAL_PP(data_ptr), "file", sizeof("file"), (void *) &file_ptr)) {
				http_error(HE_NOTICE, HTTP_E_INVALID_PARAM, "Post file array entry misses either 'name', 'type' or 'file' entry");
			} else {
				CURLcode err;
				const char *path;
				zval *file = http_zsep(IS_STRING, *file_ptr);
				zval *type = http_zsep(IS_STRING, *type_ptr);
				zval *name = http_zsep(IS_STRING, *name_ptr);
				
				HTTP_CHECK_OPEN_BASEDIR(Z_STRVAL_P(file), curl_formfree(http_post_data[0]); return NULL);
				
				/* this is blatant but should be sufficient for most cases */
				if (strncasecmp(Z_STRVAL_P(file), "file://", lenof("file://"))) {
					path = Z_STRVAL_P(file);
				} else {
					path = Z_STRVAL_P(file) + lenof("file://");
				}
				
				err = curl_formadd(&http_post_data[0], &http_post_data[1],
					CURLFORM_COPYNAME,		Z_STRVAL_P(name),
					CURLFORM_FILE,			path,
					CURLFORM_CONTENTTYPE,	Z_STRVAL_P(type),
					CURLFORM_END
				);
				
				zval_ptr_dtor(&file);
				zval_ptr_dtor(&type);
				zval_ptr_dtor(&name);
				
				if (CURLE_OK != err) {
					http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not encode post files: %s", curl_easy_strerror(err));
					curl_formfree(http_post_data[0]);
					return NULL;
				}
			}
		}
		
		return http_request_body_init_rel(body, HTTP_REQUEST_BODY_CURLPOST, http_post_data[0], 0, 1);

	} else if (fields) {
		char *encoded;
		size_t encoded_len;

		if (SUCCESS != http_urlencode_hash_ex(fields, 1, NULL, 0, &encoded, &encoded_len)) {
			http_error(HE_WARNING, HTTP_E_ENCODING, "Could not encode post data");
			return NULL;
		}
		
		return http_request_body_init_rel(body, HTTP_REQUEST_BODY_CSTRING, encoded, encoded_len, 1);
	} else {
		return http_request_body_init_rel(body, HTTP_REQUEST_BODY_CSTRING, estrndup("", 0), 0, 1);
	}
}

/* STATUS http_request_body_encode(http_request_body *, char**, size_t *) */
PHP_HTTP_API STATUS _http_request_body_encode(http_request_body *body, char **buf, size_t *len TSRMLS_DC)
{
	switch (body->type) {
		case HTTP_REQUEST_BODY_CURLPOST:
		{
#ifdef HAVE_CURL_FORMGET
			phpstr str;
			
			phpstr_init_ex(&str, 0x8000, 0);
			if (curl_formget(body->data, &str, (curl_formget_callback) phpstr_append)) {
				phpstr_dtor(&str);
			} else {
				phpstr_fix(&str);
				*buf = PHPSTR_VAL(&str);
				*len = PHPSTR_LEN(&str);
				return SUCCESS;
			}
#endif
			break;
		}
		
		case HTTP_REQUEST_BODY_CSTRING:
			*buf = estrndup(body->data, *len = body->size);
			return SUCCESS;
		
		default:
			break;
	}
	return FAILURE;
}
/* }}} */

/* {{{ void http_request_body_dtor(http_request_body *) */
PHP_HTTP_API void _http_request_body_dtor(http_request_body *body TSRMLS_DC)
{
	if (body) {
		if (body->free) {
			switch (body->type) {
				case HTTP_REQUEST_BODY_CSTRING:
					if (body->data) {
						efree(body->data);
					}
					break;
	
				case HTTP_REQUEST_BODY_CURLPOST:
					curl_formfree(body->data);
					break;
	
				case HTTP_REQUEST_BODY_UPLOADFILE:
					php_stream_close(body->data);
					break;
			}
		}
		memset(body, 0, sizeof(http_request_body));
	}
}
/* }}} */

/* {{{ void http_request_body_free(http_request_body *) */
PHP_HTTP_API void _http_request_body_free(http_request_body **body TSRMLS_DC)
{
	if (*body) {
		http_request_body_dtor(*body);
		efree(*body);
		*body = NULL;
	}
}
/* }}} */

#endif /* HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
