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

static zend_class_entry *php_http_client_request_class_entry;
zend_class_entry *php_http_get_client_request_class_entry(void)
{
	return php_http_client_request_class_entry;
}

void php_http_client_options_set_subr(zval *this_ptr, char *key, size_t len, zval *opts, int overwrite);
void php_http_client_options_set(zval *this_ptr, zval *opts);
void php_http_client_options_get_subr(zval *this_ptr, char *key, size_t len, zval *return_value);

#define PHP_HTTP_CLIENT_REQUEST_OBJECT_INIT(obj) \
	do { \
		if (!obj->message) { \
			obj->message = php_http_message_init(NULL, PHP_HTTP_REQUEST, NULL); \
		} \
	} while(0)

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, url)
	ZEND_ARG_ARRAY_INFO(0, headers, 1)
	ZEND_ARG_OBJ_INFO(0, body, http\\Message\\Body, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, __construct)
{
	char *meth_str = NULL;
	size_t meth_len = 0;
	zval *zheaders = NULL, *zbody = NULL, *zurl = NULL;
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s!z!a!O!", &meth_str, &meth_len, &zurl, &zheaders, &zbody, php_http_get_message_body_class_entry()), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	if (obj->message) {
		php_http_message_set_type(obj->message, PHP_HTTP_REQUEST);
	} else {
		obj->message = php_http_message_init(NULL, PHP_HTTP_REQUEST, NULL);
	}

	if (zbody) {
		php_http_expect(SUCCESS == php_http_message_object_set_body(obj, zbody), unexpected_val, return);
	}
	if (meth_str && meth_len) {
		PHP_HTTP_INFO(obj->message).request.method = estrndup(meth_str, meth_len);
	}
	if (zurl) {
		php_http_url_t *url = php_http_url_from_zval(zurl, PHP_HTTP_URL_STDFLAGS);

		if (url) {
			PHP_HTTP_INFO(obj->message).request.url = php_http_url_mod(url, NULL, PHP_HTTP_URL_STDFLAGS);
			php_http_url_free(&url);
		}
	}
	if (zheaders) {
		array_copy(Z_ARRVAL_P(zheaders), &obj->message->hdrs);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setContentType, 0, 0, 1)
	ZEND_ARG_INFO(0, content_type)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setContentType)
{
	zend_string *ct_str;
	php_http_message_object_t *obj;
	zval zct;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "S", &ct_str), invalid_arg, return);

	if (ct_str->len && !strchr(ct_str->val, '/')) {
		php_http_throw(unexpected_val, "Content type \"%s\" does not seem to contain a primary and a secondary part", ct_str->val);
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_CLIENT_REQUEST_OBJECT_INIT(obj);

	ZVAL_STR_COPY(&zct, ct_str);
	zend_hash_str_update(&obj->message->hdrs, "Content-Type", lenof("Content-Type"), &zct);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getContentType, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getContentType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		zval *zct;

		PHP_HTTP_CLIENT_REQUEST_OBJECT_INIT(obj);

		php_http_message_update_headers(obj->message);
		zct = php_http_message_header(obj->message, ZEND_STRL("Content-Type"));
		if (zct) {
			RETURN_ZVAL(zct, 1, 0);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setQuery, 0, 0, 0)
	ZEND_ARG_INFO(0, query_data)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setQuery)
{
	zval *qdata = NULL, arr, str;
	php_http_message_object_t *obj;
	php_http_url_t *old_url = NULL, new_url = {NULL};
	unsigned flags = PHP_HTTP_URL_REPLACE;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "z!", &qdata), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_CLIENT_REQUEST_OBJECT_INIT(obj);

	ZVAL_NULL(&str);
	if (qdata) {
		array_init(&arr);

		php_http_expect(SUCCESS == php_http_querystring_update(&arr, qdata, &str), bad_querystring,
				zval_dtor(&arr);
				return;
		);

		new_url.query = Z_STRVAL(str);
		zval_dtor(&arr);
	} else {
		flags = PHP_HTTP_URL_STRIP_QUERY;
	}

	if (obj->message->http.info.request.url) {
		old_url = obj->message->http.info.request.url;
	}

	obj->message->http.info.request.url = php_http_url_mod(old_url, &new_url, flags);

	if (old_url) {
		php_http_url_free(&old_url);
	}
	zval_ptr_dtor(&str);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getQuery, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getQuery)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_CLIENT_REQUEST_OBJECT_INIT(obj);

		if (obj->message->http.info.request.url && obj->message->http.info.request.url->query) {
			RETVAL_STRING(obj->message->http.info.request.url->query);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_addQuery, 0, 0, 1)
	ZEND_ARG_INFO(0, query_data)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, addQuery)
{
	zval *qdata, arr, str;
	php_http_message_object_t *obj;
	php_http_url_t *old_url = NULL, new_url = {NULL};

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "z", &qdata), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_CLIENT_REQUEST_OBJECT_INIT(obj);

	array_init(&arr);
	ZVAL_NULL(&str);

	php_http_expect(SUCCESS == php_http_querystring_update(&arr, qdata, &str), bad_querystring,
			zval_dtor(&arr);
			return;
	);
	new_url.query = Z_STRVAL(str);
	zval_dtor(&arr);

	if (obj->message->http.info.request.url) {
		old_url = obj->message->http.info.request.url;
	}

	obj->message->http.info.request.url = php_http_url_mod(old_url, &new_url, PHP_HTTP_URL_JOIN_QUERY);

	if (old_url) {
		php_http_url_free(&old_url);
	}
	zval_ptr_dtor(&str);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setOptions)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set(getThis(), opts);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval tmp, *zoptions = zend_read_property(php_http_client_request_class_entry, getThis(), ZEND_STRL("options"), 0, &tmp);
		RETURN_ZVAL(zoptions, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_setSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, setSslOptions)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set_subr(getThis(), ZEND_STRL("ssl"), opts, 1);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_addSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, addSslOptions)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set_subr(getThis(), ZEND_STRL("ssl"), opts, 0);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientRequest_getSslOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientRequest, getSslOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRL("ssl"), return_value);
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

PHP_MINIT_FUNCTION(http_client_request)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Client", "Request", php_http_client_request_methods);
	php_http_client_request_class_entry = zend_register_internal_class_ex(&ce, php_http_message_get_class_entry());

	zend_declare_property_null(php_http_client_request_class_entry, ZEND_STRL("options"), ZEND_ACC_PROTECTED);

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

