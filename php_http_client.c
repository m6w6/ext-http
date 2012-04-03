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

#include <ext/spl/spl_observer.h>

PHP_HTTP_API php_http_client_t *php_http_client_init(php_http_client_t *h, php_http_client_ops_t *ops, php_http_resource_factory_t *rf, void *init_arg TSRMLS_DC)
{
	php_http_client_t *free_h = NULL;

	if (!h) {
		free_h = h = emalloc(sizeof(*h));
	}
	memset(h, 0, sizeof(*h));

	h->ops = ops;
	if (rf) {
		h->rf = rf;
	} else if (ops->rsrc) {
		h->rf = php_http_resource_factory_init(NULL, h->ops->rsrc, h, NULL);
	}
	h->buffer = php_http_buffer_init(NULL);
	h->parser = php_http_message_parser_init(NULL TSRMLS_CC);
	h->message = php_http_message_init(NULL, 0 TSRMLS_CC);

	TSRMLS_SET_CTX(h->ts);

	if (h->ops->init) {
		if (!(h = h->ops->init(h, init_arg))) {
			php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "Could not initialize request");
			if (free_h) {
				h->ops->dtor = NULL;
				php_http_client_free(&free_h);
			}
		}
	}

	return h;
}

PHP_HTTP_API void php_http_client_dtor(php_http_client_t *h)
{
	if (h->ops->dtor) {
		h->ops->dtor(h);
	}

	php_http_resource_factory_free(&h->rf);

	php_http_message_parser_free(&h->parser);
	php_http_message_free(&h->message);
	php_http_buffer_free(&h->buffer);
}

PHP_HTTP_API void php_http_client_free(php_http_client_t **h)
{
	if (*h) {
		php_http_client_dtor(*h);
		efree(*h);
		*h = NULL;
	}
}

PHP_HTTP_API php_http_client_t *php_http_client_copy(php_http_client_t *from, php_http_client_t *to)
{
	if (!from->ops->copy) {
		return NULL;
	} else {
		TSRMLS_FETCH_FROM_CTX(from->ts);

		if (!to) {
			to = ecalloc(1, sizeof(*to));
		}

		to->ops = from->ops;
		if (from->rf) {
			php_http_resource_factory_addref(from->rf);
			to->rf = from->rf;
		} else if (to->ops->rsrc){
			to->rf = php_http_resource_factory_init(NULL, to->ops->rsrc, to, NULL);
		}
		to->buffer = php_http_buffer_init(NULL);
		to->parser = php_http_message_parser_init(NULL TSRMLS_CC);
		to->message = php_http_message_init(NULL, 0 TSRMLS_CC);

		TSRMLS_SET_CTX(to->ts);

		return to->ops->copy(from, to);
	}
}

PHP_HTTP_API STATUS php_http_client_exec(php_http_client_t *h, php_http_message_t *msg)
{
	if (h->ops->exec) {
		return h->ops->exec(h, msg);
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_reset(php_http_client_t *h)
{
	if (h->ops->reset) {
		return h->ops->reset(h);
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_setopt(php_http_client_t *h, php_http_client_setopt_opt_t opt, void *arg)
{
	if (h->ops->setopt) {
		return h->ops->setopt(h, opt, arg);
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_getopt(php_http_client_t *h, php_http_client_getopt_opt_t opt, void *arg)
{
	if (h->ops->getopt) {
		return h->ops->getopt(h, opt, arg);
	}
	return FAILURE;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpClient, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpClient, method, 0)
#define PHP_HTTP_CLIENT_ME(method, visibility)	PHP_ME(HttpClient, method, PHP_HTTP_ARGS(HttpClient, method), visibility)
#define PHP_HTTP_CLIENT_ALIAS(method, func)	PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpClient, method))
#define PHP_HTTP_CLIENT_MALIAS(me, al, vis)	ZEND_FENTRY(me, ZEND_MN(HttpClient_##al), PHP_HTTP_ARGS(HttpClient, al), vis)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_ARR(options, 1, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getOptions);
PHP_HTTP_BEGIN_ARGS(setOptions, 0)
	PHP_HTTP_ARG_ARR(options, 1, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getSslOptions);
PHP_HTTP_BEGIN_ARGS(setSslOptions, 0)
	PHP_HTTP_ARG_ARR(ssl_options, 1, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addSslOptions, 0)
	PHP_HTTP_ARG_ARR(ssl_options, 1, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getCookies);
PHP_HTTP_BEGIN_ARGS(setCookies, 0)
	PHP_HTTP_ARG_VAL(cookies, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addCookies, 1)
	PHP_HTTP_ARG_VAL(cookies, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(enableCookies);
PHP_HTTP_BEGIN_ARGS(resetCookies, 0)
	PHP_HTTP_ARG_VAL(session_only, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(flushCookies);

PHP_HTTP_EMPTY_ARGS(getResponseMessageClass);
PHP_HTTP_BEGIN_ARGS(setResponseMessageClass, 1)
	PHP_HTTP_ARG_VAL(message_class_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseMessage);
PHP_HTTP_EMPTY_ARGS(getRequestMessage);
PHP_HTTP_EMPTY_ARGS(getHistory);
PHP_HTTP_EMPTY_ARGS(clearHistory);

PHP_HTTP_BEGIN_ARGS(setRequest, 1)
	PHP_HTTP_ARG_OBJ(http\\Client\\Request, request, 1)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(getRequest);

PHP_HTTP_EMPTY_ARGS(getObservers);
PHP_HTTP_BEGIN_ARGS(attach, 1)
	PHP_HTTP_ARG_OBJ(SplObserver, observer, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(detach, 1)
	PHP_HTTP_ARG_OBJ(SplObserver, observer, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(notify);
PHP_HTTP_EMPTY_ARGS(getProgress);
PHP_HTTP_BEGIN_ARGS(getTransferInfo, 0)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;


static zend_class_entry *php_http_client_class_entry;

zend_class_entry *php_http_client_get_class_entry(void)
{
	return php_http_client_class_entry;
}

static zend_function_entry php_http_client_method_entry[] = {
	PHP_HTTP_CLIENT_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_CLIENT_ME(getObservers, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(notify, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(attach, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getProgress, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getTransferInfo, ZEND_ACC_PUBLIC)

	PHP_HTTP_CLIENT_ME(setOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(setSslOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getSslOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(addSslOptions, ZEND_ACC_PUBLIC)

	PHP_HTTP_CLIENT_ME(addCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(setCookies, ZEND_ACC_PUBLIC)

	PHP_HTTP_CLIENT_ME(enableCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(resetCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(flushCookies, ZEND_ACC_PUBLIC)

	PHP_HTTP_CLIENT_ME(setRequest, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getRequest, ZEND_ACC_PUBLIC)

	PHP_HTTP_CLIENT_ME(getResponseMessage, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getRequestMessage, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(getHistory, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(clearHistory, ZEND_ACC_PUBLIC)

	PHP_HTTP_CLIENT_ME(getResponseMessageClass, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_ME(setResponseMessageClass, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

static zend_object_handlers php_http_client_object_handlers;

zend_object_handlers *php_http_client_get_object_handlers(void)
{
	return &php_http_client_object_handlers;
}

static php_http_client_ops_t php_http_client_user_ops = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	(php_http_new_t) php_http_client_object_new_ex,
	php_http_client_get_class_entry
};

zend_object_value php_http_client_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_client_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_client_object_new_ex(zend_class_entry *ce, php_http_client_t *r, php_http_client_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_client_object_t *o;

	o = ecalloc(1, sizeof(*o));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	ov.handle = zend_objects_store_put(o, NULL, php_http_client_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_client_object_handlers;

	if (!(o->client = r)) {
		o->client = php_http_client_init(NULL, &php_http_client_user_ops, NULL, &ov TSRMLS_CC);
	}

	if (ptr) {
		*ptr = o;
	}

	return ov;
}

zend_object_value php_http_client_object_clone(zval *this_ptr TSRMLS_DC)
{
	zend_object_value new_ov;
	php_http_client_object_t *new_obj, *old_obj = zend_object_store_get_object(this_ptr TSRMLS_CC);

	new_ov = php_http_client_object_new_ex(old_obj->zo.ce, php_http_client_copy(old_obj->client, NULL), &new_obj TSRMLS_CC);
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);

	return new_ov;
}

void php_http_client_object_free(void *object TSRMLS_DC)
{
	php_http_client_object_t *o = (php_http_client_object_t *) object;

	php_http_client_free(&o->client);
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static inline zend_object_value php_http_client_object_message(zval *this_ptr, php_http_message_t *msg TSRMLS_DC)
{
	zend_object_value ov;
	zval *zcn = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("responseMessageClass"), 0 TSRMLS_CC);
	zend_class_entry *class_entry;

	if (Z_STRLEN_P(zcn)
	&&	(class_entry = zend_fetch_class(Z_STRVAL_P(zcn), Z_STRLEN_P(zcn), 0 TSRMLS_CC))
	&&	(SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_message_object_new_ex, php_http_client_response_get_class_entry(), msg, NULL TSRMLS_CC))) {
		return ov;
	} else {
		return php_http_message_object_new_ex(php_http_client_response_get_class_entry(), msg, NULL TSRMLS_CC);
	}
}

STATUS php_http_client_object_handle_request(zval *zclient, zval **zreq TSRMLS_DC)
{
	php_http_client_object_t *obj = zend_object_store_get_object(zclient TSRMLS_CC);
	php_http_client_progress_t *progress;
	zval *zoptions;

	/* do we have a valid request? */
	if (*zreq) {
		/* remember the request */
		zend_update_property(php_http_client_class_entry, zclient, ZEND_STRL("request"), *zreq TSRMLS_CC);
	} else {
		/* maybe a request is already set */
		*zreq = zend_read_property(php_http_client_class_entry, zclient, ZEND_STRL("request"), 0 TSRMLS_CC);

		if (Z_TYPE_PP(zreq) != IS_OBJECT || !instanceof_function(Z_OBJCE_PP(zreq), php_http_client_request_get_class_entry() TSRMLS_CC)) {
			php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "The client does not have a valid request set");
			return FAILURE;
		}
	}

	/* reset request handle */
	php_http_client_reset(obj->client);

	/* reset transfer info */
	zend_update_property_null(php_http_client_class_entry, zclient, ZEND_STRL("info") TSRMLS_CC);

	/* set client options */
	zoptions = zend_read_property(php_http_client_class_entry, zclient, ZEND_STRL("options"), 0 TSRMLS_CC);
	php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_SETTINGS, Z_ARRVAL_P(zoptions));
	/* set request options */
	zoptions = zend_read_property(php_http_client_request_get_class_entry(), *zreq, ZEND_STRL("options"), 0 TSRMLS_CC);
	php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_SETTINGS, Z_ARRVAL_P(zoptions));

	/* set progress callback */
	if (SUCCESS == php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, &progress)) {
		if (!progress->callback) {
			php_http_client_progress_callback_t *callback = emalloc(sizeof(*callback));

			callback->type = PHP_HTTP_CLIENT_PROGRESS_CALLBACK_USER;
			callback->pass_state = 0;
			MAKE_STD_ZVAL(callback->func.user);
			array_init(callback->func.user);
			Z_ADDREF_P(zclient);
			add_next_index_zval(callback->func.user, zclient);
			add_next_index_stringl(callback->func.user, ZEND_STRL("notify"), 1);

			php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_PROGRESS_CALLBACK, callback);
		}
		progress->state.info = "start";
		php_http_client_progress_notify(progress TSRMLS_CC);
		progress->state.started = 1;
	}

	return SUCCESS;
}


STATUS php_http_client_object_handle_response(zval *zclient TSRMLS_DC)
{
	php_http_client_object_t *obj = zend_object_store_get_object(zclient TSRMLS_CC);
	php_http_client_progress_t *progress;
	php_http_message_t *msg;
	zval *info;

	/* always fetch info */
	MAKE_STD_ZVAL(info);
	array_init(info);
	php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_TRANSFER_INFO, Z_ARRVAL_P(info));
	zend_update_property(php_http_client_class_entry, zclient, ZEND_STRL("transferInfo"), info TSRMLS_CC);
	zval_ptr_dtor(&info);

	if ((msg = obj->client->message)) {
		/* update history */
		if (i_zend_is_true(zend_read_property(php_http_client_class_entry, zclient, ZEND_STRL("recordHistory"), 0 TSRMLS_CC))) {
			zval *new_hist, *old_hist = zend_read_property(php_http_client_class_entry, zclient, ZEND_STRL("history"), 0 TSRMLS_CC);
			zend_object_value ov = php_http_client_object_message(zclient, php_http_message_copy(msg, NULL) TSRMLS_CC);

			MAKE_STD_ZVAL(new_hist);
			ZVAL_OBJVAL(new_hist, ov, 0);

			if (Z_TYPE_P(old_hist) == IS_OBJECT) {
				php_http_message_object_prepend(new_hist, old_hist, 1 TSRMLS_CC);
			}

			zend_update_property(php_http_client_class_entry, zclient, ZEND_STRL("history"), new_hist TSRMLS_CC);
			zval_ptr_dtor(&new_hist);
		}

		/* update response info */
		if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, msg)) {
			zval *message;

			MAKE_STD_ZVAL(message);
			ZVAL_OBJVAL(message, php_http_client_object_message(zclient, msg TSRMLS_CC), 0);
			zend_update_property(php_http_client_class_entry, zclient, ZEND_STRL("responseMessage"), message TSRMLS_CC);
			zval_ptr_dtor(&message);

			obj->client->message = php_http_message_init(NULL, 0 TSRMLS_CC);
			msg = msg->parent;
		} else {
			zend_update_property_null(php_http_client_class_entry, zclient, ZEND_STRL("responseMessage") TSRMLS_CC);
		}
	} else {
		zend_update_property_null(php_http_client_class_entry, zclient, ZEND_STRL("responseMessage") TSRMLS_CC);
	}

	/* there might be a 100-Continue response in between */
	while (msg && !PHP_HTTP_MESSAGE_TYPE(REQUEST, msg)) {
		msg = msg->parent;
	}

	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, msg)) {
		zval *message;

		/* update the actual request message */
		MAKE_STD_ZVAL(message);
		ZVAL_OBJVAL(message, php_http_message_object_new_ex(php_http_message_get_class_entry(), php_http_message_copy_ex(msg, NULL, 0), NULL TSRMLS_CC), 0);
		zend_update_property(php_http_client_class_entry, zclient, ZEND_STRL("requestMessage"), message TSRMLS_CC);
		zval_ptr_dtor(&message);
	}

	if (SUCCESS == php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, &progress)) {
		progress->state.info = "finished";
		progress->state.finished = 1;
		php_http_client_progress_notify(progress TSRMLS_CC);
	}
	php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_PROGRESS_CALLBACK, NULL);

	return SUCCESS;
}

void php_http_client_options_set(zval *this_ptr, zval *opts TSRMLS_DC)
{
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	HashPosition pos;
	zval *new_opts;
	zend_class_entry *this_ce = Z_OBJCE_P(getThis());
	zend_bool is_client = instanceof_function(this_ce, php_http_client_class_entry TSRMLS_CC);

	MAKE_STD_ZVAL(new_opts);
	array_init(new_opts);

	if (!opts || !zend_hash_num_elements(Z_ARRVAL_P(opts))) {
		zend_update_property(this_ce, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
		zval_ptr_dtor(&new_opts);
	} else {
		zval *old_opts, *add_opts, **opt;

		MAKE_STD_ZVAL(add_opts);
		array_init(add_opts);
		/* some options need extra attention -- thus cannot use array_merge() directly */
		FOREACH_KEYVAL(pos, opts, key, opt) {
			if (key.type == HASH_KEY_IS_STRING) {
#define KEYMATCH(k, s) ((sizeof(s)==k.len) && !strcasecmp(k.str, s))
				if (Z_TYPE_PP(opt) == IS_ARRAY && (KEYMATCH(key, "ssl") || KEYMATCH(key, "cookies"))) {
					php_http_client_options_set_subr(getThis(), key.str, key.len, *opt, 0 TSRMLS_CC);
				} else if (is_client && (KEYMATCH(key, "recordHistory") || KEYMATCH(key, "responseMessageClass"))) {
					zend_update_property(this_ce, getThis(), key.str, key.len-1, *opt TSRMLS_CC);
				} else if (Z_TYPE_PP(opt) == IS_NULL) {
					old_opts = zend_read_property(this_ce, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
					if (Z_TYPE_P(old_opts) == IS_ARRAY) {
						zend_symtable_del(Z_ARRVAL_P(old_opts), key.str, key.len);
					}
				} else {
					Z_ADDREF_P(*opt);
					add_assoc_zval_ex(add_opts, key.str, key.len, *opt);
				}
			}
		}

		old_opts = zend_read_property(this_ce, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		if (Z_TYPE_P(old_opts) == IS_ARRAY) {
			array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL_P(new_opts));
		}
		array_join(Z_ARRVAL_P(add_opts), Z_ARRVAL_P(new_opts), 0, 0);
		zend_update_property(this_ce, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
		zval_ptr_dtor(&new_opts);
		zval_ptr_dtor(&add_opts);
	}
}

void php_http_client_options_set_subr(zval *this_ptr, char *key, size_t len, zval *opts, int overwrite TSRMLS_DC)
{
	if (overwrite || (opts && zend_hash_num_elements(Z_ARRVAL_P(opts)))) {
		zend_class_entry *this_ce = Z_OBJCE_P(getThis());
		zval *old_opts, *new_opts, **entry = NULL;

		MAKE_STD_ZVAL(new_opts);
		array_init(new_opts);
		old_opts = zend_read_property(this_ce, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		if (Z_TYPE_P(old_opts) == IS_ARRAY) {
			array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL_P(new_opts));
		}

		if (overwrite) {
			if (opts && zend_hash_num_elements(Z_ARRVAL_P(opts))) {
				Z_ADDREF_P(opts);
				zend_symtable_update(Z_ARRVAL_P(new_opts), key, len, (void *) &opts, sizeof(zval *), NULL);
			} else {
				zend_symtable_del(Z_ARRVAL_P(new_opts), key, len);
			}
		} else if (opts && zend_hash_num_elements(Z_ARRVAL_P(opts))) {
			if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(new_opts), key, len, (void *) &entry)) {
				array_join(Z_ARRVAL_P(opts), Z_ARRVAL_PP(entry), 0, 0);
			} else {
				Z_ADDREF_P(opts);
				zend_symtable_update(Z_ARRVAL_P(new_opts), key, len, (void *) &opts, sizeof(zval *), NULL);
			}
		}

		zend_update_property(this_ce, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
		zval_ptr_dtor(&new_opts);
	}
}

void php_http_client_options_get_subr(zval *this_ptr, char *key, size_t len, zval *return_value TSRMLS_DC)
{
	zend_class_entry *this_ce = Z_OBJCE_P(getThis());
	zval **options, *opts = zend_read_property(this_ce, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);

	if ((Z_TYPE_P(opts) == IS_ARRAY) && (SUCCESS == zend_symtable_find(Z_ARRVAL_P(opts), key, len, (void *) &options))) {
		RETVAL_ZVAL(*options, 1, 0);
	}
}

PHP_METHOD(HttpClient, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		zval *os, *opts = NULL;

		MAKE_STD_ZVAL(os);
		object_init_ex(os, spl_ce_SplObjectStorage);
		zend_update_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), os TSRMLS_CC);
		zval_ptr_dtor(&os);

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
			php_http_client_options_set(getThis(), opts TSRMLS_CC);
		}

	} end_error_handling();
}

PHP_METHOD(HttpClient, getObservers)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			RETVAL_PROP(php_http_client_class_entry, "observers");
		}
	} end_error_handling();
}

static int notify(zend_object_iterator *iter, void *puser TSRMLS_DC)
{
	zval **observer = NULL;

	iter->funcs->get_current_data(iter, &observer TSRMLS_CC);
	if (observer) {
		zval *retval;

		zend_call_method_with_1_params(observer, NULL, NULL, "update", &retval, puser);
		zval_ptr_dtor(&retval);
		return SUCCESS;
	}
	return FAILURE;
}

PHP_METHOD(HttpClient, notify)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);

		if (Z_TYPE_P(observers) == IS_OBJECT) {
			Z_ADDREF_P(getThis());
			spl_iterator_apply(observers, notify, getThis() TSRMLS_CC);
			zval_ptr_dtor(&getThis());
		}
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, attach)
{
	zval *observer;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &observer, spl_ce_SplObserver)) {
		zval *retval, *observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);
		zend_call_method_with_1_params(&observers, NULL, NULL, "attach", &retval, observer);
		zval_ptr_dtor(&retval);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, detach)
{
	zval *observer;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &observer, spl_ce_SplObserver)) {
		zval *retval, *observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);
		zend_call_method_with_1_params(&observers, NULL, NULL, "detach", &retval, observer);
		zval_ptr_dtor(&retval);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, getProgress)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_client_progress_t *progress = NULL;

		if (SUCCESS == php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, &progress)) {
			object_init(return_value);
			add_property_bool(return_value, "started", progress->state.started);
			add_property_bool(return_value, "finished", progress->state.finished);
			add_property_string(return_value, "info", STR_PTR(progress->state.info), 1);
			add_property_double(return_value, "dltotal", progress->state.dl.total);
			add_property_double(return_value, "dlnow", progress->state.dl.now);
			add_property_double(return_value, "ultotal", progress->state.ul.total);
			add_property_double(return_value, "ulnow", progress->state.ul.now);
		}
	}
}

PHP_METHOD(HttpClient, getTransferInfo)
{
	char *info_name = NULL;
	int info_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &info_name, &info_len)) {
		zval **infop, *temp = NULL, *info = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("transferInfo"), 0 TSRMLS_CC);

		/* request completed? */
		if (Z_TYPE_P(info) != IS_ARRAY) {
			php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			MAKE_STD_ZVAL(temp);
			array_init(temp);
			php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_TRANSFER_INFO, Z_ARRVAL_P(temp));
			info = temp;
		}

		if (info_len && info_name) {
			if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(info), php_http_pretty_key(info_name, info_len, 0, 0), info_len + 1, (void *) &infop)) {
				RETVAL_ZVAL(*infop, 1, 0);
			} else {
				php_http_error(HE_NOTICE, PHP_HTTP_E_INVALID_PARAM, "Could not find transfer info named %s", info_name);
				RETVAL_FALSE;
			}
		} else {
			RETVAL_ZVAL(info, 1, 0);
		}

		if (temp) {
			zval_ptr_dtor(&temp);
		}
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClient, setOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set(getThis(), opts TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

PHP_METHOD(HttpClient, getOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_client_class_entry, "options");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClient, setSslOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("ssl"), opts, 1 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

PHP_METHOD(HttpClient, addSslOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("ssl"), opts, 0 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

PHP_METHOD(HttpClient, getSslOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRS("ssl"), return_value TSRMLS_CC);
	}
}

PHP_METHOD(HttpClient, setCookies)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("cookies"), opts, 1 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

PHP_METHOD(HttpClient, addCookies)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("ssl"), opts, 0 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

PHP_METHOD(HttpClient, getCookies)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRS("cookies"), return_value TSRMLS_CC);
	}
}

PHP_METHOD(HttpClient, enableCookies)
{
	if (SUCCESS == zend_parse_parameters_none()){
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_COOKIES_ENABLE, NULL);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, resetCookies)
{
	zend_bool session_only = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &session_only)) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		if (session_only) {
			php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_COOKIES_RESET_SESSION, NULL);
		} else {
			php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_COOKIES_RESET, NULL);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, flushCookies)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_COOKIES_FLUSH, NULL);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, getResponseMessage)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *message = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);

			if (Z_TYPE_P(message) == IS_OBJECT) {
				RETVAL_OBJECT(message, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpClient does not contain a response message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpClient, getRequestMessage)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *message = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("requestMessage"), 0 TSRMLS_CC);

			if (Z_TYPE_P(message) == IS_OBJECT) {
				RETVAL_OBJECT(message, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpClient does not contain a request message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpClient, getHistory)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *hist = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("history"), 0 TSRMLS_CC);

			if (Z_TYPE_P(hist) == IS_OBJECT) {
				RETVAL_OBJECT(hist, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "The history is empty");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpClient, clearHistory)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zend_update_property_null(php_http_client_class_entry, getThis(), ZEND_STRL("history") TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, getResponseMessageClass)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_client_class_entry, "responseMessageClass");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClient, setResponseMessageClass)
{
	char *cn;
	int cl;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &cn, &cl)) {
		zend_update_property_stringl(php_http_client_class_entry, getThis(), ZEND_STRL("responseMessageClass"), cn, cl TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, setRequest)
{
	zval *zreq = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &zreq, php_http_client_request_get_class_entry())) {
		zend_update_property(php_http_client_class_entry, getThis(), ZEND_STRL("request"), zreq TSRMLS_CC);
	}
	RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClient, getRequest)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_client_class_entry, "request");
	}
}

PHP_METHOD(HttpClient, send)
{
	zval *zreq = NULL;

	RETVAL_FALSE;

	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O!", &zreq, php_http_client_request_get_class_entry())) {
			if (SUCCESS == php_http_client_object_handle_request(getThis(), &zreq TSRMLS_CC)) {
				php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
				php_http_message_object_t *req = zend_object_store_get_object(zreq TSRMLS_CC);

				php_http_client_exec(obj->client, req->message);

				if (SUCCESS == php_http_client_object_handle_response(getThis() TSRMLS_CC)) {
					RETVAL_PROP(php_http_client_class_entry, "responseMessage");
				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "Failed to handle response");
				}
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "Failed to handle request");
			}
		}
	} end_error_handling();
}

PHP_MINIT_FUNCTION(http_client)
{
	PHP_HTTP_REGISTER_CLASS(http\\Client, AbstractClient, http_client, php_http_object_get_class_entry(), ZEND_ACC_ABSTRACT);
	php_http_client_class_entry->create_object = php_http_client_object_new;
	memcpy(&php_http_client_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_client_object_handlers.clone_obj = php_http_client_object_clone;

	zend_class_implements(php_http_client_class_entry TSRMLS_CC, 2, spl_ce_SplSubject, php_http_client_interface_get_class_entry());

	zend_declare_property_string(php_http_client_class_entry, ZEND_STRL("responseMessageClass"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("observers"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("transferInfo"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("responseMessage"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("requestMessage"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("history"), ZEND_ACC_PRIVATE TSRMLS_CC);

	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("options"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("request"), ZEND_ACC_PROTECTED TSRMLS_CC);

	zend_declare_property_bool(php_http_client_class_entry, ZEND_STRL("recordHistory"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

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

