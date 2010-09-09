
#include "php_http.h"

#include <ext/spl/spl_iterators.h>

static HashTable php_http_request_datashare_options;
static php_http_request_datashare_t php_http_request_datashare_global;
static int php_http_request_datashare_compare_handles(void *h1, void *h2);
static void php_http_request_datashare_destroy_handles(void *el);

#ifdef ZTS
static void *php_http_request_datashare_locks_init(void);
static void php_http_request_datashare_locks_dtor(void *l);
static void php_http_request_datashare_lock_func(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr);
static void php_http_request_datashare_unlock_func(CURL *handle, curl_lock_data data, void *userptr);
#endif

php_http_request_datashare_t *php_http_request_datashare_global_get(void)
{
	return &php_http_request_datashare_global;
}

PHP_HTTP_API php_http_request_datashare_t *php_http_request_datashare_init(php_http_request_datashare_t *share, zend_bool persistent TSRMLS_DC)
{
	zend_bool free_share;

	if ((free_share = !share)) {
		share = pemalloc(sizeof(php_http_request_datashare_t), persistent);
	}
	memset(share, 0, sizeof(php_http_request_datashare_t));

	if (SUCCESS != php_http_persistent_handle_acquire(ZEND_STRL("http_request_datashare"), &share->ch TSRMLS_CC)) {
		if (free_share) {
			pefree(share, persistent);
		}
		return NULL;
	}

	if (!(share->persistent = persistent)) {
		share->handle.list = emalloc(sizeof(zend_llist));
		zend_llist_init(share->handle.list, sizeof(zval *), ZVAL_PTR_DTOR, 0);
#ifdef ZTS
	} else {
		if (SUCCESS == php_http_persistent_handle_acquire(ZEND_STRL("http_request_datashare_lock"), (void *) &share->handle.locks)) {
			curl_share_setopt(share->ch, CURLSHOPT_LOCKFUNC, php_http_request_datashare_lock_func);
			curl_share_setopt(share->ch, CURLSHOPT_UNLOCKFUNC, php_http_request_datashare_unlock_func);
			curl_share_setopt(share->ch, CURLSHOPT_USERDATA, share->handle.locks);
		}
#endif
	}

	TSRMLS_SET_CTX(share->ts);

	return share;
}

PHP_HTTP_API STATUS php_http_request_datashare_attach(php_http_request_datashare_t *share, zval *request)
{
	CURLcode rc;
	TSRMLS_FETCH_FROM_CTX(share->ts);
	php_http_request_object_t *obj = zend_object_store_get_object(request TSRMLS_CC);

	if (obj->share) {
		if (obj->share == share)  {
			return SUCCESS;
		} else if (SUCCESS != php_http_request_datashare_detach(obj->share, request)) {
			return FAILURE;
		}
	}

	PHP_HTTP_CHECK_CURL_INIT(obj->request->ch, php_http_curl_init(obj->request->ch, obj->request TSRMLS_CC), return FAILURE);
	if (CURLE_OK != (rc = curl_easy_setopt(obj->request->ch, CURLOPT_SHARE, share->ch))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Could not attach HttpRequest object(#%d) to the HttpRequestDataShare: %s", Z_OBJ_HANDLE_P(request), curl_easy_strerror(rc));
		return FAILURE;
	}

	obj->share = share;
	Z_ADDREF_P(request);
	zend_llist_add_element(PHP_HTTP_RSHARE_HANDLES(share), (void *) &request);

	return SUCCESS;
}

PHP_HTTP_API STATUS php_http_request_datashare_detach(php_http_request_datashare_t *share, zval *request)
{
	CURLcode rc;
	TSRMLS_FETCH_FROM_CTX(share->ts);
	php_http_request_object_t *obj = zend_object_store_get_object(request TSRMLS_CC);

	if (!obj->share) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "HttpRequest object(#%d) is not attached to any HttpRequestDataShare", Z_OBJ_HANDLE_P(request));
	} else if (obj->share != share) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "HttpRequest object(#%d) is not attached to this HttpRequestDataShare", Z_OBJ_HANDLE_P(request));
	} else if (CURLE_OK != (rc = curl_easy_setopt(obj->request->ch, CURLOPT_SHARE, NULL))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Could not detach HttpRequest object(#%d) from the HttpRequestDataShare: %s", Z_OBJ_HANDLE_P(request), curl_share_strerror(rc));
	} else {
		obj->share = NULL;
		zend_llist_del_element(PHP_HTTP_RSHARE_HANDLES(share), request, php_http_request_datashare_compare_handles);
		return SUCCESS;
	}
	return FAILURE;
}

PHP_HTTP_API void php_http_request_datashare_detach_all(php_http_request_datashare_t *share)
{
	zval **r;

	while ((r = zend_llist_get_first(PHP_HTTP_RSHARE_HANDLES(share)))) {
		php_http_request_datashare_detach(share, *r);
	}
}

PHP_HTTP_API void php_http_request_datashare_dtor(php_http_request_datashare_t *share)
{
	TSRMLS_FETCH_FROM_CTX(share->ts);

	if (!share->persistent) {
		zend_llist_destroy(share->handle.list);
		efree(share->handle.list);
	}
	php_http_persistent_handle_release(ZEND_STRL("http_request_datashare"), &share->ch TSRMLS_CC);
#ifdef ZTS
	if (share->persistent) {
		php_http_persistent_handle_release(ZEND_STRL("http_request_datashare_lock"), (void *) &share->handle.locks TSRMLS_CC);
	}
#endif
}

PHP_HTTP_API void php_http_request_datashare_free(php_http_request_datashare_t **share)
{
	php_http_request_datashare_dtor(*share);
	pefree(*share, (*share)->persistent);
	*share = NULL;
}

PHP_HTTP_API STATUS php_http_request_datashare_set(php_http_request_datashare_t *share, const char *option, size_t option_len, zend_bool enable)
{
	curl_lock_data *opt;
	CURLSHcode rc;
	TSRMLS_FETCH_FROM_CTX(share->ts);

	if (SUCCESS == zend_hash_find(&php_http_request_datashare_options, (char *) option, option_len + 1, (void *) &opt)) {
		if (CURLSHE_OK == (rc = curl_share_setopt(share->ch, enable ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, *opt))) {
			return SUCCESS;
		}
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Could not %s sharing of %s data: %s",  enable ? "enable" : "disable", option, curl_share_strerror(rc));
	}
	return FAILURE;
}

static int php_http_request_datashare_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
}

static void php_http_request_datashare_destroy_handles(void *el)
{
	zval **r = (zval **) el;
	TSRMLS_FETCH();

	{ /* gcc 2.95 needs these braces */
		php_http_request_object_t *obj = zend_object_store_get_object(*r TSRMLS_CC);

		curl_easy_setopt(obj->request->ch, CURLOPT_SHARE, NULL);
		zval_ptr_dtor(r);
	}
}

#ifdef ZTS
static void *php_http_request_datashare_locks_init(void)
{
	int i;
	php_http_request_datashare_lock_t *locks = pecalloc(CURL_LOCK_DATA_LAST, sizeof(php_http_request_datashare_lock_t), 1);

	if (locks) {
		for (i = 0; i < CURL_LOCK_DATA_LAST; ++i) {
			locks[i].mx = tsrm_mutex_alloc();
		}
	}

	return locks;
}

static void php_http_request_datashare_locks_dtor(void *l)
{
	int i;
	php_http_request_datashare_lock_t *locks = (php_http_request_datashare_lock_t *) l;

	for (i = 0; i < CURL_LOCK_DATA_LAST; ++i) {
		tsrm_mutex_free(locks[i].mx);
	}
	pefree(locks, 1);
}

static void php_http_request_datashare_lock_func(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr)
{
	php_http_request_datashare_lock_t *locks = (php_http_request_datashare_lock_t *) userptr;

	/* TSRM can't distinguish shared/exclusive locks */
	tsrm_mutex_lock(locks[data].mx);
	locks[data].ch = handle;
}

static void php_http_request_datashare_unlock_func(CURL *handle, curl_lock_data data, void *userptr)
{
	php_http_request_datashare_lock_t *locks = (php_http_request_datashare_lock_t *) userptr;

	if (locks[data].ch == handle) {
		tsrm_mutex_unlock(locks[data].mx);
	}
}
#endif


#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpRequestDataShare, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpRequestDataShare, method, 0)
#define PHP_HTTP_RSHARE_ME(method, visibility)	PHP_ME(HttpRequestDataShare, method, PHP_HTTP_ARGS(HttpRequestDataShare, method), visibility)

PHP_HTTP_EMPTY_ARGS(__destruct);
PHP_HTTP_EMPTY_ARGS(count);

PHP_HTTP_BEGIN_ARGS(attach, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(detach, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(reset);

PHP_HTTP_EMPTY_ARGS(getGlobalInstance);


static zval *php_http_request_datashare_object_read_prop(zval *object, zval *member, int type, const zend_literal *literal_key TSRMLS_DC);
static void php_http_request_datashare_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC);

#define THIS_CE php_http_request_datashare_class_entry
zend_class_entry *php_http_request_datashare_class_entry;
zend_function_entry php_http_request_datashare_method_entry[] = {
	PHP_HTTP_RSHARE_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_HTTP_RSHARE_ME(count, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(attach, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(reset, ZEND_ACC_PUBLIC)
	PHP_HTTP_RSHARE_ME(getGlobalInstance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
		o->share = php_http_request_datashare_init(NULL, 0 TSRMLS_CC);
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
	zend_object_std_dtor((zend_object *) o);
	efree(o);
}

static zval *php_http_request_datashare_object_read_prop(zval *object, zval *member, int type, const zend_literal *literal_key TSRMLS_DC)
{
	if (type == BP_VAR_W && zend_get_property_info(THIS_CE, member, 1 TSRMLS_CC)) {
		zend_error(E_ERROR, "Cannot access HttpRequestDataShare default properties by reference or array key/index");
		return NULL;
	}

	return zend_get_std_object_handlers()->read_property(object, member, type, literal_key TSRMLS_CC);
}

static void php_http_request_datashare_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC)
{
	if (zend_get_property_info(THIS_CE, member, 1 TSRMLS_CC)) {
		int status;
		php_http_request_datashare_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);

		status = php_http_request_datashare_set(obj->share, Z_STRVAL_P(member), Z_STRLEN_P(member), (zend_bool) i_zend_is_true(value));
		if (SUCCESS != status) {
			return;
		}
	}

	zend_get_std_object_handlers()->write_property(object, member, value, literal_key TSRMLS_CC);
}

PHP_METHOD(HttpRequestDataShare, __destruct)
{
	php_http_request_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	if (SUCCESS == zend_parse_parameters_none()) {
		; /* we always want to clean up */
	}

	php_http_request_datashare_detach_all(obj->share);
}

PHP_METHOD(HttpRequestDataShare, count)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_datashare_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(zend_llist_count(PHP_HTTP_RSHARE_HANDLES(obj->share)));
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

		php_http_request_datashare_detach_all(obj->share);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestDataShare, getGlobalInstance)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *instance = *zend_std_get_static_property(THIS_CE, ZEND_STRL("instance"), 0, NULL TSRMLS_CC);

			if (Z_TYPE_P(instance) != IS_OBJECT) {
				MAKE_STD_ZVAL(instance);
				ZVAL_OBJVAL(instance, php_http_request_datashare_object_new_ex(THIS_CE, php_http_request_datashare_global_get(), NULL TSRMLS_CC), 1);
				zend_update_static_property(THIS_CE, ZEND_STRL("instance"), instance TSRMLS_CC);

				if (PHP_HTTP_G->request_datashare.cookie) {
					zend_update_property_bool(THIS_CE, instance, ZEND_STRL("cookie"), PHP_HTTP_G->request_datashare.cookie TSRMLS_CC);
				}
				if (PHP_HTTP_G->request_datashare.dns) {
					zend_update_property_bool(THIS_CE, instance, ZEND_STRL("dns"), PHP_HTTP_G->request_datashare.dns TSRMLS_CC);
				}
				if (PHP_HTTP_G->request_datashare.ssl) {
					zend_update_property_bool(THIS_CE, instance, ZEND_STRL("ssl"), PHP_HTTP_G->request_datashare.ssl TSRMLS_CC);
				}
				if (PHP_HTTP_G->request_datashare.connect) {
					zend_update_property_bool(THIS_CE, instance, ZEND_STRL("connect"), PHP_HTTP_G->request_datashare.connect TSRMLS_CC);
				}
			}

			RETVAL_ZVAL(instance, 0, 0);
		}
	}end_error_handling();
}

PHP_MINIT_FUNCTION(http_request_datashare)
{
	curl_lock_data val;

	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_request_datashare"), curl_share_init, (php_http_persistent_handle_dtor_t) curl_share_cleanup, NULL)) {
		return FAILURE;
	}
#ifdef ZTS
	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_request_datashare_lock"), php_http_request_datashare_locks_init, php_http_request_datashare_locks_dtor, NULL)) {
		return FAILURE;
	}
#endif

	if (!php_http_request_datashare_init(&php_http_request_datashare_global, 1 TSRMLS_CC)) {
		return FAILURE;
	}

	zend_hash_init(&php_http_request_datashare_options, 4, NULL, NULL, 1);
#define ADD_DATASHARE_OPT(name, opt) \
	val = opt; \
	zend_hash_add(&php_http_request_datashare_options, name, sizeof(name), &val, sizeof(curl_lock_data), NULL)
	ADD_DATASHARE_OPT("cookie", CURL_LOCK_DATA_COOKIE);
	ADD_DATASHARE_OPT("dns", CURL_LOCK_DATA_DNS);
	ADD_DATASHARE_OPT("ssl", CURL_LOCK_DATA_SSL_SESSION);
	ADD_DATASHARE_OPT("connect", CURL_LOCK_DATA_CONNECT);

	PHP_HTTP_REGISTER_CLASS(http\\request, DataShare, http_request_datashare, php_http_object_class_entry, 0);
	php_http_request_datashare_class_entry->create_object = php_http_request_datashare_object_new;
	memcpy(&php_http_request_datashare_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_request_datashare_object_handlers.clone_obj = NULL;
	php_http_request_datashare_object_handlers.read_property = php_http_request_datashare_object_read_prop;
	php_http_request_datashare_object_handlers.write_property = php_http_request_datashare_object_write_prop;
	php_http_request_datashare_object_handlers.get_property_ptr_ptr = NULL;

	zend_class_implements(php_http_request_datashare_class_entry TSRMLS_CC, 1, spl_ce_Countable);

	zend_declare_property_null(THIS_CE, ZEND_STRL("instance"), (ZEND_ACC_STATIC|ZEND_ACC_PRIVATE) TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRL("cookie"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRL("dns"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRL("ssl"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRL("connect"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_request_datashare)
{
	php_http_request_datashare_dtor(&php_http_request_datashare_global);
	zend_hash_destroy(&php_http_request_datashare_options);

	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_request_datashare)
{
	zend_llist_init(&PHP_HTTP_G->request_datashare.handles, sizeof(zval *), php_http_request_datashare_destroy_handles, 0);

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_request_datashare)
{
	zend_llist_destroy(&PHP_HTTP_G->request_datashare.handles);

	return SUCCESS;
}

