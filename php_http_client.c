#include "php_http_api.h"
#include "php_http_client.h"

#include <ext/spl/spl_observer.h>

/*
 * array of name => php_http_client_driver_t*
 */
static HashTable php_http_client_drivers;

PHP_HTTP_API STATUS php_http_client_driver_add(const char *name_str, uint name_len, php_http_client_driver_t *driver)
{
	return zend_hash_add(&php_http_client_drivers, name_str, name_len + 1, (void *) driver, sizeof(php_http_client_driver_t), NULL);
}

PHP_HTTP_API STATUS php_http_client_driver_get(char **name_str, uint *name_len, php_http_client_driver_t *driver)
{
	php_http_client_driver_t *tmp;

	if (*name_str && SUCCESS == zend_hash_find(&php_http_client_drivers, *name_str, (*name_len) + 1, (void *) &tmp)) {
		*driver = *tmp;
		return SUCCESS;
	} else if (SUCCESS == zend_hash_get_current_data(&php_http_client_drivers, (void *) &tmp)) {
		zend_hash_get_current_key_ex(&php_http_client_drivers, name_str, name_len, NULL, 0, NULL);
		--(*name_len);
		*driver = *tmp;
		return SUCCESS;
	}
	return FAILURE;
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

void php_http_client_options_get_subr(zval *this_ptr, char *key, size_t len, zval *return_value TSRMLS_DC)
{
	zend_class_entry *this_ce = Z_OBJCE_P(getThis());
	zval **options, *opts = zend_read_property(this_ce, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);

	if ((Z_TYPE_P(opts) == IS_ARRAY) && (SUCCESS == zend_symtable_find(Z_ARRVAL_P(opts), key, len, (void *) &options))) {
		RETVAL_ZVAL(*options, 1, 0);
	}
}

static void queue_dtor(void *enqueued)
{
	php_http_client_enqueue_t *e = enqueued;

	if (e->dtor) {
		e->dtor(e);
	}
}

PHP_HTTP_API php_http_client_t *php_http_client_init(php_http_client_t *h, php_http_client_ops_t *ops, php_resource_factory_t *rf, void *init_arg TSRMLS_DC)
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
	TSRMLS_SET_CTX(h->ts);

	if (h->ops->init) {
		if (!(h = h->ops->init(h, init_arg))) {
			php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "Could not initialize client");
			if (free_h) {
				efree(h);
			}
		}
	}

	return h;
}

PHP_HTTP_API php_http_client_t *php_http_client_copy(php_http_client_t *from, php_http_client_t *to)
{
	if (from->ops->copy) {
		return from->ops->copy(from, to);
	}

	return NULL;
}

PHP_HTTP_API void php_http_client_dtor(php_http_client_t *h)
{
	if (h->ops->dtor) {
		h->ops->dtor(h);
	}

	zend_llist_clean(&h->requests);
	zend_llist_clean(&h->responses);

	php_resource_factory_free(&h->rf);
}

PHP_HTTP_API void php_http_client_free(php_http_client_t **h) {
	if (*h) {
		php_http_client_dtor(*h);
		efree(*h);
		*h = NULL;
	}
}

PHP_HTTP_API STATUS php_http_client_enqueue(php_http_client_t *h, php_http_client_enqueue_t *enqueue)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->enqueue) {
		if (php_http_client_enqueued(h, enqueue->request, NULL)) {
			php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "Failed to enqueue request; request already in queue");
			return FAILURE;
		}
		return h->ops->enqueue(h, enqueue);
	}

	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_dequeue(php_http_client_t *h, php_http_message_t *request)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (h->ops->dequeue) {
		php_http_client_enqueue_t *enqueue = php_http_client_enqueued(h, request, NULL);

		if (!enqueue) {
			php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "Failed to dequeue request; request not in queue");
			return FAILURE;
		}
		return h->ops->dequeue(h, enqueue);
	}
	return FAILURE;
}

PHP_HTTP_API php_http_client_enqueue_t *php_http_client_enqueued(php_http_client_t *h, void *compare_arg, php_http_client_enqueue_cmp_func_t compare_func)
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

PHP_HTTP_API STATUS php_http_client_wait(php_http_client_t *h, struct timeval *custom_timeout)
{
	if (h->ops->wait) {
		return h->ops->wait(h, custom_timeout);
	}

	return FAILURE;
}

PHP_HTTP_API int php_http_client_once(php_http_client_t *h)
{
	if (h->ops->once) {
		return h->ops->once(h);
	}

	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_exec(php_http_client_t *h)
{
	if (h->ops->exec) {
		return h->ops->exec(h);
	}

	return FAILURE;
}

PHP_HTTP_API void php_http_client_reset(php_http_client_t *h)
{
	if (h->ops->reset) {
		h->ops->reset(h);
	}

	zend_llist_clean(&h->requests);
	zend_llist_clean(&h->responses);
}

PHP_HTTP_API STATUS php_http_client_setopt(php_http_client_t *h, php_http_client_setopt_opt_t opt, void *arg)
{
	if (h->ops->setopt) {
		return h->ops->setopt(h, opt, arg);
	}

	return FAILURE;
}

PHP_HTTP_API STATUS php_http_client_getopt(php_http_client_t *h, php_http_client_getopt_opt_t opt, void *arg, void *res_ptr)
{
	if (h->ops->getopt) {
		return h->ops->getopt(h, opt, arg, res_ptr);
	}
	return FAILURE;
}

zend_class_entry *php_http_client_class_entry;
static zend_object_handlers php_http_client_object_handlers;

void php_http_client_object_free(void *object TSRMLS_DC)
{
	php_http_client_object_t *o = (php_http_client_object_t *) object;

	php_http_client_free(&o->client);
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

zend_object_value php_http_client_object_new_ex(zend_class_entry *ce, php_http_client_t *client, php_http_client_object_t **ptr TSRMLS_DC)
{
	php_http_client_object_t *o;

	o = ecalloc(1, sizeof(php_http_client_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	o->client = client;

	if (ptr) {
		*ptr = o;
	}

	o->zv.handle = zend_objects_store_put(o, NULL, php_http_client_object_free, NULL TSRMLS_CC);
	o->zv.handlers = &php_http_client_object_handlers;

	return o->zv;
}

zend_object_value php_http_client_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_client_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

static void handle_history(zval *zclient, php_http_message_t *request, php_http_message_t *response TSRMLS_DC)
{
	zval *new_hist, *old_hist = zend_read_property(php_http_client_class_entry, zclient, ZEND_STRL("history"), 0 TSRMLS_CC);
	php_http_message_t *zipped = php_http_message_zip(response, request);
	zend_object_value ov = php_http_message_object_new_ex(php_http_message_get_class_entry(), zipped, NULL TSRMLS_CC);

	MAKE_STD_ZVAL(new_hist);
	ZVAL_OBJVAL(new_hist, ov, 0);

	if (Z_TYPE_P(old_hist) == IS_OBJECT) {
		php_http_message_object_prepend(new_hist, old_hist, 1 TSRMLS_CC);
	}

	zend_update_property(php_http_client_class_entry, zclient, ZEND_STRL("history"), new_hist TSRMLS_CC);
	zval_ptr_dtor(&new_hist);
}

static STATUS handle_response(void *arg, php_http_client_t *client, php_http_client_enqueue_t *e, php_http_message_t **request, php_http_message_t **response)
{
	zval zclient;
	php_http_message_t *msg;
	php_http_client_progress_state_t *progress;
	TSRMLS_FETCH_FROM_CTX(client->ts);

	INIT_PZVAL(&zclient);
	ZVAL_OBJVAL(&zclient, ((php_http_client_object_t*) arg)->zv, 0);

	if ((msg = *response)) {
		php_http_message_object_t *msg_obj;
		zval *info, *zresponse, *zrequest;

		if (i_zend_is_true(zend_read_property(php_http_client_class_entry, &zclient, ZEND_STRL("recordHistory"), 0 TSRMLS_CC))) {
			handle_history(&zclient, *request, *response TSRMLS_CC);
		}

		/* hard detach, redirects etc. are in the history */
		php_http_message_free(&msg->parent);
		*response = NULL;

		MAKE_STD_ZVAL(zresponse);
		ZVAL_OBJVAL(zresponse, php_http_message_object_new_ex(php_http_client_response_get_class_entry(), msg, &msg_obj TSRMLS_CC), 0);

		MAKE_STD_ZVAL(zrequest);
		ZVAL_OBJVAL(zrequest, ((php_http_message_object_t *) e->opaque)->zv, 1);

		php_http_message_object_prepend(zresponse, zrequest, 1 TSRMLS_CC);

		MAKE_STD_ZVAL(info);
		array_init(info);
		php_http_client_getopt(client, PHP_HTTP_CLIENT_OPT_TRANSFER_INFO, e->request, &Z_ARRVAL_P(info));
		zend_update_property(php_http_client_response_get_class_entry(), zresponse, ZEND_STRL("transferInfo"), info TSRMLS_CC);
		zval_ptr_dtor(&info);

		zend_objects_store_add_ref_by_handle(msg_obj->zv.handle TSRMLS_CC);
		zend_llist_add_element(&client->responses, &msg_obj);

		if (e->closure.fci.size) {
			zval *retval = NULL;

			zend_fcall_info_argn(&e->closure.fci TSRMLS_CC, 1, &zresponse);
			with_error_handling(EH_NORMAL, NULL) {
				zend_fcall_info_call(&e->closure.fci, &e->closure.fcc, &retval, NULL TSRMLS_CC);
			} end_error_handling();
			zend_fcall_info_argn(&e->closure.fci TSRMLS_CC, 0);

			if (retval) {
				if (Z_TYPE_P(retval) == IS_BOOL && Z_BVAL_P(retval)) {
					php_http_client_dequeue(client, e->request);
				}
				zval_ptr_dtor(&retval);
			}
		}

		zval_ptr_dtor(&zresponse);
		zval_ptr_dtor(&zrequest);
	}

	if (SUCCESS == php_http_client_getopt(client, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, e->request, &progress)) {
		progress->info = "finished";
		progress->finished = 1;
		client->callback.progress.func(client->callback.progress.arg, client, e, progress);
	}

	return SUCCESS;
}

static void handle_progress(void *arg, php_http_client_t *client, php_http_client_enqueue_t *e, php_http_client_progress_state_t *state)
{
	zval *zrequest, *retval = NULL, *zclient;
	TSRMLS_FETCH_FROM_CTX(client->ts);

	MAKE_STD_ZVAL(zclient);
	ZVAL_OBJVAL(zclient, ((php_http_client_object_t *) arg)->zv, 1);
	MAKE_STD_ZVAL(zrequest);
	ZVAL_OBJVAL(zrequest, ((php_http_message_object_t *) e->opaque)->zv, 1);
	with_error_handling(EH_NORMAL, NULL) {
		zend_call_method_with_1_params(&zclient, NULL, NULL, "notify", &retval, zrequest);
	} end_error_handling();
	zval_ptr_dtor(&zclient);
	zval_ptr_dtor(&zrequest);
	if (retval) {
		zval_ptr_dtor(&retval);
	}
}

static void response_dtor(void *data)
{
	php_http_message_object_t *msg_obj = *(php_http_message_object_t **) data;
	TSRMLS_FETCH_FROM_CTX(msg_obj->message->ts);

	zend_objects_store_del_ref_by_handle_ex(msg_obj->zv.handle, msg_obj->zv.handlers TSRMLS_CC);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, driver)
	ZEND_ARG_INFO(0, persistent_handle_id)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		char *driver_str = NULL, *persistent_handle_str = NULL;
		int driver_len = 0, persistent_handle_len = 0;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss", &driver_str, &driver_len, &persistent_handle_str, &persistent_handle_len)) {
			php_http_client_driver_t driver;

			if (SUCCESS == php_http_client_driver_get(&driver_str, (uint *) &driver_len, &driver)) {
				php_resource_factory_t *rf = NULL;
				php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
				zval *os;

				MAKE_STD_ZVAL(os);
				object_init_ex(os, spl_ce_SplObjectStorage);
				zend_update_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), os TSRMLS_CC);
				zval_ptr_dtor(&os);

				if (persistent_handle_len) {
					char *name_str;
					size_t name_len;
					php_persistent_handle_factory_t *pf;

					name_len = spprintf(&name_str, 0, "http\\Client\\%s", php_http_pretty_key(driver_str, driver_len, 1, 1));

					if ((pf = php_persistent_handle_concede(NULL , name_str, name_len, persistent_handle_str, persistent_handle_len, NULL, NULL TSRMLS_CC))) {
						rf = php_resource_factory_init(NULL, php_persistent_handle_get_resource_factory_ops(), pf, (void (*)(void *)) php_persistent_handle_abandon);
					}

					efree(name_str);
				}

				if ((obj->client = php_http_client_init(NULL, driver.client_ops, rf, NULL TSRMLS_CC))) {
					obj->client->callback.response.func = handle_response;
					obj->client->callback.response.arg = obj;
					obj->client->callback.progress.func = handle_progress;
					obj->client->callback.progress.arg = obj;

					obj->client->responses.dtor = response_dtor;
				}
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_FACTORY, "Failed to locate \"%s\" client request handler", driver_str);
			}
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_reset, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, reset)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		obj->iterator = 0;
		php_http_client_reset(obj->client);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

static HashTable *combined_options(zval *client, zval *request TSRMLS_DC)
{
	HashTable *options;
	int num_options = 0;
	zval *z_roptions = NULL, *z_coptions = zend_read_property(php_http_client_class_entry, client, ZEND_STRL("options"), 0 TSRMLS_CC);

	if (Z_TYPE_P(z_coptions) == IS_ARRAY) {
		num_options = zend_hash_num_elements(Z_ARRVAL_P(z_coptions));
	}
	zend_call_method_with_0_params(&request, NULL, NULL, "getOptions", &z_roptions);
	if (z_roptions && Z_TYPE_P(z_roptions) == IS_ARRAY) {
		int num = zend_hash_num_elements(Z_ARRVAL_P(z_roptions));
		if (num > num_options) {
			num_options = num;
		}
	}
	ALLOC_HASHTABLE(options);
	ZEND_INIT_SYMTABLE_EX(options, num_options, 0);
	if (Z_TYPE_P(z_coptions) == IS_ARRAY) {
		array_copy(Z_ARRVAL_P(z_coptions), options);
	}
	if (z_roptions) {
		if (Z_TYPE_P(z_roptions) == IS_ARRAY) {
			array_join(Z_ARRVAL_P(z_roptions), options, 1, 0);
		}
		zval_ptr_dtor(&z_roptions);
	}
	return options;
}

static void msg_queue_dtor(php_http_client_enqueue_t *e)
{
	php_http_message_object_t *msg_obj = e->opaque;
	TSRMLS_FETCH_FROM_CTX(msg_obj->message->ts);

	zend_objects_store_del_ref_by_handle_ex(msg_obj->zv.handle, msg_obj->zv.handlers TSRMLS_CC);
	zend_hash_destroy(e->options);
	FREE_HASHTABLE(e->options);

	if (e->closure.fci.size) {
		zval_ptr_dtor(&e->closure.fci.function_name);
		if (e->closure.fci.object_ptr) {
			zval_ptr_dtor(&e->closure.fci.object_ptr);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_enqueue, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
	ZEND_ARG_INFO(0, callable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, enqueue)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		zval *request;
		zend_fcall_info fci = empty_fcall_info;
		zend_fcall_info_cache fcc = empty_fcall_info_cache;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|f", &request, php_http_client_request_get_class_entry(), &fci, &fcc)) {
			php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			php_http_message_object_t *msg_obj = zend_object_store_get_object(request TSRMLS_CC);

			if (php_http_client_enqueued(obj->client, msg_obj->message, NULL)) {
				php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "Failed to enqueue request; request already in queue");
			} else {
				php_http_client_enqueue_t q = {
					msg_obj->message,
					combined_options(getThis(), request TSRMLS_CC),
					msg_queue_dtor,
					msg_obj,
					{fci, fcc}
				};

				if (fci.size) {
					Z_ADDREF_P(fci.function_name);
					if (fci.object_ptr) {
						Z_ADDREF_P(fci.object_ptr);
					}
				}

				zend_objects_store_add_ref_by_handle(msg_obj->zv.handle TSRMLS_CC);
				php_http_client_enqueue(obj->client, &q);
			}
		}
	} end_error_handling();

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_dequeue, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, dequeue)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		zval *request;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_client_request_get_class_entry())) {
			php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			php_http_message_object_t *msg_obj = zend_object_store_get_object(request TSRMLS_CC);

			php_http_client_dequeue(obj->client, msg_obj->message);
		}
	} end_error_handling();

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_requeue, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
	ZEND_ARG_INFO(0, callable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, requeue)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		zval *request;
		zend_fcall_info fci = empty_fcall_info;
		zend_fcall_info_cache fcc = empty_fcall_info_cache;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|f", &request, php_http_client_request_get_class_entry(), &fci, &fcc)) {
			php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			php_http_message_object_t *msg_obj = zend_object_store_get_object(request TSRMLS_CC);
			php_http_client_enqueue_t q = {
				msg_obj->message,
				combined_options(getThis(), request TSRMLS_CC),
				msg_queue_dtor,
				msg_obj,
				{fci, fcc}
			};

			if (fci.size) {
				Z_ADDREF_P(fci.function_name);
				if (fci.object_ptr) {
					Z_ADDREF_P(fci.object_ptr);
				}
			}

			zend_objects_store_add_ref_by_handle(msg_obj->zv.handle TSRMLS_CC);
			if (php_http_client_enqueued(obj->client, msg_obj->message, NULL)) {
				php_http_client_dequeue(obj->client, msg_obj->message);
			}
			php_http_client_enqueue(obj->client, &q);
		}
	} end_error_handling();

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getResponse, 0, 0, 0)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getResponse)
{
	zval *zrequest = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|O", &zrequest, php_http_client_request_get_class_entry())) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!zrequest) {
			/* pop off the last respone */
			if (obj->client->responses.tail) {
				php_http_message_object_t *response_obj = *(php_http_message_object_t **) obj->client->responses.tail->data;

				/* pop off and go */
				if (response_obj) {
					RETVAL_OBJVAL(response_obj->zv, 1);
					zend_llist_remove_tail(&obj->client->responses);
				}
			}
		} else {
			/* lookup the response with the request */
			zend_llist_element *el = NULL;
			php_http_message_object_t *req_obj = zend_object_store_get_object(zrequest TSRMLS_CC);

			for (el = obj->client->responses.head; el; el = el->next) {
				php_http_message_object_t *response_obj = *(php_http_message_object_t **) el->data;

				if (response_obj->message->parent == req_obj->message) {
					RETVAL_OBJVAL(response_obj->zv, 1);
					break;
				}
			}
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getHistory, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getHistory)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *zhistory = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("history"), 0 TSRMLS_CC);
		RETVAL_ZVAL(zhistory, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_send, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, send)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				php_http_client_exec(obj->client);
			} end_error_handling();
		}
	} end_error_handling();

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_once, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, once)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (0 < php_http_client_once(obj->client)) {
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_wait, 0, 0, 0)
	ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, wait)
{
	double timeout = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|d", &timeout)) {
		struct timeval timeout_val;
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		timeout_val.tv_sec = (time_t) timeout;
		timeout_val.tv_usec = PHP_HTTP_USEC(timeout) % PHP_HTTP_MCROSEC;

		RETURN_SUCCESS(php_http_client_wait(obj->client, timeout > 0 ? &timeout_val : NULL));
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_enablePipelining, 0, 0, 0)
	ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, enablePipelining)
{
	zend_bool enable = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_ENABLE_PIPELINING, &enable);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_enableEvents, 0, 0, 0)
	ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, enableEvents)
{
	zend_bool enable = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
		php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_client_setopt(obj->client, PHP_HTTP_CLIENT_OPT_USE_EVENTS, &enable);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

static int notify(zend_object_iterator *iter, void *puser TSRMLS_DC)
{
	zval **observer = NULL, **args = puser;

	iter->funcs->get_current_data(iter, &observer TSRMLS_CC);
	if (observer) {
		zval *retval = NULL;

		zend_call_method(observer, NULL, NULL, ZEND_STRL("update"), &retval, args[1]?2:1, args[0], args[1] TSRMLS_CC);
		if (retval) {
			zval_ptr_dtor(&retval);
		}

		return SUCCESS;
	}
	return FAILURE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_notify, 0, 0, 0)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, notify)
{
	zval *request = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|O!", &request, php_http_client_request_get_class_entry())) {
		zval *args[2], *observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);

		if (Z_TYPE_P(observers) == IS_OBJECT) {
			Z_ADDREF_P(getThis());
			if (request) {
				Z_ADDREF_P(request);
			}
			args[0] = getThis();
			args[1] = request;
			spl_iterator_apply(observers, notify, &args TSRMLS_CC);
			zval_ptr_dtor(&getThis());
			if (request) {
				zval_ptr_dtor(&request);
			}
		}
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_attach, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, observer, SplObserver, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, attach)
{
	zval *observer;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &observer, spl_ce_SplObserver)) {
		zval *retval = NULL, *observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);
		zend_call_method_with_1_params(&observers, NULL, NULL, "attach", &retval, observer);
		if (retval) {
			zval_ptr_dtor(&retval);
		}
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_detach, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, observer, SplObserver, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, detach)
{
	zval *observer;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &observer, spl_ce_SplObserver)) {
		zval *retval, *observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);
		zend_call_method_with_1_params(&observers, NULL, NULL, "detach", &retval, observer);
		zval_ptr_dtor(&retval);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getObservers, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getObservers)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *observers = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);
			RETVAL_ZVAL(observers, 1, 0);
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getProgressInfo, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getProgressInfo)
{
	zval *request;

	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_client_request_get_class_entry())) {
			php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			php_http_message_object_t *req_obj = zend_object_store_get_object(request TSRMLS_CC);
			php_http_client_progress_state_t *progress;

			if (SUCCESS == php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, req_obj->message, &progress)) {
				object_init(return_value);
				add_property_bool(return_value, "started", progress->started);
				add_property_bool(return_value, "finished", progress->finished);
				add_property_string(return_value, "info", STR_PTR(progress->info), 1);
				add_property_double(return_value, "dltotal", progress->dl.total);
				add_property_double(return_value, "dlnow", progress->dl.now);
				add_property_double(return_value, "ultotal", progress->ul.total);
				add_property_double(return_value, "ulnow", progress->ul.now);
			}
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getTransferInfo, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, request, http\\Client\\Request, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getTransferInfo)
{
	zval *request;

	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_client_request_get_class_entry())) {
			php_http_client_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			php_http_message_object_t *req_obj = zend_object_store_get_object(request TSRMLS_CC);

			array_init(return_value);
			php_http_client_getopt(obj->client, PHP_HTTP_CLIENT_OPT_TRANSFER_INFO, req_obj->message, &Z_ARRVAL_P(return_value));
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_setOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, setOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set(getThis(), opts TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *options = zend_read_property(php_http_client_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		RETVAL_ZVAL(options, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_setSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_option, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, setSslOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("ssl"), opts, 1 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_addSslOptions, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, ssl_options, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, addSslOptions)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("ssl"), opts, 0 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getSslOptions, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getSslOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRS("ssl"), return_value TSRMLS_CC);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_setCookies, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, cookies, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, setCookies)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("cookies"), opts, 1 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_addCookies, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, cookies, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, addCookies)
{
	zval *opts = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		php_http_client_options_set_subr(getThis(), ZEND_STRS("cookies"), opts, 0 TSRMLS_CC);

		RETVAL_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClient_getCookies, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClient, getCookies)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_client_options_get_subr(getThis(), ZEND_STRS("cookies"), return_value TSRMLS_CC);
	}
}

static zend_function_entry php_http_client_methods[] = {
	PHP_ME(HttpClient, __construct, ai_HttpClient_construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpClient, reset, ai_HttpClient_reset, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, enqueue, ai_HttpClient_enqueue, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, dequeue, ai_HttpClient_dequeue, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, requeue, ai_HttpClient_requeue, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, send, ai_HttpClient_send, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, once, ai_HttpClient_once, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, wait, ai_HttpClient_wait, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getResponse, ai_HttpClient_getResponse, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getHistory, ai_HttpClient_getHistory, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, enablePipelining, ai_HttpClient_enablePipelining, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, enableEvents, ai_HttpClient_enableEvents, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, notify, ai_HttpClient_notify, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, attach, ai_HttpClient_attach, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, detach, ai_HttpClient_detach, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getObservers, ai_HttpClient_getObservers, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getProgressInfo, ai_HttpClient_getProgressInfo, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getTransferInfo, ai_HttpClient_getTransferInfo, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, setOptions, ai_HttpClient_setOptions, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getOptions, ai_HttpClient_getOptions, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, setSslOptions, ai_HttpClient_setSslOptions, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, addSslOptions, ai_HttpClient_addSslOptions, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getSslOptions, ai_HttpClient_getSslOptions, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, setCookies, ai_HttpClient_setCookies, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, addCookies, ai_HttpClient_addCookies, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClient, getCookies, ai_HttpClient_getCookies, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_client)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Client", php_http_client_methods);
	php_http_client_class_entry = zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);
	php_http_client_class_entry->create_object = php_http_client_object_new;
	zend_class_implements(php_http_client_class_entry TSRMLS_CC, 1, spl_ce_SplSubject);
	memcpy(&php_http_client_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_client_object_handlers.clone_obj = NULL;
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("observers"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("options"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_client_class_entry, ZEND_STRL("history"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(php_http_client_class_entry, ZEND_STRL("recordHistory"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

	zend_hash_init(&php_http_client_drivers, 2, NULL, NULL, 1);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_client)
{
	zend_hash_destroy(&php_http_client_drivers);
	return SUCCESS;
}
