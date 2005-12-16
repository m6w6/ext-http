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

#if defined(HTTP_HAVE_CURL) && defined(HTTP_HAVE_WRAPPER)

#include "php_streams.h"

#include "php_http_api.h"
#include "php_http_wrapper_api.h"
#include "php_http_request_api.h"
#include "php_http_headers_api.h"
#include "php_http_message_api.h"
#include "php_http_url_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

typedef struct {
	phpstr b;
	size_t p;
} http_wrapper_t;

static php_stream_ops http_stream_ops;

PHP_MINIT_FUNCTION(http_wrapper)
{
	php_unregister_url_stream_wrapper("http" TSRMLS_CC);
	return php_register_url_stream_wrapper("http", &http_wrapper TSRMLS_CC);
}

php_stream *http_wrapper_ex(php_stream_wrapper *wrapper, char *path, char *mode, int options, char **opened_path, php_stream_context *context STREAMS_DC TSRMLS_DC)
{
	zval opt_array, **opt_elem, *ssl_opt;
	http_request request;
	php_stream *stream;
	
	if (strpbrk(mode, "awx+")) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "HTTP wrapper does not support writeable connections.");
		return NULL;
	}

	INIT_PZVAL(&opt_array);
	array_init(&opt_array);
	
	http_request_init(&request);
	request.url = estrdup(path);
	
	/*
		HTTP options
	*/
	
	/* request method */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "method", &opt_elem)) {
		switch (Z_TYPE_PP(opt_elem))
		{
			case IS_LONG:
				if (http_request_method_exists(0, Z_LVAL_PP(opt_elem), NULL)) {
					request.meth = Z_LVAL_PP(opt_elem);
				}
			break;
			default:
			{
				ulong method;
				zval *orig = *opt_elem;
				convert_to_string_ex(opt_elem);
				if ((method = http_request_method_exists(1, 0, Z_STRVAL_PP(opt_elem)))) {
					request.meth = method;
				}
				if (orig != *opt_elem) zval_ptr_dtor(opt_elem);
			}
			break;
		}
	}
	
	/* header */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "header", &opt_elem)) {
		if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
			zval *headers;
			
			MAKE_STD_ZVAL(headers);
			array_init(headers);
			if (SUCCESS == http_parse_headers(Z_STRVAL_PP(opt_elem), headers)) {
				add_assoc_zval(&opt_array, "headers", headers);
			} else {
				zval_ptr_dtor(&headers);
			}
		}
	}
	
	/* user_agent */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "user_agent", &opt_elem)) {
		if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
			add_assoc_stringl(&opt_array, "useragent", Z_STRVAL_PP(opt_elem), Z_STRLEN_PP(opt_elem), 1);
		}
	}
	
	/* proxy */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "proxy", &opt_elem)) {
		if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
			if (strstr(Z_STRVAL_PP(opt_elem), "://")) {
				if (!strncasecmp(Z_STRVAL_PP(opt_elem), "tcp://", lenof("tcp://"))) {
					phpstr proxy;
					
					phpstr_init(&proxy);
					phpstr_appends(&proxy, "http://");
					phpstr_append(&proxy, Z_STRVAL_PP(opt_elem)+lenof("tcp://"), Z_STRLEN_PP(opt_elem)-lenof("tcp://"));
					phpstr_fix(&proxy);
					
					add_assoc_stringl(&opt_array, "proxy", PHPSTR_VAL(&proxy), PHPSTR_LEN(&proxy), 0);
				}
			} else {
				ZVAL_ADDREF(*opt_elem);
				add_assoc_zval(&opt_array, "proxy", *opt_elem);
			}
		}
	}
	
	/* redirect */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "max_redirects", &opt_elem)) {
		zval *orig = *opt_elem;
		
		convert_to_long_ex(opt_elem);
		if (0 < Z_LVAL_PP(opt_elem)) {
			add_assoc_long(&opt_array, "redirect", Z_LVAL_PP(opt_elem)-1);
		}
		if (orig != *opt_elem) {
			zval_ptr_dtor(opt_elem);
		}
	}
	
	/* request body */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "content", &opt_elem)) {
		if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
			request.body = http_request_body_init_ex(request.body, HTTP_REQUEST_BODY_CSTRING,
				estrndup(Z_STRVAL_PP(opt_elem), Z_STRLEN_PP(opt_elem)), Z_STRLEN_PP(opt_elem), 1);
		}
	}
	
	/*
		SSL options
	*/
	
	MAKE_STD_ZVAL(ssl_opt);
	array_init(ssl_opt);
	
	/* verify_peer */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "verify_peer", &opt_elem)) {
		if (zval_is_true(*opt_elem)) {
			add_assoc_bool(ssl_opt, "verifypeer", 1);
		}
	}
	
	/* cafile */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "cafile", &opt_elem)) {
		if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
			ZVAL_ADDREF(*opt_elem);
			add_assoc_zval(ssl_opt, "cainfo", *opt_elem);
		}
	}
	
	/* capath */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "capath", &opt_elem)) {
		if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
			ZVAL_ADDREF(*opt_elem);
			add_assoc_zval(ssl_opt, "capath", *opt_elem);
		}
	}
	
	/* local_cert */
	if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "local_cert", &opt_elem)) {
		if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
			zval **pass = NULL;
			
			ZVAL_ADDREF(*opt_elem);
			add_assoc_zval(ssl_opt, "cert", *opt_elem);
				
			if (SUCCESS == php_stream_context_get_option(context, wrapper->wops->label, "passphrase", &opt_elem)) {
				if (Z_TYPE_PP(opt_elem) == IS_STRING && Z_STRLEN_PP(opt_elem)) {
					ZVAL_ADDREF(*opt_elem);
					add_assoc_zval(ssl_opt, "certpasswd", *opt_elem);
				}
			}
		}
	}
	
	add_assoc_zval(&opt_array, "ssl", ssl_opt);
	
	if (SUCCESS == http_request_prepare(&request, Z_ARRVAL(opt_array))) {
		http_message *msg;
		
		http_request_exec(&request);
		
		if (opened_path) {
			char *url;
			if (CURLE_OK == curl_easy_getinfo(request.ch, CURLINFO_EFFECTIVE_URL, &url) && url) {
				*opened_path = estrdup(url);
			} else {
				*opened_path = NULL;
			}
		}
		
		if ((msg = http_message_parse(PHPSTR_VAL(&request.conv.response), PHPSTR_LEN(&request.conv.response)))) {
			http_wrapper_t *w = emalloc(sizeof(http_wrapper_t));
			
			w->p = 0;
			phpstr_init(&w->b);
			phpstr_append(&w->b, PHPSTR_VAL(&msg->body), PHPSTR_LEN(&msg->body));
			
			stream = php_stream_alloc(&http_stream_ops, w, NULL, mode);
			
			http_message_free(&msg);
		}
	}
	
	http_request_dtor(&request);
	
	zval_dtor(&opt_array);
	return stream;
}

static size_t http_wrapper_write(php_stream *stream, const char *buf, size_t count TSRMLS_DC)
{
	return 0;
}

static size_t http_wrapper_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
{
	http_wrapper_t *w = (http_wrapper_t *) stream->abstract;
	size_t len = MIN(count, w->b.used - w->p);
	
	if (len) {
		memcpy(buf, w->b.data + w->p, len);
		w->p += len;
	}
	
	return len;
}

static int http_wrapper_close(php_stream *stream, int close_handle TSRMLS_DC)
{
	phpstr_dtor(&((http_wrapper_t *)stream->abstract)->b);
	efree(stream->abstract);
	stream->abstract = NULL;
	return 0;
}

static int http_wrapper_flush(php_stream *stream TSRMLS_DC)
{
	return 0;
}

static int http_wrapper_seek(php_stream *stream, off_t offset, int whence, off_t *newoffset TSRMLS_DC)
{
	http_wrapper_t *w = (http_wrapper_t *) stream->abstract;
	off_t o;
	
	*newoffset = w->p;
	
	switch (whence)
	{
		case SEEK_SET:
			if (offset < 0 || offset > w->b.used) {
				return -1;
			}
			w->p = offset;
		break;
		
		case SEEK_END:
			o = w->b.used + offset;
			if (o < 0 || o > w->b.used) {
				return -1;
			}
			w->p = o;
		break;
		
		case SEEK_CUR:
			o = w->p + offset;
			if (o < 0 || o > w->b.used) {
				return -1;
			}
			w->p = o;
		break;
		
		default:
			return -1;
		break;
	}
	
	*newoffset = w->p;
	
	return 0;
}

static php_stream_ops http_stream_ops = {
	http_wrapper_write,
	http_wrapper_read,
	http_wrapper_close,
	http_wrapper_flush,
	"http",
	http_wrapper_seek,
	NULL,
	NULL,
	NULL,
};

static php_stream_wrapper_ops http_wrapper_ops = {
	http_wrapper_ex,
	NULL,
	NULL,
	NULL,
	NULL,
	"http",
	NULL,
	NULL,
	NULL,
	NULL
};

PHP_HTTP_API php_stream_wrapper http_wrapper = {
	&http_wrapper_ops,
	NULL,
	1
};

#endif /* HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */


