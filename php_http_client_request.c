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

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpClientRequest, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpClientRequest, method, 0)
#define PHP_HTTP_CLIENT_REQUEST_ME(method, visibility)	PHP_ME(HttpClientRequest, method, PHP_HTTP_ARGS(HttpClientRequest, method), visibility)
#define PHP_HTTP_CLIENT_REQUEST_ALIAS(method, func)		PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpClientRequest, method))
#define PHP_HTTP_CLIENT_REQUEST_MALIAS(me, al, vis)		ZEND_FENTRY(me, ZEND_MN(HttpClientRequest_##al), PHP_HTTP_ARGS(HttpClientRequest, al), vis)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(method, 0)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_ARR(headers, 1, 0)
	PHP_HTTP_ARG_OBJ(http\\Message\\Body, body, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getContentType);
PHP_HTTP_BEGIN_ARGS(setContentType, 1)
	PHP_HTTP_ARG_VAL(content_type, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getQuery);
PHP_HTTP_BEGIN_ARGS(setQuery, 0)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addQuery, 1)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;


PHP_METHOD(HttpClientRequest, __construct)
{
	char *meth_str = NULL, *url_str = NULL;
	int meth_len = 0, url_len = 0;
	zval *zheaders = NULL, *zbody = NULL;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!s!a!O!", &meth_str, &meth_len, &url_str, &url_len, &zheaders, &zbody, php_http_message_body_class_entry)) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			if (obj->message) {
				php_http_message_set_type(obj->message, PHP_HTTP_REQUEST);
			} else {
				obj->message = php_http_message_init(NULL, PHP_HTTP_REQUEST TSRMLS_CC);
			}

			if (meth_str && meth_len) {
				PHP_HTTP_INFO(obj->message).request.method = estrndup(meth_str, meth_len);
			}
			if (url_str && url_len) {
				PHP_HTTP_INFO(obj->message).request.url = estrndup(url_str, url_len);
			}
			if (zheaders) {
				array_copy(Z_ARRVAL_P(zheaders), &obj->message->hdrs);
			}
			if (zbody) {
				php_http_message_body_object_t *body_obj = zend_object_store_get_object(zbody TSRMLS_CC);

				php_http_message_body_dtor(&obj->message->body);
				php_http_message_body_copy(body_obj->body, &obj->message->body, 0);
				Z_OBJ_ADDREF_P(zbody);
				obj->body = Z_OBJVAL_P(zbody);
			}
		}
	} end_error_handling();
}


PHP_METHOD(HttpClientRequest, setContentType)
{
	char *ct_str;
	int ct_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ct_str, &ct_len)) {
		int invalid = 0;

		if (ct_len) {
			PHP_HTTP_CHECK_CONTENT_TYPE(ct_str, invalid = 1);
		}

		if (!invalid) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			zval *zct;

			MAKE_STD_ZVAL(zct);
			ZVAL_STRINGL(zct, ct_str, ct_len, 1);
			zend_hash_update(&obj->message->hdrs, "Content-Type", sizeof("Content-Type"), (void *) &zct, sizeof(void *), NULL);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientRequest, getContentType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		zval *zct = php_http_message_header(obj->message, ZEND_STRL("Content-Type"), 1);

		RETURN_ZVAL(zct, 0, 0);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientRequest, setQuery)
{
	zval *qdata = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z!", &qdata)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_url *old_url = NULL, new_url = {NULL};
		char empty[] = "";

		if (qdata) {
			zval arr, str;

			INIT_PZVAL(&arr);
			array_init(&arr);
			INIT_PZVAL(&str);
			ZVAL_NULL(&str);

			php_http_querystring_update(&arr, qdata, &str TSRMLS_CC);
			new_url.query = Z_STRVAL(str);
			zval_dtor(&arr);
		} else {
			new_url.query = &empty[0];
		}

		if (obj->message->http.info.request.url) {
			old_url = php_url_parse(obj->message->http.info.request.url);
			efree(obj->message->http.info.request.url);
		}

		php_http_url(PHP_HTTP_URL_REPLACE, old_url, &new_url, NULL, &obj->message->http.info.request.url, NULL TSRMLS_CC);

		if (old_url) {
			php_url_free(old_url);
		}
		if (new_url.query != &empty[0]) {
			STR_FREE(new_url.query);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientRequest, getQuery)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->message->http.info.request.url) {
			php_url *purl = php_url_parse(obj->message->http.info.request.url);

			if (purl) {
				if (purl->query) {
					RETVAL_STRING(purl->query, 0);
					purl->query = NULL;
				}
				php_url_free(purl);
			}
		}
	}
}

PHP_METHOD(HttpClientRequest, addQuery)
{
	zval *qdata;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &qdata)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_url *old_url = NULL, new_url = {NULL};
		char empty[] = "";

		zval arr, str;

		INIT_PZVAL(&arr);
		array_init(&arr);
		INIT_PZVAL(&str);
		ZVAL_NULL(&str);

		php_http_querystring_update(&arr, qdata, &str TSRMLS_CC);
		new_url.query = Z_STRVAL(str);
		zval_dtor(&arr);

		if (obj->message->http.info.request.url) {
			old_url = php_url_parse(obj->message->http.info.request.url);
			efree(obj->message->http.info.request.url);
		}

		php_http_url(PHP_HTTP_URL_JOIN_QUERY, old_url, &new_url, NULL, &obj->message->http.info.request.url, NULL TSRMLS_CC);

		if (old_url) {
			php_url_free(old_url);
		}
		STR_FREE(new_url.query);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}


zend_class_entry *php_http_client_request_class_entry;
zend_function_entry php_http_client_request_method_entry[] = {
	PHP_HTTP_CLIENT_REQUEST_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_CLIENT_REQUEST_ME(getQuery, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(setQuery, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(addQuery, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(getContentType, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(setContentType, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_client_request)
{
	PHP_HTTP_REGISTER_CLASS(http\\Client, Request, http_client_request, php_http_message_class_entry, 0);

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

