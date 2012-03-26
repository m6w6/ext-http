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

PHP_HTTP_API php_http_client_pool_t *php_http_client_pool_init(php_http_client_pool_t *h, php_http_client_pool_ops_t *ops, php_http_resource_factory_t *rf, void *init_arg TSRMLS_DC)
{
	php_http_client_pool_t *free_h = NULL;

	if (!h) {
		free_h = h = emalloc(sizeof(*h));
	}
	memset(h, 0, sizeof(*h));

	h->ops = ops;
	h->rf = rf ? rf : php_http_resource_factory_init(NULL, h->ops->rsrc, NULL, NULL);
	zend_llist_init(&h->clients.attached, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	zend_llist_init(&h->clients.finished, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	TSRMLS_SET_CTX(h->ts);

	if (h->ops->init) {
		if (!(h = h->ops->init(h, init_arg))) {
			php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_POOL, "Could not initialize request pool");
			if (free_h) {
				efree(h);
			}
		}
	}

	return h;
}

PHP_HTTP_API php_http_client_pool_t *php_http_client_pool_copy(php_http_client_pool_t *from, php_http_client_pool_t *to)
{
	if (from->ops->copy) {
		return from->ops->copy(from, to);
	}

	return NULL;
}

PHP_HTTP_API void php_http_client_pool_dtor(php_http_client_pool_t *h)
{
	if (h->ops->dtor) {
		h->ops->dtor(h);
	}

	zend_llist_clean(&h->clients.finished);
	zend_llist_clean(&h->clients.attached);

	php_http_resource_factory_free(&h->rf);
}

PHP_HTTP_API void php_http_client_pool_free(php_http_client_pool_t **h) {
	if (*h) {
		php_http_client_pool_dtor(*h);
		efree(*h);
		*h = NULL;
	}
}

PHP_HTTP_API STATUS php_http_client_pool_attach(php_http_client_pool_t *h, zval *client)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->attach) {
		char *url = NULL;
		char *m = NULL;
		php_http_message_body_t *body = NULL;
		php_http_client_object_t *obj = zend_object_store_get_object(client TSRMLS_CC);

		if (SUCCESS != php_http_client_object_requesthandler(obj, client, &m, &url, &body TSRMLS_CC)) {
			return FAILURE;
		}
		if (SUCCESS == h->ops->attach(h, obj->client, m, url, body)) {
			STR_FREE(url);
			Z_ADDREF_P(client);
			zend_llist_add_element(&h->clients.attached, &client);
			return SUCCESS;
		}
		STR_FREE(url);
	}

	return FAILURE;
}

static int php_http_client_pool_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
}


PHP_HTTP_API STATUS php_http_client_pool_detach(php_http_client_pool_t *h, zval *client)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->detach) {
		php_http_client_object_t *obj = zend_object_store_get_object(client TSRMLS_CC);

		if (SUCCESS == h->ops->detach(h, obj->client)) {
			zend_llist_del_element(&h->clients.finished, client, php_http_client_pool_compare_handles);
			zend_llist_del_element(&h->clients.attached, client, php_http_client_pool_compare_handles);
			return SUCCESS;
		}
	}

	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_pool_wait(php_http_client_pool_t *h, struct timeval *custom_timeout)
{
	if (h->ops->wait) {
		return h->ops->wait(h, custom_timeout);
	}

	return FAILURE;
}

PHP_HTTP_API int php_http_client_pool_once(php_http_client_pool_t *h)
{
	if (h->ops->once) {
		return h->ops->once(h);
	}

	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_pool_exec(php_http_client_pool_t *h)
{
	if (h->ops->exec) {
		return h->ops->exec(h);
	}

	return FAILURE;
}

static void detach(void *r, void *h TSRMLS_DC)
{
	((php_http_client_pool_t *) h)->ops->detach(h, ((php_http_client_object_t *) zend_object_store_get_object(*((zval **) r) TSRMLS_CC))->client);
}

PHP_HTTP_API void php_http_client_pool_reset(php_http_client_pool_t *h)
{
	if (h->ops->reset) {
		h->ops->reset(h);
	} else if (h->ops->detach) {
		TSRMLS_FETCH_FROM_CTX(h->ts);

		zend_llist_apply_with_argument(&h->clients.attached, detach, h TSRMLS_CC);
	}

	zend_llist_clean(&h->clients.attached);
	zend_llist_clean(&h->clients.finished);
}

PHP_HTTP_API STATUS php_http_client_pool_setopt(php_http_client_pool_t *h, php_http_client_pool_setopt_opt_t opt, void *arg)
{
	if (h->ops->setopt) {
		return h->ops->setopt(h, opt, arg);
	}

	return FAILURE;
}

PHP_HTTP_API void php_http_client_pool_requests(php_http_client_pool_t *h, zval ***attached, zval ***finished)
{
	zval **handle;
	int i, count;

	if (attached) {
		if ((count = zend_llist_count(&h->clients.attached))) {
			*attached = ecalloc(count + 1 /* terminating NULL */, sizeof(zval *));

			for (i = 0, handle = zend_llist_get_first(&h->clients.attached); handle; handle = zend_llist_get_next(&h->clients.attached)) {
				Z_ADDREF_PP(handle);
				(*attached)[i++] = *handle;
			}
		} else {
			*attached = NULL;
		}
	}

	if (finished) {
		if ((count = zend_llist_count(&h->clients.finished))) {
			*finished = ecalloc(count + 1 /* terminating NULL */, sizeof(zval *));

			for (i = 0, handle = zend_llist_get_first(&h->clients.finished); handle; handle = zend_llist_get_next(&h->clients.finished)) {
				Z_ADDREF_PP(handle);
				(*finished)[i++] = *handle;
			}
		} else {
			*finished = NULL;
		}
	}
}

/*#*/

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpClientPool, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpClientPool, method, 0)
#define PHP_HTTP_REQPOOL_ME(method, visibility)	PHP_ME(HttpClientPool, method, PHP_HTTP_ARGS(HttpClientPool, method), visibility)

PHP_HTTP_EMPTY_ARGS(__destruct);
PHP_HTTP_EMPTY_ARGS(reset);

PHP_HTTP_BEGIN_ARGS(attach, 1)
	PHP_HTTP_ARG_OBJ(http\\Client, client, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(detach, 1)
	PHP_HTTP_ARG_OBJ(http\\Client, client, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(send);
PHP_HTTP_EMPTY_ARGS(once);
PHP_HTTP_BEGIN_ARGS(wait, 0)
	PHP_HTTP_ARG_VAL(timeout, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(valid);
PHP_HTTP_EMPTY_ARGS(current);
PHP_HTTP_EMPTY_ARGS(key);
PHP_HTTP_EMPTY_ARGS(next);
PHP_HTTP_EMPTY_ARGS(rewind);

PHP_HTTP_EMPTY_ARGS(count);

PHP_HTTP_EMPTY_ARGS(getAttachedRequests);
PHP_HTTP_EMPTY_ARGS(getFinishedRequests);

PHP_HTTP_BEGIN_ARGS(enablePipelining, 0)
	PHP_HTTP_ARG_VAL(enable, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(enableEvents, 0)
	PHP_HTTP_ARG_VAL(enable, 0)
PHP_HTTP_END_ARGS;

zend_class_entry *php_http_client_pool_class_entry;
zend_function_entry php_http_client_pool_method_entry[] = {
	PHP_HTTP_REQPOOL_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_HTTP_REQPOOL_ME(attach, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(send, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(reset, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQPOOL_ME(once, ZEND_ACC_PROTECTED)
	PHP_HTTP_REQPOOL_ME(wait, ZEND_ACC_PROTECTED)

	/* implements Iterator */
	PHP_HTTP_REQPOOL_ME(valid, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(current, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(key, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(next, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(rewind, ZEND_ACC_PUBLIC)

	/* implmenents Countable */
	PHP_HTTP_REQPOOL_ME(count, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQPOOL_ME(getAttachedRequests, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(getFinishedRequests, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQPOOL_ME(enablePipelining, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(enableEvents, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

static zend_object_handlers php_http_client_pool_object_handlers;

extern zend_object_handlers *php_http_client_pool_get_object_handlers(void)
{
	return &php_http_client_pool_object_handlers;
}

zend_object_value php_http_client_pool_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_client_pool_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_client_pool_object_new_ex(zend_class_entry *ce, php_http_client_pool_t *p, php_http_client_pool_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_client_pool_object_t *o;

	o = ecalloc(1, sizeof(php_http_client_pool_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (!(o->pool = p)) {
		o->pool = php_http_client_pool_init(NULL, NULL, NULL, NULL TSRMLS_CC);
	}

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, php_http_client_pool_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_client_pool_object_handlers;

	return ov;
}

void php_http_client_pool_object_free(void *object TSRMLS_DC)
{
	php_http_client_pool_object_t *o = (php_http_client_pool_object_t *) object;

	php_http_client_pool_free(&o->pool);
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static void php_http_client_pool_object_llist2array(zval **req, zval *array TSRMLS_DC)
{
	Z_ADDREF_P(*req);
	add_next_index_zval(array, *req);
}

PHP_METHOD(HttpClientPool, __destruct)
{
	php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	if (SUCCESS != zend_parse_parameters_none()) {
		; /* we always want to clean up */
	}
	/* FIXME: move to php_http_client_pool_dtor */
	php_http_client_pool_reset(obj->pool);
}

PHP_METHOD(HttpClientPool, reset)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		obj->iterator.pos = 0;
		php_http_client_pool_reset(obj->pool);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientPool, attach)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		zval *request;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_client_class_entry)) {
			with_error_handling(EH_THROW, php_http_exception_class_entry) {
				php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				if (obj->iterator.pos > 0 && obj->iterator.pos < zend_llist_count(&obj->pool->clients.attached)) {
					php_http_error(HE_THROW, PHP_HTTP_E_CLIENT_POOL, "Cannot attach to the HttpClientPool while the iterator is active");
				} else {
					php_http_client_pool_attach(obj->pool, request);
				}
			} end_error_handling();
		}
	} end_error_handling();

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientPool, detach)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		zval *request;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_client_class_entry)) {
			with_error_handling(EH_THROW, php_http_exception_class_entry) {
				php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				obj->iterator.pos = -1;
				php_http_client_pool_detach(obj->pool, request);
			} end_error_handling();
		}
	} end_error_handling();

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientPool, send)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			with_error_handling(EH_THROW, php_http_exception_class_entry) {
				php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				php_http_client_pool_exec(obj->pool);
			} end_error_handling();
		}
	} end_error_handling();

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientPool, once)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (0 < php_http_client_pool_once(obj->pool)) {
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientPool, wait)
{
	double timeout = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|d", &timeout)) {
		struct timeval timeout_val;
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		timeout_val.tv_sec = (time_t) timeout;
		timeout_val.tv_usec = PHP_HTTP_USEC(timeout) % PHP_HTTP_MCROSEC;

		RETURN_SUCCESS(php_http_client_pool_wait(obj->pool, timeout > 0 ? &timeout_val : NULL));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientPool, valid)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_BOOL(obj->iterator.pos >= 0 && obj->iterator.pos < zend_llist_count(&obj->pool->clients.attached));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientPool, current)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->iterator.pos < zend_llist_count(&obj->pool->clients.attached)) {
			long pos = 0;
			zval **current = NULL;
			zend_llist_position lpos;

			for (	current = zend_llist_get_first_ex(&obj->pool->clients.attached, &lpos);
					current && obj->iterator.pos != pos++;
					current = zend_llist_get_next_ex(&obj->pool->clients.attached, &lpos));
			if (current) {
				RETURN_OBJECT(*current, 1);
			}
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientPool, key)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(obj->iterator.pos);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientPool, next)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		++obj->iterator.pos;
	}
}

PHP_METHOD(HttpClientPool, rewind)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		obj->iterator.pos = 0;
	}
}

PHP_METHOD(HttpClientPool, count)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG((long) zend_llist_count(&obj->pool->clients.attached));
	}
}

PHP_METHOD(HttpClientPool, getAttachedRequests)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		array_init(return_value);
		zend_llist_apply_with_argument(&obj->pool->clients.attached,
			(llist_apply_with_arg_func_t) php_http_client_pool_object_llist2array,
			return_value TSRMLS_CC);
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientPool, getFinishedRequests)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		array_init(return_value);
		zend_llist_apply_with_argument(&obj->pool->clients.finished,
				(llist_apply_with_arg_func_t) php_http_client_pool_object_llist2array,
				return_value TSRMLS_CC);
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientPool, enablePipelining)
{
	zend_bool enable = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_client_pool_setopt(obj->pool, PHP_HTTP_CLIENT_POOL_OPT_ENABLE_PIPELINING, &enable);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientPool, enableEvents)
{
	zend_bool enable = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
		php_http_client_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_client_pool_setopt(obj->pool, PHP_HTTP_CLIENT_POOL_OPT_USE_EVENTS, &enable);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_MINIT_FUNCTION(http_client_pool)
{
	PHP_HTTP_REGISTER_CLASS(http\\Client\\Pool, AbstractPool, http_client_pool, php_http_object_class_entry, 0);
	php_http_client_pool_class_entry->create_object = php_http_object_new; //php_http_client_pool_object_new;
	memcpy(&php_http_client_pool_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_client_pool_object_handlers.clone_obj = NULL;

	zend_class_implements(php_http_client_pool_class_entry TSRMLS_CC, 2, spl_ce_Countable, zend_ce_iterator);

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

