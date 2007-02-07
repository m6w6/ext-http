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

#if defined(HAVE_CURL_GETFORMDATA) && !defined(HAVE_CURL_FORMGET)
struct FormData {
	struct FormData *next;
	int type;
	char *line;
	size_t length;
};

CURLcode Curl_getFormData(struct FormData **, struct curl_httppost *post, curl_off_t *size);

static char *file_get_contents(char *file, size_t *len TSRMLS_DC)
{
	php_stream *s = NULL;
	char *buf = NULL;
	
	if ((s = php_stream_open_wrapper_ex(file, "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL, HTTP_DEFAULT_STREAM_CONTEXT))) {
		*len = php_stream_copy_to_mem(s, &buf, (size_t) -1, 0);
		php_stream_close(s);
	} else {
		*len = 0;
	}
	return buf;
}

static int curl_formget(struct FormData *post, phpstr *str TSRMLS_DC)
{
	int fgc_error = 0;
	char *fdata;
	size_t fsize;
	struct FormData *next, *pptr = post;
	
	while (pptr) {
		next = pptr->next;
		
		if (!fgc_error) {
			if (pptr->type) {
				if ((fdata = file_get_contents(pptr->line, &fsize TSRMLS_CC))) {
					phpstr_append(str, fdata, fsize);
					efree(fdata);
				} else {
					fgc_error = 1;
				}
			} else {
				phpstr_append(str, pptr->line, pptr->length);
			}
		}
		
		curl_free(pptr->line);
		curl_free(pptr);
		pptr = next;
	}
	
	return fgc_error;
}
#endif


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
		zval **data;
		HashPosition pos;
		struct curl_httppost *http_post_data[2] = {NULL, NULL};

		/* normal data */
		if (fields) {
			FOREACH_HASH_KEYVAL(pos, fields, key, data) {
				if (key.type == HASH_KEY_IS_STRING) {
					CURLcode err;
					zval *orig = *data;
					
					convert_to_string_ex(data);
					err = curl_formadd(&http_post_data[0], &http_post_data[1],
						CURLFORM_COPYNAME,			key.str,
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
				}
			}
		}

		/* file data */
		FOREACH_HASH_VAL(pos, files, data) {
			zval **file, **type, **name;
			
			if (Z_TYPE_PP(data) != IS_ARRAY) {
				http_error(HE_NOTICE, HTTP_E_INVALID_PARAM, "Unrecognized type of post file array entry");
			} else if (	SUCCESS != zend_hash_find(Z_ARRVAL_PP(data), "name", sizeof("name"), (void *) &name) ||
						SUCCESS != zend_hash_find(Z_ARRVAL_PP(data), "type", sizeof("type"), (void *) &type) ||
						SUCCESS != zend_hash_find(Z_ARRVAL_PP(data), "file", sizeof("file"), (void *) &file)) {
				http_error(HE_NOTICE, HTTP_E_INVALID_PARAM, "Post file array entry misses either 'name', 'type' or 'file' entry");
			} else {
				CURLcode err;
				const char *path;
				zval *ofile = *file, *otype = *type, *oname = *name;
				
				convert_to_string_ex(file);
				convert_to_string_ex(type);
				convert_to_string_ex(name);
				
				HTTP_CHECK_OPEN_BASEDIR(Z_STRVAL_PP(file), curl_formfree(http_post_data[0]); return NULL);
				
				/* this is blatant but should be sufficient for most cases */
				if (strncasecmp(Z_STRVAL_PP(file), "file://", lenof("file://"))) {
					path = Z_STRVAL_PP(file);
				} else {
					path = Z_STRVAL_PP(file) + lenof("file://");
				}
				
				err = curl_formadd(&http_post_data[0], &http_post_data[1],
					CURLFORM_COPYNAME,		Z_STRVAL_PP(name),
					CURLFORM_FILE,			path,
					CURLFORM_CONTENTTYPE,	Z_STRVAL_PP(type),
					CURLFORM_END
				);
				
				if (ofile != *file) zval_ptr_dtor(file);
				if (otype != *type) zval_ptr_dtor(type);
				if (oname != *name) zval_ptr_dtor(name);
				
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
#if defined(HAVE_CURL_FORMGET)
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
#elif defined(HAVE_CURL_GETFORMDATA)
			struct FormData *data;
			curl_off_t size;
			
			if (!Curl_getFormData(&data, body->data, &size)) {
				phpstr str;
				
				phpstr_init_ex(&str, (size_t) size, 0);
				if (curl_formget(data, &str TSRMLS_CC)) {
					phpstr_dtor(&str);
				} else {
					phpstr_fix(&str);
					*buf = PHPSTR_VAL(&str);
					*len = PHPSTR_LEN(&len);
					return SUCCESS;
				}
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
