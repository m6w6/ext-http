/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

void php_http_client_options_set_subr(zval *this_ptr, char *key, size_t len, zval *opts, int overwrite TSRMLS_DC);
void php_http_client_options_set(zval *this_ptr, zval *opts TSRMLS_DC);
void php_http_client_options_get_subr(zval *this_ptr, char *key, size_t len, zval *return_value TSRMLS_DC);

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, url)
	ZEND_ARG_ARRAY_INFO(0, headers, 1)
	ZEND_ARG_OBJ_INFO(0, body, http\\Message\\Body, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, __construct)
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
				obj->message = php_http_message_init(NULL, PHP_HTTP_REQUEST, NULL TSRMLS_CC);
			}

			if (zbody) {
				php_http_message_object_set_body(obj, zbody TSRMLS_CC);
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
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setContentType, 0, 0, 1)
	ZEND_ARG_INFO(0, content_type)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setContentType)
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

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getContentType, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getContentType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		zval *zct;

		php_http_message_update_headers(obj->message);
		zct = php_http_message_header(obj->message, ZEND_STRL("Content-Type"), 1);
		if (zct) {
			RETURN_ZVAL(zct, 0, 1);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setQuery, 0, 0, 0)
	ZEND_ARG_INFO(0, query_data)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setQuery)
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

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getQuery, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getQuery)
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

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_addQuery, 0, 0, 1)
	ZEND_ARG_INFO(0, query_data)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, addQuery)
{
	zval *qdata;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &qdata)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_url *old_url = NULL, new_url = {NULL};

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

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set(getThis(), opts TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *zoptions = zend_read_property(php_http_client_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		RETURN_ZVAL(zoptions, 1, 0);
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setSslOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("ssl"), opts, 1 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_addSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, addSslOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("ssl"), opts, 0 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getSslOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getSslOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRS("ssl"), return_value TSRMLS_CC);
	}
}

static zend_function_entry php_http_client_request_methods[] = {
	PHP_ME(HttpClientRequest, __construct,    ai_HttpClientRequest___construct,    ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpClientRequest, setContentType, ai_HttpClientRequest_setContentType, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, getContentType, ai_HttpClientRequest_getContentType, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, setQuery,       ai_HttpClientRequest_setQuery,       ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, getQuery,       ai_HttpClientRequest_getQuery,       ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, addQuery,       ai_HttpClientRequest_addQuery,       ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, setOptions,     ai_HttpClientRequest_setOptions,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, getOptions,     ai_HttpClientRequest_getOptions,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, setSslOptions,  ai_HttpClientRequest_setSslOptions,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, getSslOptions,  ai_HttpClientRequest_getSslOptions,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientRequest, addSslOptions,  ai_HttpClientRequest_addSslOptions,  ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

zend_class_entry *php_http_client_request_class_entry;

PHP_MINIT_FUNCTION(http_client_request)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Client", "Request", php_http_client_request_methods);
	php_http_client_request_class_entry = zend_register_internal_class_ex(&ce, php_http_message_class_entry, NULL TSRMLS_CC);

	zend_declare_property_null(php_http_client_request_class_entry, ZEND_STRL("options"), ZEND_ACC_PROTECTED TSRMLS_CC);

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

