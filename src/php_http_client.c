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
#include "php_http_client.h"

#include "ext/spl/spl_observer.h"

/*
 * array of name => php_http_client_driver_t*
 */
static HashTable php_http_client_drivers;

static void php_http_client_driver_hash_dtor(zval *pData)
{
	pefree(Z_PTR_P(pData), 1);
}

ZEND_RESULT_CODE php_http_client_driver_add(php_http_client_driver_t *driver)
{
	return zend_hash_add_mem(&php_http_client_drivers, driver->driver_name, (void *) driver, sizeof(php_http_client_driver_t))
			? SUCCESS : FAILURE;
}

php_http_client_driver_t *php_http_client_driver_get(zend_string *name)
{
	zval *ztmp;
	php_http_client_driver_t *tmp;

	if (name && (tmp = zend_hash_find_ptr(&php_http_client_drivers, name))) {
		return tmp;
	}
	if ((ztmp = zend_hash_get_current_data(&php_http_client_drivers))) {
		return Z_PTR_P(ztmp);
	}
	return NULL;
}

static int apply_driver_list(zval *p, void *arg)
{
	php_http_client_driver_t *d = Z_PTR_P(p);
	zval zname;

	ZVAL_STR_COPY(&zname, d->driver_name);

	zend_hash_next_index_insert(arg, &zname);
	return ZEND_HASH_APPLY_KEEP;
}

void php_http_client_driver_list(HashTable *ht)
{
	zend_hash_apply_with_argument(&php_http_client_drivers, apply_driver_list, ht);
}

static zend_class_entry *php_http_client_class_entry;
zend_class_entry *php_http_client_get_class_entry(void)
{
	return php_http_client_class_entry;
}

void php_http_client_options_set_subr(zval *instance, char *key, size_t len, zval *opts, int overwrite)
{
	if (overwrite || (opts && zend_hash_num_elements(Z_ARRVAL_P(opts)))) {
		zend_class_entry *this_ce = Z_OBJCE_P(instance);
		zval old_opts_tmp, *old_opts, new_opts, *entry = NULL;

		array_init(&new_opts);
		old_opts = zend_read_property(this_ce, instance, ZEND_STRL("options"), 0, &old_opts_tmp);

		if (Z_TYPE_P(old_opts) == IS_ARRAY) {
			array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL(new_opts));
		}

		if (overwrite) {
			if (opts && zend_hash_num_elements(Z_ARRVAL_P(opts))) {
				Z_ADDREF_P(opts);
				zend_symtable_str_update(Z_ARRVAL(new_opts), key, len, opts);
			} else {
				zend_symtable_str_del(Z_ARRVAL(new_opts), key, len);
			}
		} else if (opts && zend_hash_num_elements(Z_ARRVAL_P(opts))) {
			if ((entry = zend_symtable_str_find(Z_ARRVAL(new_opts), key, len))) {
				SEPARATE_ZVAL(entry);
				array_join(Z_ARRVAL_P(opts), Z_ARRVAL_P(entry), 0, 0);
			} else {
				Z_ADDREF_P(opts);
				zend_symtable_str_update(Z_ARRVAL(new_opts), key, len, opts);
			}
		}

		zend_update_property(this_ce, instance, ZEND_STRL("options"), &new_opts);
		zval_ptr_dtor(&new_opts);
	}
}

void php_http_client_options_set(zval *instance, zval *opts)
{
	php_http_arrkey_t key;
	zval new_opts;
	zend_class_entry *this_ce = Z_OBJCE_P(instance);
	zend_bool is_client = instanceof_function(this_ce, php_http_client_class_entry);

	array_init(&new_opts);

	if (!opts || !zend_hash_num_elements(Z_ARRVAL_P(opts))) {
		zend_update_property(this_ce, instance, ZEND_STRL("options"), &new_opts);
		zval_ptr_dtor(&new_opts);
	} else {
		zval old_opts_tmp, *old_opts, add_opts, *opt;

		array_init(&add_opts);
		/* some options need extra attention -- thus cannot use array_merge() directly */
		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(opts), key.h, key.key, opt)
		{
			if (key.key) {
				if (Z_TYPE_P(opt) == IS_ARRAY && (zend_string_equals_literal(key.key, "ssl") || zend_string_equals_literal(key.key, "cookies"))) {
					php_http_client_options_set_subr(instance, key.key->val, key.key->len, opt, 0);
				} else if (is_client && (zend_string_equals_literal(key.key, "recordHistory") || zend_string_equals_literal(key.key, "responseMessageClass"))) {
					zend_update_property(this_ce, instance, key.key->val, key.key->len, opt);
				} else if (Z_TYPE_P(opt) == IS_NULL) {
					old_opts = zend_read_property(this_ce, instance, ZEND_STRL("options"), 0, &old_opts_tmp);
					if (Z_TYPE_P(old_opts) == IS_ARRAY) {
						zend_symtable_del(Z_ARRVAL_P(old_opts), key.key);
					}
				} else {
					Z_TRY_ADDREF_P(opt);
					add_assoc_zval_ex(&add_opts, key.key->val, key.key->len, opt);
				}
			}
		}
		ZEND_HASH_FOREACH_END();

		old_opts = zend_read_property(this_ce, instance, ZEND_STRL("options"), 0, &old_opts_tmp);
		if (Z_TYPE_P(old_opts) == IS_ARRAY) {
			array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL(new_opts));
		}
		array_join(Z_ARRVAL(add_opts), Z_ARRVAL(new_opts), 0, 0);
		zend_update_property(this_ce, instance, ZEND_STRL("options"), &new_opts);
		zval_ptr_dtor(&new_opts);
		zval_ptr_dtor(&add_opts);
	}
}

void php_http_client_options_get_subr(zval *instance, char *key, size_t len, zval *return_value)
{
	zend_class_entry *this_ce = Z_OBJCE_P(instance);
	zval *options, opts_tmp, *opts = zend_read_property(this_ce, instance, ZEND_STRL("options"), 0, &opts_tmp);

	if ((Z_TYPE_P(opts) == IS_ARRAY) && (options = zend_symtable_str_find(Z_ARRVAL_P(opts), key, len))) {
		RETVAL_ZVAL(options, 1, 0);
	}
}

static void queue_dtor(void *enqueued)
{
	php_http_client_enqueue_t *e = enqueued;

	if (e->dtor) {
		e->dtor(e);
	}
}

php_http_client_t *php_http_client_init(php_http_client_t *h, php_http_client_ops_t *ops, php_resource_factory_t *rf, void *init_arg)
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
		h->rf = php_resource_factory_init(NULL, h->ops->rsrc, h, NULL);
	}
	zend_llist_init(&h->requests, sizeof(php_http_client_enqueue_t), queue_dtor, 0);
	zend_llist_init(&h->responses, sizeof(void *), NULL, 0);

	if (h->ops->init) {
		if (!(h = h->ops->init(h, init_arg))) {
			php_error_docref(NULL, E_WARNING, "Could not initialize client");
			PTR_FREE(free_h);
		}
	}

	return h;
}

php_http_client_t *php_http_client_copy(php_http_client_t *from, php_http_client_t *to)
{
	if (from->ops->copy) {
		return from->ops->copy(from, to);
	}

	return NULL;
}

void php_http_client_dtor(php_http_client_t *h)
{
	php_http_client_reset(h);

	if (h->ops->dtor) {
		h->ops->dtor(h);
	}
	h->callback.debug.func = NULL;
	h->callback.debug.arg = NULL;

	php_resource_factory_free(&h->rf);
}

void php_http_client_free(php_http_client_t **h) {
	if (*h) {
		php_http_client_dtor(*h);
		efree(*h);
		*h = NULL;
	}
}

ZEND_RESULT_CODE php_http_client_enqueue(php_http_client_t *h, php_http_client_enqueue_t *enqueue)
{
	if (h->ops->enqueue) {
		if (php_http_client_enqueued(h, enqueue->request, NULL)) {
			php_error_docref(NULL, E_WARNING, "Failed to enqueue request; request already in queue");
			return FAILURE;
		}
		return h->ops->enqueue(h, enqueue);
	}

	return FAILURE;
}

ZEND_RESULT_CODE php_http_client_dequeue(php_http_client_t *h, php_http_message_t *request)
{
	if (h->ops->dequeue) {
		php_http_client_enqueue_t *enqueue = php_http_client_enqueued(h, request, NULL);

		if (!enqueue) {
			php_error_docref(NULL, E_WARNING, "Failed to dequeue request; request not in queue");
			return FAILURE;
		}
		return h->ops->dequeue(h, enqueue);
	}
	return FAILURE;
}

php_http_client_enqueue_t *php_http_client_enqueued(php_http_client_t *h, void *compare_arg, php_http_client_enqueue_cmp_func_t compare_func)
{
	zend_llist_element *el = NULL;

	if (compare_func) {
		for (el = h->requests.head; el; el = el->next) {
			if (compare_func((php_http_client_enqueue_t *) el->data, compare_arg)) {
				break;
			}
		}
	} else {
		for (el = h->requests.head; el; el = el->next) {
			if (((php_http_client_enqueue_t *) el->data)->request == compare_arg) {
				break;
			}
		}
	}
	return el ? (php_http_client_enqueue_t *) el->data : NULL;
}

ZEND_RESULT_CODE php_http_client_wait(php_http_client_t *h, struct timeval *custom_timeout)
{
	if (h->ops->wait) {
		return h->ops->wait(h, custom_timeout);
	}

	return FAILURE;
}

int php_http_client_once(php_http_client_t *h)
{
	if (h->ops->once) {
		return h->ops->once(h);
	}

	return FAILURE;
}

ZEND_RESULT_CODE php_http_client_exec(php_http_client_t *h)
{
	if (h->ops->exec) {
		return h->ops->exec(h);
	}

	return FAILURE;
}

void php_http_client_reset(php_http_client_t *h)
{
	if (h->ops->reset) {
		h->ops->reset(h);
	}

	zend_llist_clean(&h->requests);
	zend_llist_clean(&h->responses);
}

ZEND_RESULT_CODE php_http_client_setopt(php_http_client_t *h, php_http_client_setopt_opt_t opt, void *arg)
{
	if (h->ops->setopt) {
		return h->ops->setopt(h, opt, arg);
	}

	return FAILURE;
}

ZEND_RESULT_CODE php_http_client_getopt(php_http_client_t *h, php_http_client_getopt_opt_t opt, void *arg, void *res_ptr)
{
	if (h->ops->getopt) {
		return h->ops->getopt(h, opt, arg, res_ptr);
	}
	return FAILURE;
}

static zend_object_handlers php_http_client_object_handlers;

void php_http_client_object_free(zend_object *object)
{
	php_http_client_object_t *o = PHP_HTTP_OBJ(object, NULL);

	PTR_FREE(o->gc);

	php_http_client_free(&o->client);
	if (o->debug.fci.size > 0) {
		zend_fcall_info_args_clear(&o->debug.fci, 1);
		zval_ptr_dtor(&o->debug.fci.function_name);
		o->debug.fci.size = 0;
	}
	php_http_object_method_dtor(&o->notify);
	php_http_object_method_free(&o->update);
	zend_object_std_dtor(object);
}

php_http_client_object_t *php_http_client_object_new_ex(zend_class_entry *ce, php_http_client_t *client)
{
	php_http_client_object_t *o;

	o = ecalloc(1, sizeof(*o) + zend_object_properties_size(ce));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	o->client = client;

	o->zo.handlers = &php_http_client_object_handlers;

	return o;
}

zend_object *php_http_client_object_new(zend_class_entry *ce)
{
	return &php_http_client_object_new_ex(ce, NULL)->zo;
}

static HashTable *php_http_client_object_get_gc(zval *object, zval **table, int *n)
{
	php_http_client_object_t *obj = PHP_HTTP_OBJ(NULL, object);
	zend_llist_element *el = NULL;
	HashTable *props = Z_OBJPROP_P(object);
	uint32_t count = zend_hash_num_elements(props) + zend_llist_count(&obj->client->responses) + zend_llist_count(&obj->client->requests) + 2;
	zval *val;

	*n = 0;
	*table = obj->gc = erealloc(obj->gc, sizeof(zval) * count);

#if PHP_HTTP_HAVE_LIBCURL
	if (obj->client->ops == php_http_client_curl_get_ops()) {
		php_http_client_curl_t *curl = obj->client->ctx;

		if (curl->ev_ops == php_http_client_curl_user_ops_get()) {
			php_http_client_curl_user_context_t *ctx = curl->ev_ctx;

			ZVAL_COPY_VALUE(&obj->gc[(*n)++], &ctx->user);
		}
	}
#endif

	if (obj->debug.fci.size > 0) {
		ZVAL_COPY_VALUE(&obj->gc[(*n)++], &obj->debug.fci.function_name);
	}

	for (el = obj->client->responses.head; el; el = el->next) {
		php_http_message_object_t *response_obj = *(php_http_message_object_t **) el->data;
		ZVAL_OBJ(&obj->gc[(*n)++], &response_obj->zo);
	}

	for (el = obj->client->requests.head; el; el = el->next) {
		php_http_client_enqueue_t *q = (php_http_client_enqueue_t *) el->data;
		php_http_message_object_t *request_obj = q->opaque; /* FIXME */
		ZVAL_OBJ(&obj->gc[(*n)++], &request_obj->zo);
	}

	ZEND_HASH_FOREACH_VAL(props, val)
	{
		ZVAL_COPY_VALUE(&obj->gc[(*n)++], val);
	}
	ZEND_HASH_FOREACH_END();

	return NULL;
}

static void handle_history(zval *zclient, php_http_message_t *request, php_http_message_t *response)
{
	zval new_hist, old_hist_tmp, *old_hist = zend_read_property(php_http_client_class_entry, zclient, ZEND_STRL("history"), 0, &old_hist_tmp);
	php_http_message_t *req_copy = php_http_message_copy(request, NULL);
	php_http_message_t *res_copy = php_http_message_copy(response, NULL);
	php_http_message_t *zipped = php_http_message_zip(res_copy, req_copy);
	php_http_message_object_t *obj = php_http_message_object_new_ex(php_http_message_get_class_entry(), zipped);

	ZVAL_OBJ(&new_hist, &obj->zo);

	if (Z_TYPE_P(old_hist) == IS_OBJECT) {
		php_http_message_object_prepend(&new_hist, old_hist, 1);
	}

	zend_update_property(php_http_client_class_entry, zclient, ZEND_STRL("history"), &new_hist);
	zval_ptr_dtor(&new_hist);
}

static ZEND_RESULT_CODE handle_response(void *arg, php_http_client_t *client, php_http_client_enqueue_t *e, php_http_message_t **response)
{
	zend_bool dequeue = 0;
	zval zclient;
	php_http_message_t *msg;
	php_http_client_progress_state_t *progress;

	ZVAL_OBJ(&zclient, &((php_http_client_object_t*) arg)->zo);

	if ((msg = *response)) {
		php_http_message_object_t *msg_obj;
		zval info, zresponse, zrequest, rec_hist_tmp;
		HashTable *info_ht;

		/* ensure the message is of type response (could be uninitialized in case of early error, like DNS) */
		php_http_message_set_type(msg, PHP_HTTP_RESPONSE);

		if (zend_is_true(zend_read_property(php_http_client_class_entry, &zclient, ZEND_STRL("recordHistory"), 0, &rec_hist_tmp))) {
			handle_history(&zclient, e->request, *response);
		}

		/* hard detach, redirects etc. are in the history */
		php_http_message_free(&msg->parent);
		*response = NULL;

		msg_obj = php_http_message_object_new_ex(php_http_get_client_response_class_entry(), msg);
		ZVAL_OBJECT(&zresponse, &msg_obj->zo, 1);
		ZVAL_OBJECT(&zrequest, &((php_http_message_object_t *) e->opaque)->zo, 1);

		php_http_message_object_prepend(&zresponse, &zrequest, 1);

		object_init(&info);
		info_ht = HASH_OF(&info);
		php_http_client_getopt(client, PHP_HTTP_CLIENT_OPT_TRANSFER_INFO, e->request, &info_ht);
		zend_update_property(php_http_get_client_response_class_entry(), &zresponse, ZEND_STRL("transferInfo"), &info);
		zval_ptr_dtor(&info);

		zend_llist_add_element(&client->responses, &msg_obj);

		if (e->closure.fci.size) {
			zval retval;
			zend_error_handling zeh;

			ZVAL_UNDEF(&retval);
			zend_fcall_info_argn(&e->closure.fci, 1, &zresponse);
			zend_replace_error_handling(EH_NORMAL, NULL, &zeh);
			++client->callback.depth;
			zend_fcall_info_call(&e->closure.fci, &e->closure.fcc, &retval, NULL);
			--client->callback.depth;
			zend_restore_error_handling(&zeh);
			zend_fcall_info_argn(&e->closure.fci, 0);

			if (Z_TYPE(retval) == IS_TRUE) {
				dequeue = 1;
			}
			zval_ptr_dtor(&retval);
		}

		zval_ptr_dtor(&zresponse);
		zval_ptr_dtor(&zrequest);
	}

	if (SUCCESS == php_http_client_getopt(client, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, e->request, &progress)) {
		progress->info = "finished";
		progress->finished = 1;
		client->callback.progress.func(client->callback.progress.arg, client, e, progress);
	}

	if (dequeue) {
		php_http_client_dequeue(client, e->request);
	}

	return SUCCESS;
}

static void handle_progress(void *arg, php_http_client_t *client, php_http_client_enqueue_t *e, php_http_client_progress_state_t *progress)
{
	zval zclient, args[2];
	php_http_client_object_t *client_obj = arg;
	zend_error_handling zeh;

	ZVAL_OBJECT(&zclient, &client_obj->zo, 1);
	ZVAL_OBJECT(&args[0], &((php_http_message_object_t *) e->opaque)->zo, 1);
	object_init(&args[1]);
	add_property_bool(&args[1], "started", progress->started);
	add_property_bool(&args[1], "finished", progress->finished);
	add_property_string(&args[1], "info", STR_PTR(progress->info));
	add_property_double(&args[1], "dltotal", progress->dl.total);
	add_property_double(&args[1], "dlnow", progress->dl.now);
	add_property_double(&args[1], "ultotal", progress->ul.total);
	add_property_double(&args[1], "ulnow", progress->ul.now);

	zend_replace_error_handling(EH_NORMAL, NULL, &zeh);
	++client->callback.depth;
	php_http_object_method_call(&client_obj->notify, &zclient, NULL, 2, args);
	--client->callback.depth;
	zend_restore_error_handling(&zeh);

	zval_ptr_dtor(&zclient);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
}

static void handle_debug(void *arg, php_http_client_t *client, php_http_client_enqueue_t *e, unsigned type, const char *data, size_t size)
{
	zval ztype, zdata, zreq, zclient;
	php_http_client_object_t *client_obj = arg;
	zend_error_handling zeh;

	ZVAL_OBJECT(&zclient, &client_obj->zo, 1);
	ZVAL_OBJECT(&zreq, &((php_http_message_object_t *) e->opaque)->zo, 1);
	ZVAL_LONG(&ztype, type);
	ZVAL_STRINGL(&zdata, data, size);

	zend_replace_error_handling(EH_NORMAL, NULL, &zeh);
	if (SUCCESS == zend_fcall_info_argn(&client_obj->debug.fci, 4, &zclient, &zreq, &ztype, &zdata)) {
		++client->callback.depth;
		zend_fcall_info_call(&client_obj->debug.fci, &client_obj->debug.fcc, NULL, NULL);
		--client->callback.depth;
		zend_fcall_info_args_clear(&client_obj->debug.fci, 0);
	}
	zend_restore_error_handling(&zeh);

	zval_ptr_dtor(&zclient);
	zval_ptr_dtor(&zreq);
	zval_ptr_dtor(&ztype);
	zval_ptr_dtor(&zdata);
}

static void response_dtor(void *data)
{
	php_http_message_object_t *msg_obj = *(php_http_message_object_t **) data;

	zend_object_release(&msg_obj->zo);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, driver)
	ZEND_ARG_INFO(0, persistent_handle_id)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, __construct)
{
		zend_string *driver_name = NULL, *persistent_handle_name = NULL;
		php_http_client_driver_t *driver;
		php_resource_factory_t *rf = NULL;
		php_http_client_object_t *obj;
		zval os;

		php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|S!S!", &driver_name, &persistent_handle_name), invalid_arg, return);

		if (!zend_hash_num_elements(&php_http_client_drivers)) {
			php_http_throw(unexpected_val, "No http\\Client drivers available", NULL);
			return;
		}
		if (!(driver = php_http_client_driver_get(driver_name))) {
			php_http_throw(unexpected_val, "Failed to locate \"%s\" client request handler", driver_name ? driver_name->val : "default");
			return;
		}

		object_init_ex(&os, spl_ce_SplObjectStorage);
		zend_update_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), &os);
		zval_ptr_dtor(&os);

		if (persistent_handle_name) {
			php_persistent_handle_factory_t *pf;

			if ((pf = php_persistent_handle_concede(NULL, driver->client_name, persistent_handle_name, NULL, NULL))) {
				rf = php_persistent_handle_resource_factory_init(NULL, pf);
			}
		}

		obj = PHP_HTTP_OBJ(NULL, getThis());

		php_http_expect(obj->client = php_http_client_init(NULL, driver->client_ops, rf, NULL), runtime, return);

		php_http_object_method_init(&obj->notify, getThis(), ZEND_STRL("notify"));

		obj->client->callback.response.func = handle_response;
		obj->client->callback.response.arg = obj;
		obj->client->callback.progress.func = handle_progress;
		obj->client->callback.progress.arg = obj;

		obj->client->responses.dtor = response_dtor;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_reset, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, reset)
{
	php_http_client_object_t *obj;
	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	obj->iterator = 0;
	php_http_client_reset(obj->client);

	RETVAL_ZVAL(getThis(), 1, 0);
}

static HashTable *combined_options(zval *client, zval *request)
{
	HashTable *options;
	unsigned num_options = 0;
	zval z_roptions, z_options_tmp, *z_coptions = zend_read_property(php_http_client_class_entry, client, ZEND_STRL("options"), 0, &z_options_tmp);

	if (Z_TYPE_P(z_coptions) == IS_ARRAY) {
		num_options = zend_hash_num_elements(Z_ARRVAL_P(z_coptions));
	}
	ZVAL_UNDEF(&z_roptions);
	zend_call_method_with_0_params(request, NULL, NULL, "getOptions", &z_roptions);
	if (Z_TYPE(z_roptions) == IS_ARRAY) {
		unsigned num = zend_hash_num_elements(Z_ARRVAL(z_roptions));
		if (num > num_options) {
			num_options = num;
		}
	}
	ALLOC_HASHTABLE(options);
	ZEND_INIT_SYMTABLE_EX(options, num_options, 0);
	if (Z_TYPE_P(z_coptions) == IS_ARRAY) {
		array_copy(Z_ARRVAL_P(z_coptions), options);
	}
	if (Z_TYPE(z_roptions) == IS_ARRAY) {
		array_join(Z_ARRVAL(z_roptions), options, 0, 0);
	}
	zval_ptr_dtor(&z_roptions);

	return options;
}

static void msg_queue_dtor(php_http_client_enqueue_t *e)
{
	php_http_message_object_t *msg_obj = e->opaque;

	zend_object_release(&msg_obj->zo);
	zend_hash_destroy(e->options);
	FREE_HASHTABLE(e->options);

	if (e->closure.fci.size) {
		zval_ptr_dtor(&e->closure.fci.function_name);
		if (e->closure.fci.object) {
			zend_object_release(e->closure.fci.object);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_enqueue, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
	ZEND_ARG_INFO(0, callable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, enqueue)
{
	zval *request;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	php_http_client_object_t *obj;
	php_http_message_object_t *msg_obj;
	php_http_client_enqueue_t q = {0};

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O|f", &request, php_http_get_client_request_class_entry(), &fci, &fcc), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	msg_obj = PHP_HTTP_OBJ(NULL, request);

	if (php_http_client_enqueued(obj->client, msg_obj->message, NULL)) {
		php_http_throw(bad_method_call, "Failed to enqueue request; request already in queue", NULL);
		return;
	}

	/* set early for progress callback */
	q.opaque = msg_obj;

	if (obj->client->callback.progress.func) {
		php_http_client_progress_state_t progress = {0};

		progress.info = "prepare";
		obj->client->callback.progress.func(obj->client->callback.progress.arg, obj->client, &q, &progress);
	}

	Z_ADDREF_P(request);
	q.request = msg_obj->message;
	q.options = combined_options(getThis(), request);
	q.dtor = msg_queue_dtor;
	q.opaque = msg_obj;
	q.closure.fci = fci;
	q.closure.fcc = fcc;

	if (fci.size) {
		Z_TRY_ADDREF(fci.function_name);
		if (fci.object) {
			GC_ADDREF(fci.object);
		}
	}

	php_http_expect(SUCCESS == php_http_client_enqueue(obj->client, &q), runtime,
			msg_queue_dtor(&q);
			return;
	);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_dequeue, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, dequeue)
{
	zval *request;
	php_http_client_object_t *obj;
	php_http_message_object_t *msg_obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &request, php_http_get_client_request_class_entry()), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	msg_obj = PHP_HTTP_OBJ(NULL, request);

	if (!php_http_client_enqueued(obj->client, msg_obj->message, NULL)) {
		php_http_throw(bad_method_call, "Failed to dequeue request; request not in queue", NULL);
		return;
	}

	php_http_expect(SUCCESS == php_http_client_dequeue(obj->client, msg_obj->message), runtime, return);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_requeue, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
	ZEND_ARG_INFO(0, callable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, requeue)
{
	zval *request;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	php_http_client_object_t *obj;
	php_http_message_object_t *msg_obj;
	php_http_client_enqueue_t q;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O|f", &request, php_http_get_client_request_class_entry(), &fci, &fcc), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	msg_obj = PHP_HTTP_OBJ(NULL, request);

	if (php_http_client_enqueued(obj->client, msg_obj->message, NULL)) {
		php_http_expect(SUCCESS == php_http_client_dequeue(obj->client, msg_obj->message), runtime, return);
	}

	q.request = msg_obj->message;
	q.options = combined_options(getThis(), request);
	q.dtor = msg_queue_dtor;
	q.opaque = msg_obj;
	q.closure.fci = fci;
	q.closure.fcc = fcc;

	if (fci.size) {
		Z_TRY_ADDREF(fci.function_name);
		if (fci.object) {
			GC_ADDREF(fci.object);
		}
	}

	Z_ADDREF_P(request);

	php_http_expect(SUCCESS == php_http_client_enqueue(obj->client, &q), runtime,
			msg_queue_dtor(&q);
			return;
	);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_count, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, count)
{
	zend_long count_mode = -1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &count_mode)) {
		php_http_client_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		RETVAL_LONG(zend_llist_count(&obj->client->requests));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getResponse, 0, 0, 0)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getResponse)
{
	zval *zrequest = NULL;
	php_http_client_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|O", &zrequest, php_http_get_client_request_class_entry()), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	if (zrequest) {
		/* lookup the response with the request */
		zend_llist_element *el = NULL;
		php_http_message_object_t *req_obj = PHP_HTTP_OBJ(NULL, zrequest);

		for (el = obj->client->responses.head; el; el = el->next) {
			php_http_message_object_t *response_obj = *(php_http_message_object_t **) el->data;

			if (response_obj->message->parent == req_obj->message) {
				RETURN_OBJECT(&response_obj->zo, 1);
			}
		}

		/* not found for the request! */
		php_http_throw(unexpected_val, "Could not find response for the request", NULL);
		return;
	}

	/* pop off the last response */
	if (obj->client->responses.tail) {
		php_http_message_object_t *response_obj = *(php_http_message_object_t **) obj->client->responses.tail->data;

		/* pop off and go */
		if (response_obj) {
			RETVAL_OBJECT(&response_obj->zo, 1);
			zend_llist_remove_tail(&obj->client->responses);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getHistory, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getHistory)
{
	zval zhistory_tmp, *zhistory;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	zhistory = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("history"), 0, &zhistory_tmp);
	RETVAL_ZVAL(zhistory, 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_send, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, send)
{
	php_http_client_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	php_http_expect(SUCCESS == php_http_client_exec(obj->client), runtime, return);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_once, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, once)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		RETURN_BOOL(0 < php_http_client_once(obj->client));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_wait, 0, 0, 0)
	ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, wait)
{
	double timeout = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|d", &timeout)) {
		struct timeval timeout_val;
		php_http_client_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		timeout_val.tv_sec = (time_t) timeout;
		timeout_val.tv_usec = PHP_HTTP_USEC(timeout) % PHP_HTTP_MCROSEC;

		RETURN_BOOL(SUCCESS == php_http_client_wait(obj->client, timeout > 0 ? &timeout_val : NULL));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_configure, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, settings, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, configure)
{
	HashTable *settings = NULL;
	php_http_client_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|H!", &settings), invalid_arg, return);
	obj = PHP_HTTP_OBJ(NULL, getThis());

	php_http_expect(SUCCESS == php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_CONFIGURATION, settings), unexpected_val, return);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_enablePipelining, 0, 0, 0)
	ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, enablePipelining)
{
	zend_bool enable = 1;
	php_http_client_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enable), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	php_http_expect(SUCCESS == php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_ENABLE_PIPELINING, &enable), unexpected_val, return);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_enableEvents, 0, 0, 0)
	ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, enableEvents)
{
	zend_bool enable = 1;
	php_http_client_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enable), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	php_http_expect(SUCCESS == php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_USE_EVENTS, &enable), unexpected_val, return);

	RETVAL_ZVAL(getThis(), 1, 0);
}

struct notify_arg {
	php_http_object_method_t *cb;
	zval args[3];
	int argc;
};

static int notify(zend_object_iterator *iter, void *puser)
{
	zval *observer;
	struct notify_arg *arg = puser;

	if ((observer = iter->funcs->get_current_data(iter))) {
		if (SUCCESS == php_http_object_method_call(arg->cb, observer, NULL, arg->argc, arg->args)) {
			return ZEND_HASH_APPLY_KEEP;
		}
	}
	return ZEND_HASH_APPLY_STOP;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_notify, 0, 0, 0)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, notify)
{
	zval *request = NULL, *zprogress = NULL, observers_tmp, *observers;
	php_http_client_object_t *client_obj;
	struct notify_arg arg = {NULL};

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|O!o!", &request, php_http_get_client_request_class_entry(), &zprogress), invalid_arg, return);

	client_obj = PHP_HTTP_OBJ(NULL, getThis());
	observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0, &observers_tmp);

	if (Z_TYPE_P(observers) != IS_OBJECT) {
		php_http_throw(unexpected_val, "Observer storage is corrupted", NULL);
		return;
	}

	if (client_obj->update) {
		arg.cb = client_obj->update;
		ZVAL_COPY(&arg.args[0], getThis());
		arg.argc = 1;

		if (request) {
			ZVAL_COPY(&arg.args[1], request);
			arg.argc += 1;
		}
		if (zprogress) {
			ZVAL_COPY(&arg.args[2], zprogress);
			arg.argc += 1;
		}

		spl_iterator_apply(observers, notify, &arg);

		zval_ptr_dtor(getThis());
		if (request) {
			zval_ptr_dtor(request);
		}
		if (zprogress) {
			zval_ptr_dtor(zprogress);
		}
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_attach, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, observer, SplObserver, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, attach)
{
	zval observers_tmp, *observers, *observer, retval;
	php_http_client_object_t *client_obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &observer, spl_ce_SplObserver), invalid_arg, return);

	client_obj = PHP_HTTP_OBJ(NULL, getThis());
	observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0, &observers_tmp);

	if (Z_TYPE_P(observers) != IS_OBJECT) {
		php_http_throw(unexpected_val, "Observer storage is corrupted", NULL);
		return;
	}

	if (!client_obj->update) {
		client_obj->update = php_http_object_method_init(NULL, observer, ZEND_STRL("update"));
	}

	ZVAL_UNDEF(&retval);
	zend_call_method_with_1_params(observers, NULL, NULL, "attach", &retval, observer);
	zval_ptr_dtor(&retval);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_detach, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, observer, SplObserver, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, detach)
{
	zval observers_tmp, *observers, *observer, retval;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &observer, spl_ce_SplObserver), invalid_arg, return);

	observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0, &observers_tmp);

	if (Z_TYPE_P(observers) != IS_OBJECT) {
		php_http_throw(unexpected_val, "Observer storage is corrupted", NULL);
		return;
	}

	ZVAL_UNDEF(&retval);
	zend_call_method_with_1_params(observers, NULL, NULL, "detach", &retval, observer);
	zval_ptr_dtor(&retval);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getObservers, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getObservers)
{
	zval observers_tmp, *observers;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0, &observers_tmp);

	if (Z_TYPE_P(observers) != IS_OBJECT) {
		php_http_throw(unexpected_val, "Observer storage is corrupted", NULL);
		return;
	}

	RETVAL_ZVAL(observers, 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getProgressInfo, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getProgressInfo)
{
	zval *request;
	php_http_client_object_t *obj;
	php_http_message_object_t *req_obj;
	php_http_client_progress_state_t *progress;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &request, php_http_get_client_request_class_entry()), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	req_obj = PHP_HTTP_OBJ(NULL, request);

	php_http_expect(SUCCESS == php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, req_obj->message, &progress), unexpected_val, return);

	object_init(return_value);
	add_property_bool(return_value, "started", progress->started);
	add_property_bool(return_value, "finished", progress->finished);
	add_property_string(return_value, "info", STR_PTR(progress->info));
	add_property_double(return_value, "dltotal", progress->dl.total);
	add_property_double(return_value, "dlnow", progress->dl.now);
	add_property_double(return_value, "ultotal", progress->ul.total);
	add_property_double(return_value, "ulnow", progress->ul.now);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getTransferInfo, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getTransferInfo)
{
	zval *request;
	HashTable *info;
	php_http_client_object_t *obj;
	php_http_message_object_t *req_obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &request, php_http_get_client_request_class_entry()), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	req_obj = PHP_HTTP_OBJ(NULL, request);

	object_init(return_value);
	info = HASH_OF(return_value);
	php_http_expect(SUCCESS == php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_TRANSFER_INFO, req_obj->message, &info), unexpected_val, return);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_setOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, setOptions)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set(getThis(), opts);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval options_tmp, *options = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("options"), 0, &options_tmp);
		RETVAL_ZVAL(options, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_setSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_option, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, setSslOptions)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set_subr(getThis(), ZEND_STRL("ssl"), opts, 1);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_addSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, addSslOptions)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set_subr(getThis(), ZEND_STRL("ssl"), opts, 0);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getSslOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getSslOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRL("ssl"), return_value);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_setCookies, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, cookies, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, setCookies)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set_subr(getThis(), ZEND_STRL("cookies"), opts, 1);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_addCookies, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, cookies, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, addCookies)
{
	zval *opts = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|a!/", &opts), invalid_arg, return);

	php_http_client_options_set_subr(getThis(), ZEND_STRL("cookies"), opts, 0);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getCookies, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getCookies)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRL("cookies"), return_value);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getAvailableDrivers, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getAvailableDrivers)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		array_init(return_value);
		php_http_client_driver_list(Z_ARRVAL_P(return_value));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getAvailableOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getAvailableOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		array_init(return_value);
		php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_AVAILABLE_OPTIONS, NULL, &Z_ARRVAL_P(return_value));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getAvailableConfiguration, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getAvailableConfiguration)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		array_init(return_value);
		php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_AVAILABLE_CONFIGURATION, NULL, &Z_ARRVAL_P(return_value));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_setDebug, 0, 0, 1)
	/* using IS_CALLABLE type hint would create a forwards compatibility break */
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, setDebug)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
	php_http_client_object_t *client_obj;

	fci.size = 0;
	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|f", &fci, &fcc), invalid_arg, return);

	client_obj = PHP_HTTP_OBJ(NULL, getThis());

	if (client_obj->debug.fci.size > 0) {
		zval_ptr_dtor(&client_obj->debug.fci.function_name);
		client_obj->debug.fci.size = 0;
	}
	if (fci.size > 0) {
		memcpy(&client_obj->debug.fci, &fci, sizeof(fci));
		memcpy(&client_obj->debug.fcc, &fcc, sizeof(fcc));
		Z_ADDREF_P(&fci.function_name);
		client_obj->client->callback.debug.func = handle_debug;
		client_obj->client->callback.debug.arg = client_obj;
	} else {
		client_obj->client->callback.debug.func = NULL;
		client_obj->client->callback.debug.arg = NULL;
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

static zend_function_entry php_http_client_methods[] = {
	PHP_ME(HttpClient, __construct,          ai_HttpClient_construct,            ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpClient, reset,                ai_HttpClient_reset,                ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, enqueue,              ai_HttpClient_enqueue,              ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, dequeue,              ai_HttpClient_dequeue,              ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, requeue,              ai_HttpClient_requeue,              ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, count,                ai_HttpClient_count,                ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, send,                 ai_HttpClient_send,                 ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, once,                 ai_HttpClient_once,                 ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, wait,                 ai_HttpClient_wait,                 ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getResponse,          ai_HttpClient_getResponse,          ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getHistory,           ai_HttpClient_getHistory,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, configure,            ai_HttpClient_configure,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, enablePipelining,     ai_HttpClient_enablePipelining,     ZEND_ACC_PUBLIC|ZEND_ACC_DEPRECATED)
	PHP_ME(HttpClient, enableEvents,         ai_HttpClient_enableEvents,         ZEND_ACC_PUBLIC|ZEND_ACC_DEPRECATED)
	PHP_ME(HttpClient, notify,               ai_HttpClient_notify,               ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, attach,               ai_HttpClient_attach,               ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, detach,               ai_HttpClient_detach,               ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getObservers,         ai_HttpClient_getObservers,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getProgressInfo,      ai_HttpClient_getProgressInfo,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getTransferInfo,      ai_HttpClient_getTransferInfo,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, setOptions,           ai_HttpClient_setOptions,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getOptions,           ai_HttpClient_getOptions,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, setSslOptions,        ai_HttpClient_setSslOptions,        ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, addSslOptions,        ai_HttpClient_addSslOptions,        ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getSslOptions,        ai_HttpClient_getSslOptions,        ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, setCookies,           ai_HttpClient_setCookies,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, addCookies,           ai_HttpClient_addCookies,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getCookies,           ai_HttpClient_getCookies,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getAvailableDrivers,  ai_HttpClient_getAvailableDrivers,  ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpClient, getAvailableOptions,  ai_HttpClient_getAvailableOptions,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getAvailableConfiguration, ai_HttpClient_getAvailableConfiguration, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, setDebug,             ai_HttpClient_setDebug,             ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_client)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Client", php_http_client_methods);
	php_http_client_class_entry = zend_register_internal_class_ex(&ce, NULL);
	php_http_client_class_entry->create_object = php_http_client_object_new;
	zend_class_implements(php_http_client_class_entry, 2, spl_ce_SplSubject, spl_ce_Countable);
	memcpy(&php_http_client_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_client_object_handlers.offset = XtOffsetOf(php_http_client_object_t, zo);
	php_http_client_object_handlers.free_obj = php_http_client_object_free;
	php_http_client_object_handlers.clone_obj = NULL;
	php_http_client_object_handlers.get_gc = php_http_client_object_get_gc;
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("observers"), ZEND_ACC_PRIVATE);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("options"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("history"), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(php_http_client_class_entry, ZEND_STRL("recordHistory"), 0, ZEND_ACC_PUBLIC);

	zend_declare_class_constant_long(php_http_client_class_entry, ZEND_STRL("DEBUG_INFO"), PHP_HTTP_CLIENT_DEBUG_INFO);
	zend_declare_class_constant_long(php_http_client_class_entry, ZEND_STRL("DEBUG_IN"), PHP_HTTP_CLIENT_DEBUG_IN);
	zend_declare_class_constant_long(php_http_client_class_entry, ZEND_STRL("DEBUG_OUT"), PHP_HTTP_CLIENT_DEBUG_OUT);
	zend_declare_class_constant_long(php_http_client_class_entry, ZEND_STRL("DEBUG_HEADER"), PHP_HTTP_CLIENT_DEBUG_HEADER);
	zend_declare_class_constant_long(php_http_client_class_entry, ZEND_STRL("DEBUG_BODY"), PHP_HTTP_CLIENT_DEBUG_BODY);
	zend_declare_class_constant_long(php_http_client_class_entry, ZEND_STRL("DEBUG_SSL"), PHP_HTTP_CLIENT_DEBUG_SSL);

	zend_hash_init(&php_http_client_drivers, 2, NULL, php_http_client_driver_hash_dtor, 1);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_client)
{
	zend_hash_destroy(&php_http_client_drivers);
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
