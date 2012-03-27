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

#if PHP_HTTP_HAVE_CURL

typedef struct php_http_client_datashare_curl {
	CURLSH *handle;
} php_http_client_datashare_curl_t;


static void *php_http_curlsh_ctor(void *opaque TSRMLS_DC)
{
	return curl_share_init();
}

static void php_http_curlsh_dtor(void *opaque, void *handle TSRMLS_DC)
{
	curl_share_cleanup(handle);
}


/* datashare handler ops */

static php_http_client_datashare_t *php_http_client_datashare_curl_init(php_http_client_datashare_t *h, void *handle)
{
	php_http_client_datashare_curl_t *curl;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (!handle && !(handle = php_http_resource_factory_handle_ctor(h->rf TSRMLS_CC))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_DATASHARE, "could not initialize curl share handle");
		return NULL;
	}

	curl = ecalloc(1, sizeof(*curl));
	curl->handle = handle;
	h->ctx = curl;

	return h;
}

static void php_http_client_datashare_curl_dtor(php_http_client_datashare_t *h)
{
	php_http_client_datashare_curl_t *curl = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	php_http_resource_factory_handle_dtor(h->rf, curl->handle TSRMLS_CC);

	efree(curl);
	h->ctx = NULL;
}

static STATUS php_http_client_datashare_curl_attach(php_http_client_datashare_t *h, php_http_client_t *r)
{
	CURLcode rc;
	php_http_client_datashare_curl_t *curl = h->ctx;
	php_http_client_curl_t *recurl = r->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (CURLE_OK != (rc = curl_easy_setopt(recurl->handle, CURLOPT_SHARE, curl->handle))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_DATASHARE, "Could not attach request to the datashare: %s", curl_easy_strerror(rc));
		return FAILURE;
	}
	return SUCCESS;
}

static STATUS php_http_client_datashare_curl_detach(php_http_client_datashare_t *h, php_http_client_t *r)
{
	CURLcode rc;
	php_http_client_curl_t *recurl = r->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);


	if (CURLE_OK != (rc = curl_easy_setopt(recurl->handle, CURLOPT_SHARE, NULL))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_DATASHARE, "Could not detach request from the datashare: %s", curl_share_strerror(rc));
		return FAILURE;
	}
	return SUCCESS;
}

static STATUS php_http_client_datashare_curl_setopt(php_http_client_datashare_t *h, php_http_client_datashare_setopt_opt_t opt, void *arg)
{
	CURLSHcode rc;
	php_http_client_datashare_curl_t *curl = h->ctx;

	switch (opt) {
		case PHP_HTTP_CLIENT_DATASHARE_OPT_COOKIES:
			if (CURLSHE_OK != (rc = curl_share_setopt(curl->handle, *((zend_bool *) arg) ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, CURL_LOCK_DATA_COOKIE))) {
				TSRMLS_FETCH_FROM_CTX(h->ts);

				php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_DATASHARE, "Could not %s sharing of cookie data: %s",  *((zend_bool *) arg) ? "enable" : "disable", curl_share_strerror(rc));
				return FAILURE;
			}
			break;

		case PHP_HTTP_CLIENT_DATASHARE_OPT_RESOLVER:
			if (CURLSHE_OK != (rc = curl_share_setopt(curl->handle, *((zend_bool *) arg) ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, CURL_LOCK_DATA_DNS))) {
				TSRMLS_FETCH_FROM_CTX(h->ts);

				php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_DATASHARE, "Could not %s sharing of resolver data: %s",  *((zend_bool *) arg) ? "enable" : "disable", curl_share_strerror(rc));
				return FAILURE;
			}
			break;

#if PHP_HTTP_CURL_VERSION(7,23,0)
		case PHP_HTTP_CLIENT_DATASHARE_OPT_SSLSESSIONS:
			if (CURLSHE_OK != (rc = curl_share_setopt(curl->handle, *((zend_bool *) arg) ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, CURL_LOCK_DATA_SSL_SESSION))) {
				TSRMLS_FETCH_FROM_CTX(h->ts);

				php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_DATASHARE, "Could not %s sharing of SSL session data: %s",  *((zend_bool *) arg) ? "enable" : "disable", curl_share_strerror(rc));
				return FAILURE;
			}
			break;
#endif

		default:
			return FAILURE;
	}

	return SUCCESS;
}

static php_http_resource_factory_ops_t php_http_curlsh_resource_factory_ops = {
	php_http_curlsh_ctor,
	NULL,
	php_http_curlsh_dtor
};

static zend_class_entry *get_class_entry(void)
{
	return php_http_client_datashare_curl_class_entry;
}

static php_http_client_datashare_ops_t php_http_client_datashare_curl_ops = {
	&php_http_curlsh_resource_factory_ops,
	php_http_client_datashare_curl_init,
	NULL /* copy */,
	php_http_client_datashare_curl_dtor,
	NULL /*reset */,
	php_http_client_datashare_curl_attach,
	php_http_client_datashare_curl_detach,
	php_http_client_datashare_curl_setopt,
	(php_http_new_t) php_http_client_datashare_curl_object_new_ex,
	get_class_entry
};

PHP_HTTP_API php_http_client_datashare_ops_t *php_http_client_datashare_curl_get_ops(void)
{
	return &php_http_client_datashare_curl_ops;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpClientDataShare, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpClientDataShare, method, 0)
#define PHP_HTTP_RSHARE_ME(method, visibility)	PHP_ME(HttpClientDataShare, method, PHP_HTTP_ARGS(HttpClientDataShare, method), visibility)

zend_class_entry *php_http_client_datashare_curl_class_entry;
zend_function_entry php_http_client_datashare_curl_method_entry[] = {
	EMPTY_FUNCTION_ENTRY
};

zend_object_value php_http_client_datashare_curl_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_client_datashare_curl_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_client_datashare_curl_object_new_ex(zend_class_entry *ce, php_http_client_datashare_t *share, php_http_client_datashare_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_client_datashare_object_t *o;

	o = ecalloc(1, sizeof(*o));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (share) {
		o->share = share;
	} else {
		o->share = php_http_client_datashare_init(NULL, &php_http_client_datashare_curl_ops, NULL, NULL TSRMLS_CC);
	}

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, php_http_client_datashare_object_free, NULL TSRMLS_CC);
	ov.handlers = php_http_client_datashare_get_object_handlers();

	return ov;
}


PHP_MINIT_FUNCTION(http_client_datashare_curl)
{
	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_client_datashare.curl"), &php_http_curlsh_resource_factory_ops, NULL, NULL)) {
		return FAILURE;
	}

	PHP_HTTP_REGISTER_CLASS(http\\Client\\DataShare, CURL, http_client_datashare_curl, php_http_client_datashare_class_entry, 0);
	php_http_client_datashare_curl_class_entry->create_object = php_http_client_datashare_curl_object_new;
	return SUCCESS;
}

#endif /* PHP_HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

