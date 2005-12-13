/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#define HTTP_WANT_CURL
#include "php_http.h"

#ifdef HTTP_HAVE_CURL

#include "php_http_api.h"
#include "php_http_url_api.h"
#include "php_http_request_body_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

/* {{{ http_request_body *http_request_body_new() */
PHP_HTTP_API http_request_body *_http_request_body_init_ex(http_request_body *body, int type, void *data, size_t size, zend_bool free ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	if (!body) {
		body = emalloc_rel(sizeof(http_request_body));
	}
	
	body->type = type;
	body->free = free;
	body->data = data;
	body->size = size;
	
	return body;
}
/* }}} */

/* {{{ http_request_body *http_request_body_fill(http_request_body *body, HashTable *, HashTable *) */
PHP_HTTP_API http_request_body *_http_request_body_fill(http_request_body *body, HashTable *fields, HashTable *files ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	if (files && (zend_hash_num_elements(files) > 0)) {
		char *key = NULL;
		ulong idx;
		zval **data;
		HashPosition pos;
		struct curl_httppost *http_post_data[2] = {NULL, NULL};

		/* normal data */
		FOREACH_HASH_KEYVAL(pos, fields, key, idx, data) {
			CURLcode err;
			if (key) {
				zval *orig = *data;
				
				convert_to_string_ex(data);
				err = curl_formadd(&http_post_data[0], &http_post_data[1],
					CURLFORM_COPYNAME,			key,
					CURLFORM_COPYCONTENTS,		Z_STRVAL_PP(data),
					CURLFORM_CONTENTSLENGTH,	(long) Z_STRLEN_PP(data),
					CURLFORM_END
				);
				
				if (orig != *data) {
					zval_ptr_dtor(data);
				}
				
				if (CURLE_OK != err) {
					http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not encode post fields: %s", curl_easy_strerror(err));
					curl_formfree(http_post_data[0]);
					return NULL;
				}

				/* reset */
				key = NULL;
			}
		}

		/* file data */
		FOREACH_HASH_VAL(pos, files, data) {
			zval **file, **type, **name;
			
			if (Z_TYPE_PP(data) != IS_ARRAY) {
				http_error(HE_NOTICE, HTTP_E_INVALID_PARAM, "Unrecognized type of post file array entry");
			} else if (	SUCCESS != zend_hash_find(Z_ARRVAL_PP(data), "name", sizeof("name"), (void **) &name) ||
						SUCCESS != zend_hash_find(Z_ARRVAL_PP(data), "type", sizeof("type"), (void **) &type) ||
						SUCCESS != zend_hash_find(Z_ARRVAL_PP(data), "file", sizeof("file"), (void **) &file)) {
				http_error(HE_NOTICE, HTTP_E_INVALID_PARAM, "Post file array entry misses either 'name', 'type' or 'file' entry");
			} else {
				CURLcode err = curl_formadd(&http_post_data[0], &http_post_data[1],
					CURLFORM_COPYNAME,		Z_STRVAL_PP(name),
					CURLFORM_FILE,			Z_STRVAL_PP(file),
					CURLFORM_CONTENTTYPE,	Z_STRVAL_PP(type),
					CURLFORM_END
				);
				if (CURLE_OK != err) {
					http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not encode post files: %s", curl_easy_strerror(err));
					curl_formfree(http_post_data[0]);
					return NULL;
				}
			}
		}
		
		return http_request_body_init_rel(body, HTTP_REQUEST_BODY_CURLPOST, http_post_data[0], 0, 1);

	} else {
		char *encoded;
		size_t encoded_len;

		if (SUCCESS != http_urlencode_hash_ex(fields, 1, NULL, 0, &encoded, &encoded_len)) {
			http_error(HE_WARNING, HTTP_E_ENCODING, "Could not encode post data");
			return NULL;
		}
		
		return http_request_body_init_rel(body, HTTP_REQUEST_BODY_CSTRING, encoded, encoded_len, 1);
	}
}
/* }}} */

/* {{{ void http_request_body_dtor(http_request_body *) */
PHP_HTTP_API void _http_request_body_dtor(http_request_body *body TSRMLS_DC)
{
	if (body && body->free) {
		switch (body->type)
		{
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
