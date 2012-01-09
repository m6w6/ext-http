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

#include <curl/curl.h>
#include <ext/standard/php_string.h>
#include <ext/spl/spl_iterators.h>

static int php_http_request_datashare_compare_handles(void *h1, void *h2);

#ifdef ZTS
static void *php_http_request_datashare_locks_init(void);
static void php_http_request_datashare_locks_dtor(void *l);
static void php_http_request_datashare_lock_func(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr);
static void php_http_request_datashare_unlock_func(CURL *handle, curl_lock_data data, void *userptr);
static MUTEX_T php_http_request_datashare_global_shares_lock;
#endif
static HashTable php_http_request_datashare_global_shares;
php_http_request_datashare_t *php_http_request_datashare_global_get(const char *driver_str, size_t driver_len TSRMLS_DC)
{
	php_http_request_datashare_t *s = NULL, **s_ptr;
	char *lower_str = php_strtolower(estrndup(driver_str, driver_len), driver_len);

#ifdef ZTS
	tsrm_mutex_lock(php_http_request_datashare_global_shares_lock);
#endif
	if (zend_hash_find(&php_http_request_datashare_global_shares, lower_str, driver_len + 1, (void *) &s_ptr)) {
		s = *s_ptr;
	} else {
		php_http_request_factory_driver_t driver;

		if ((SUCCESS == php_http_request_factory_get_driver(driver_str, driver_len, &driver)) && driver.request_datashare_ops) {
			s = php_http_request_datashare_init(NULL, driver.request_datashare_ops, NULL, NULL, 1 TSRMLS_CC);
			zend_hash_add(&php_http_request_datashare_global_shares, lower_str, driver_len + 1, &s, sizeof(php_http_request_datashare_t *), NULL);
		}
	}
#ifdef ZTS
	tsrm_mutex_unlock(php_http_request_datashare_global_shares_lock);
#endif

	efree(lower_str);
	return s;
}

PHP_HTTP_API php_http_request_datashare_t *php_http_request_datashare_init(php_http_request_datashare_t *h, php_http_request_datashare_ops_t *ops, php_http_resource_factory_t *rf, void *init_arg, zend_bool persistent TSRMLS_DC)
{
	php_http_request_datashare_t *free_h = NULL;

	if (!h) {
		free_h = h = pemalloc(sizeof(*h), persistent);
	}
	memset(h, sizeof(*h), 0);

	if (!(h->persistent = persistent)) {
		h->requests = emalloc(sizeof(*h->requests));
		zend_llist_init(h->requests, sizeof(zval *), ZVAL_PTR_DTOR, 0);
		TSRMLS_SET_CTX(h->ts);
	}
	h->ops = ops;
	h->rf = rf ? rf : php_http_resource_factory_init(NULL, h->ops->rsrc, NULL, NULL);

	if (h->ops->init) {
		if (!(h = h->ops->init(h, init_arg))) {
			if (free_h) {
				pefree(free_h, persistent);
			}
		}
	}

	return h;
}

PHP_HTTP_API php_http_request_datashare_t *php_http_request_datashare_copy(php_http_request_datashare_t *from, php_http_request_datashare_t *to)
{
	if (from->ops->copy) {
		return from->ops->copy(from, to);
	}

	return NULL;
}

PHP_HTTP_API void php_http_request_datashare_dtor(php_http_request_datashare_t *h)
{
	if (h->ops->dtor) {
		h->ops->dtor(h);
	}
	if (h->requests) {
		zend_llist_destroy(h->requests);
		pefree(h->requests, h->persistent);
		h->requests = NULL;
	}

	if (h->persistent_handle_id) {
		zval_ptr_dtor(&h->persistent_handle_id);
	}
}

PHP_HTTP_API void php_http_request_datashare_free(php_http_request_datashare_t **h)
{
	php_http_request_datashare_dtor(*h);
	pefree(*h, (*h)->persistent);
	*h = NULL;
}

PHP_HTTP_API STATUS php_http_request_datashare_attach(php_http_request_datashare_t *h, zval *request)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->attach) {
		php_http_request_object_t *obj = zend_object_store_get_object(request TSRMLS_CC);

		if (SUCCESS == h->ops->attach(h, obj->request)) {
			Z_ADDREF_P(request);
			zend_llist_add_element(PHP_HTTP_REQUEST_DATASHARE_REQUESTS(h), &request);
			return SUCCESS;
		}
	}

	return FAILURE;
}

static int php_http_request_datashare_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
}

PHP_HTTP_API STATUS php_http_request_datashare_detach(php_http_request_datashare_t *h, zval *request)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->detach) {
		php_http_request_object_t *obj = zend_object_store_get_object(request TSRMLS_CC);

		if (SUCCESS == h->ops->detach(h, obj->request)) {
			zend_llist_del_element(PHP_HTTP_REQUEST_DATASHARE_REQUESTS(h), request, php_http_request_datashare_compare_handles);
			return SUCCESS;
		}
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_request_datashare_setopt(php_http_request_datashare_t *h, php_http_request_datashare_setopt_opt_t opt, void *arg)
{
	if (h->ops->setopt) {
		return h->ops->setopt(h, opt, arg);
	}
	return FAILURE;
}

static void detach(void *r, void *h TSRMLS_DC)
{
	((php_http_request_datashare_t *) h)->ops->detach(h, ((php_http_request_object_t *) zend_object_store_get_object(*((zval **) r) TSRMLS_CC))->request);
}

PHP_HTTP_API void php_http_request_datashare_reset(php_http_request_datashare_t *h)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->reset) {
		h->ops->reset(h);
	} else if (h->ops->detach) {
		zend_llist_apply_with_argument(PHP_HTTP_REQUEST_DATASHARE_REQUESTS(h), detach, h TSRMLS_CC);
	}

	zend_llist_clean(PHP_HTTP_REQUEST_DATASHARE_REQUESTS(h));
}

static void php_http_request_datashare_global_requests_dtor(void *el)
{
	//php_http_request_datashare_detach(php_http_request_datashare_global_get(), *((zval **) el));
	zval_ptr_dtor(el);
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpRequestDataShare, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpRequestDataShare, method, 0)
#define PHP_HTTP_RSHARE_ME(method, visibility)	PHP_ME(HttpRequestDataShare, method, PHP_HTTP_ARGS(HttpRequestDataShare, method), visibility)

PHP_HTTP_EMPTY_ARGS(__construct);
PHP_HTTP_EMPTY_ARGS(__destruct);
PHP_HTTP_EMPTY_ARGS(reset);
PHP_HTTP_EMPTY_ARGS(count);

PHP_HTTP_BEGIN_ARGS(attach, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(detach, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request, 0)
PHP_HTTP_END_ARGS;

static void php_http_request_datashare_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC);

#define php_http_request_datashare_class_entry php_http_request_datashare_class_entry
zend_class_entry *php_http_request_datashare_class_entry;
zend_function_entry php_http_request_datashare_method_entry[] = {
	PHP_HTTP_RSHARE_ME(__construct, ZEND_ACC_PRIVATE|ZEND_ACC_CTOR)
	PHP_HTTP_RSHARE_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_HTTP_RSHARE_ME(count, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(attach, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(reset, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers php_http_request_datashare_object_handlers;

zend_object_value php_http_request_datashare_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_request_datashare_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_request_datashare_object_new_ex(zend_class_entry *ce, php_http_request_datashare_t *share, php_http_request_datashare_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_request_datashare_object_t *o;

	o = ecalloc(1, sizeof(*o));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (share) {
		o->share = share;
	} else {
		o->share = php_http_request_datashare_init(NULL, NULL, NULL, NULL, 0 TSRMLS_CC);
	}

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, php_http_request_datashare_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_request_datashare_object_handlers;

	return ov;
}

void php_http_request_datashare_object_free(void *object TSRMLS_DC)
{
	php_http_request_datashare_object_t *o = (php_http_request_datashare_object_t *) object;

	if (!o->share->persistent) {
		php_http_request_datashare_free(&o->share);
	}
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static void php_http_request_datashare_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC)
{
	zend_property_info *pi;

	if ((pi = zend_get_property_info(php_http_request_datashare_class_entry, member, 1 TSRMLS_CC))) {
		zend_bool enable = i_zend_is_true(value);
		php_http_request_datashare_setopt_opt_t opt;
		php_http_request_datashare_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);

		if (!strcmp(pi->name, "cookie")) {
			opt = PHP_HTTP_REQUEST_DATASHARE_OPT_COOKIES;
		} else if (!strcmp(pi->name, "dns")) {
			opt = PHP_HTTP_REQUEST_DATASHARE_OPT_RESOLVER;
		} else {
			return;
		}

		if (SUCCESS != php_http_request_datashare_setopt(obj->share, opt, &enable)) {
			return;
		}
	}

	zend_get_std_object_handlers()->write_property(object, member, value, literal_key TSRMLS_CC);
}

static zval **php_http_request_datashare_object_get_prop_ptr(zval *object, zval *member, const zend_literal *literal_key TSRMLS_DC)
{
	zend_property_info *pi;

	if ((pi = zend_get_property_info(php_http_request_datashare_class_entry, member, 1 TSRMLS_CC))) {
		return &php_http_property_proxy_init(NULL, object, member TSRMLS_CC)->myself;
	}

	return zend_get_std_object_handlers()->get_property_ptr_ptr(object, member, literal_key TSRMLS_CC);
}


PHP_METHOD(HttpRequestDataShare, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		zend_parse_parameters_none();
	} end_error_handling();
}

PHP_METHOD(HttpRequestDataShare, __destruct)
{
	php_http_request_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	if (SUCCESS == zend_parse_parameters_none()) {
		; /* we always want to clean up */
	}

	php_http_request_datashare_reset(obj->share);
}

PHP_METHOD(HttpRequestDataShare, count)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(zend_llist_count(PHP_HTTP_REQUEST_DATASHARE_REQUESTS(obj->share)));
	}
	RETURN_FALSE;
}


PHP_METHOD(HttpRequestDataShare, attach)
{
	zval *request;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_request_class_entry)) {
		php_http_request_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_request_datashare_attach(obj->share, request));
	}
	RETURN_FALSE;

}

PHP_METHOD(HttpRequestDataShare, detach)
{
	zval *request;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_request_class_entry)) {
		php_http_request_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_request_datashare_detach(obj->share, request));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestDataShare, reset)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_request_datashare_reset(obj->share);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_MINIT_FUNCTION(http_request_datashare)
{
	PHP_HTTP_REGISTER_CLASS(http\\Request, DataShare, http_request_datashare, php_http_object_class_entry, 0);
	php_http_request_datashare_class_entry->create_object = php_http_request_datashare_object_new;
	memcpy(&php_http_request_datashare_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_request_datashare_object_handlers.clone_obj = NULL;
	php_http_request_datashare_object_handlers.write_property = php_http_request_datashare_object_write_prop;
	php_http_request_datashare_object_handlers.get_property_ptr_ptr = php_http_request_datashare_object_get_prop_ptr;

	zend_class_implements(php_http_request_datashare_class_entry TSRMLS_CC, 1, spl_ce_Countable);

	zend_declare_property_null(php_http_request_datashare_class_entry, ZEND_STRL("instance"), (ZEND_ACC_STATIC|ZEND_ACC_PRIVATE) TSRMLS_CC);
	zend_declare_property_bool(php_http_request_datashare_class_entry, ZEND_STRL("cookie"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(php_http_request_datashare_class_entry, ZEND_STRL("dns"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

#ifdef ZTS
	php_http_request_datashare_global_shares_lock = tsrm_mutex_alloc();
#endif
	zend_hash_init(&php_http_request_datashare_global_shares, 0, NULL, (dtor_func_t) php_http_request_datashare_free, 1);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_request_datashare)
{
	zend_hash_destroy(&php_http_request_datashare_global_shares);
#ifdef ZTS
	tsrm_mutex_free(php_http_request_datashare_global_shares_lock);
#endif

	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_request_datashare)
{
	zend_llist_init(&PHP_HTTP_G->request_datashare.requests, sizeof(zval *), php_http_request_datashare_global_requests_dtor, 0);

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_request_datashare)
{
	zend_llist_destroy(&PHP_HTTP_G->request_datashare.requests);

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

