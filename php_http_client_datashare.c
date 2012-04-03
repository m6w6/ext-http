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
#include "php_http_client_datashare.h"

static int php_http_client_datashare_compare_handles(void *h1, void *h2);

PHP_HTTP_API php_http_client_datashare_t *php_http_client_datashare_init(php_http_client_datashare_t *h, php_http_client_datashare_ops_t *ops, php_http_resource_factory_t *rf, void *init_arg TSRMLS_DC)
{
	php_http_client_datashare_t *free_h = NULL;

	if (!h) {
		free_h = h = emalloc(sizeof(*h));
	}
	memset(h, sizeof(*h), 0);

	zend_llist_init(&h->clients, sizeof(zval *), ZVAL_PTR_DTOR, 0);
	h->ops = ops;
	if (rf) {
		h->rf = rf;
	} else if (ops->rsrc) {
		h->rf = php_http_resource_factory_init(NULL, h->ops->rsrc, h, NULL);
	}
	TSRMLS_SET_CTX(h->ts);

	if (h->ops->init) {
		if (!(h = h->ops->init(h, init_arg))) {
			if (free_h) {
				efree(free_h);
			}
		}
	}

	return h;
}

PHP_HTTP_API php_http_client_datashare_t *php_http_client_datashare_copy(php_http_client_datashare_t *from, php_http_client_datashare_t *to)
{
	if (from->ops->copy) {
		return from->ops->copy(from, to);
	}

	return NULL;
}

PHP_HTTP_API void php_http_client_datashare_dtor(php_http_client_datashare_t *h)
{
	if (h->ops->dtor) {
		h->ops->dtor(h);
	}
	zend_llist_destroy(&h->clients);
	php_http_resource_factory_free(&h->rf);
}

PHP_HTTP_API void php_http_client_datashare_free(php_http_client_datashare_t **h)
{
	php_http_client_datashare_dtor(*h);
	efree(*h);
	*h = NULL;
}

PHP_HTTP_API STATUS php_http_client_datashare_attach(php_http_client_datashare_t *h, zval *client)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->attach) {
		php_http_client_object_t *obj = zend_object_store_get_object(client TSRMLS_CC);

		if (SUCCESS == h->ops->attach(h, obj->client)) {
			Z_ADDREF_P(client);
			zend_llist_add_element(&h->clients, &client);
			return SUCCESS;
		}
	}

	return FAILURE;
}

static int php_http_client_datashare_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
}

PHP_HTTP_API STATUS php_http_client_datashare_detach(php_http_client_datashare_t *h, zval *client)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->detach) {
		php_http_client_object_t *obj = zend_object_store_get_object(client TSRMLS_CC);

		if (SUCCESS == h->ops->detach(h, obj->client)) {
			zend_llist_del_element(&h->clients, client, php_http_client_datashare_compare_handles);
			return SUCCESS;
		}
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_datashare_setopt(php_http_client_datashare_t *h, php_http_client_datashare_setopt_opt_t opt, void *arg)
{
	if (h->ops->setopt) {
		return h->ops->setopt(h, opt, arg);
	}
	return FAILURE;
}

static void detach(void *r, void *h TSRMLS_DC)
{
	((php_http_client_datashare_t *) h)->ops->detach(h, ((php_http_client_object_t *) zend_object_store_get_object(*((zval **) r) TSRMLS_CC))->client);
}

PHP_HTTP_API void php_http_client_datashare_reset(php_http_client_datashare_t *h)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->reset) {
		h->ops->reset(h);
	} else if (h->ops->detach) {
		zend_llist_apply_with_argument(&h->clients, detach, h TSRMLS_CC);
	}

	zend_llist_clean(&h->clients);
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpClientDataShare, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpClientDataShare, method, 0)
#define PHP_HTTP_RSHARE_ME(method, visibility)	PHP_ME(HttpClientDataShare, method, PHP_HTTP_ARGS(HttpClientDataShare, method), visibility)

PHP_HTTP_EMPTY_ARGS(__destruct);
PHP_HTTP_EMPTY_ARGS(reset);
PHP_HTTP_EMPTY_ARGS(count);

PHP_HTTP_BEGIN_ARGS(attach, 1)
	PHP_HTTP_ARG_OBJ(http\\Client, client, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(detach, 1)
	PHP_HTTP_ARG_OBJ(http\\Client, client, 0)
PHP_HTTP_END_ARGS;

static void php_http_client_datashare_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC);

static zend_class_entry *php_http_client_datashare_class_entry;

zend_class_entry *php_http_client_datashare_get_class_entry(void)
{
	return php_http_client_datashare_class_entry;
}

static zend_function_entry php_http_client_datashare_method_entry[] = {
	PHP_HTTP_RSHARE_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_HTTP_RSHARE_ME(count, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(attach, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(reset, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

static zend_object_handlers php_http_client_datashare_object_handlers;

zend_object_handlers *php_http_client_datashare_get_object_handlers(void)
{
	return &php_http_client_datashare_object_handlers;
}
static STATUS setopt(struct php_http_client_datashare *h, php_http_client_datashare_setopt_opt_t opt, void *arg)
{
	return SUCCESS;
}

static php_http_client_datashare_ops_t php_http_client_datashare_user_ops = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	setopt,
	(php_http_new_t) php_http_client_datashare_object_new_ex,
	php_http_client_datashare_get_class_entry

};
zend_object_value php_http_client_datashare_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_client_datashare_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_client_datashare_object_new_ex(zend_class_entry *ce, php_http_client_datashare_t *share, php_http_client_datashare_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_client_datashare_object_t *o;

	o = ecalloc(1, sizeof(*o));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	ov.handle = zend_objects_store_put(o, NULL, php_http_client_datashare_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_client_datashare_object_handlers;

	if (!(o->share = share)) {
		o->share = php_http_client_datashare_init(NULL, &php_http_client_datashare_user_ops, NULL, &ov TSRMLS_CC);
	}

	if (ptr) {
		*ptr = o;
	}

	return ov;
}

void php_http_client_datashare_object_free(void *object TSRMLS_DC)
{
	php_http_client_datashare_object_t *o = (php_http_client_datashare_object_t *) object;

	php_http_client_datashare_free(&o->share);
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static void php_http_client_datashare_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC)
{
	zend_property_info *pi;

	if ((pi = zend_get_property_info(php_http_client_datashare_class_entry, member, 1 TSRMLS_CC))) {
		zend_bool enable = i_zend_is_true(value);
		php_http_client_datashare_setopt_opt_t opt;
		php_http_client_datashare_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);

		if (!strcmp(pi->name, "cookie")) {
			opt = PHP_HTTP_CLIENT_DATASHARE_OPT_COOKIES;
		} else if (!strcmp(pi->name, "dns")) {
			opt = PHP_HTTP_CLIENT_DATASHARE_OPT_RESOLVER;
		} else if (!strcmp(pi->name, "ssl")) {
			opt = PHP_HTTP_CLIENT_DATASHARE_OPT_SSLSESSIONS;
		} else {
			return;
		}

		if (SUCCESS != php_http_client_datashare_setopt(obj->share, opt, &enable)) {
			return;
		}
	}

	zend_get_std_object_handlers()->write_property(object, member, value, literal_key TSRMLS_CC);
}

static zval **php_http_client_datashare_object_get_prop_ptr(zval *object, zval *member, const zend_literal *literal_key TSRMLS_DC)
{
	zend_property_info *pi;

	if ((pi = zend_get_property_info(php_http_client_datashare_class_entry, member, 1 TSRMLS_CC))) {
		return &php_http_property_proxy_init(NULL, object, member, NULL TSRMLS_CC)->myself;
	}

	return zend_get_std_object_handlers()->get_property_ptr_ptr(object, member, literal_key TSRMLS_CC);
}


PHP_METHOD(HttpClientDataShare, __destruct)
{
	php_http_client_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
	/* FIXME: move to php_http_client_datashare_dtor */
	if (SUCCESS == zend_parse_parameters_none()) {
		; /* we always want to clean up */
	}

	php_http_client_datashare_reset(obj->share);
}

PHP_METHOD(HttpClientDataShare, count)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(zend_llist_count(&obj->share->clients));
	}
	RETURN_FALSE;
}


PHP_METHOD(HttpClientDataShare, attach)
{
	zval *client;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &client, php_http_client_get_class_entry())) {
		php_http_client_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_client_datashare_attach(obj->share, client));
	}
	RETURN_FALSE;

}

PHP_METHOD(HttpClientDataShare, detach)
{
	zval *client;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &client, php_http_client_get_class_entry())) {
		php_http_client_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_client_datashare_detach(obj->share, client));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientDataShare, reset)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_client_datashare_reset(obj->share);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_MINIT_FUNCTION(http_client_datashare)
{
	PHP_HTTP_REGISTER_CLASS(http\\Client\\DataShare, AbstractDataShare, http_client_datashare, php_http_object_get_class_entry(), 0);
	php_http_client_datashare_class_entry->create_object = php_http_client_datashare_object_new;
	memcpy(&php_http_client_datashare_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_client_datashare_object_handlers.clone_obj = NULL;
	php_http_client_datashare_object_handlers.write_property = php_http_client_datashare_object_write_prop;
	php_http_client_datashare_object_handlers.get_property_ptr_ptr = php_http_client_datashare_object_get_prop_ptr;

	zend_class_implements(php_http_client_datashare_class_entry TSRMLS_CC, 1, spl_ce_Countable);

	zend_declare_property_bool(php_http_client_datashare_class_entry, ZEND_STRL("cookie"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(php_http_client_datashare_class_entry, ZEND_STRL("dns"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(php_http_client_datashare_class_entry, ZEND_STRL("ssl"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

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

