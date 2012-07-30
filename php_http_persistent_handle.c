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

#ifndef PHP_HTTP_DEBUG_PHANDLES
#	define PHP_HTTP_DEBUG_PHANDLES 0
#endif
#if PHP_HTTP_DEBUG_PHANDLES
#	undef inline
#	define inline
#endif

static HashTable php_http_persistent_handles_hash;
#ifdef ZTS
#	define LOCK() tsrm_mutex_lock(php_http_persistent_handles_lock)
#	define UNLOCK() tsrm_mutex_unlock(php_http_persistent_handles_lock)
static MUTEX_T php_http_persistent_handles_lock;
#else
#	define LOCK()
#	define UNLOCK()
#endif

typedef struct php_http_persistent_handle_list {
	HashTable free;
	ulong used;
} php_http_persistent_handle_list_t;

typedef struct php_http_persistent_handle_provider {
	php_http_persistent_handle_list_t list; /* "ident" => array(handles) entries */
	php_http_resource_factory_t rf;
} php_http_persistent_handle_provider_t;

static inline php_http_persistent_handle_list_t *php_http_persistent_handle_list_init(php_http_persistent_handle_list_t *list)
{
	int free_list;
	
	if ((free_list = !list)) {
		list = pemalloc(sizeof(php_http_persistent_handle_list_t), 1);
	}
	
	list->used = 0;
	
	if (SUCCESS != zend_hash_init(&list->free, 0, NULL, NULL, 1)) {
		if (free_list) {
			pefree(list, 1);
		}
		list = NULL;
	}
	
	return list;
}

static int php_http_persistent_handle_apply_cleanup_ex(void *pp, void *arg TSRMLS_DC)
{
	php_http_resource_factory_t *rf = arg;
	void **handle = pp;

#if PHP_HTTP_DEBUG_PHANDLES
	fprintf(stderr, "DESTROY: %p\n", *handle);
#endif
	php_http_resource_factory_handle_dtor(rf, *handle TSRMLS_CC);
	return ZEND_HASH_APPLY_REMOVE;
}

static int php_http_persistent_handle_apply_cleanup(void *pp, void *arg TSRMLS_DC)
{
	php_http_resource_factory_t *rf = arg;
	php_http_persistent_handle_list_t **listp = pp;

	zend_hash_apply_with_argument(&(*listp)->free, php_http_persistent_handle_apply_cleanup_ex, rf TSRMLS_CC);
	if ((*listp)->used) {
		return ZEND_HASH_APPLY_KEEP;
	}
	zend_hash_destroy(&(*listp)->free);
#if PHP_HTTP_DEBUG_PHANDLES
	fprintf(stderr, "LSTFREE: %p\n", *listp);
#endif
	pefree(*listp, 1);
	*listp = NULL;
	return ZEND_HASH_APPLY_REMOVE;
}

static inline void php_http_persistent_handle_list_dtor(php_http_persistent_handle_list_t *list, php_http_persistent_handle_provider_t *provider TSRMLS_DC)
{
#if PHP_HTTP_DEBUG_PHANDLES
	fprintf(stderr, "LSTDTOR: %p\n", list);
#endif
	zend_hash_apply_with_argument(&list->free, php_http_persistent_handle_apply_cleanup_ex, &provider->rf TSRMLS_CC);
	zend_hash_destroy(&list->free);
}

static inline void php_http_persistent_handle_list_free(php_http_persistent_handle_list_t **list, php_http_persistent_handle_provider_t *provider TSRMLS_DC)
{
	php_http_persistent_handle_list_dtor(*list, provider TSRMLS_CC);
#if PHP_HTTP_DEBUG_PHANDLES
	fprintf(stderr, "LSTFREE: %p\n", *list);
#endif
	pefree(*list, 1);
	*list = NULL;
}

static int php_http_persistent_handle_list_apply_dtor(void *listp, void *provider TSRMLS_DC)
{
	php_http_persistent_handle_list_free(listp, provider TSRMLS_CC);
	return ZEND_HASH_APPLY_REMOVE;
}

static inline php_http_persistent_handle_list_t *php_http_persistent_handle_list_find(php_http_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len TSRMLS_DC)
{
	php_http_persistent_handle_list_t **list, *new_list;
	
	if (SUCCESS == zend_symtable_find(&provider->list.free, ident_str, ident_len + 1, (void *) &list)) {
#if PHP_HTTP_DEBUG_PHANDLES
		fprintf(stderr, "LSTFIND: %p\n", *list);
#endif
		return *list;
	}
	
	if ((new_list = php_http_persistent_handle_list_init(NULL))) {
		if (SUCCESS == zend_symtable_update(&provider->list.free, ident_str, ident_len + 1, (void *) &new_list, sizeof(php_http_persistent_handle_list_t *), (void *) &list)) {
#if PHP_HTTP_DEBUG_PHANDLES
			fprintf(stderr, "LSTFIND: %p (new)\n", *list);
#endif
			return *list;
		}
		php_http_persistent_handle_list_free(&new_list, provider TSRMLS_CC);
	}
	
	return NULL;
}

static inline STATUS php_http_persistent_handle_do_acquire(php_http_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len, void **handle TSRMLS_DC)
{
	ulong index;
	void **handle_ptr;
	php_http_persistent_handle_list_t *list;
	
	if ((list = php_http_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
		zend_hash_internal_pointer_end(&list->free);
		if (HASH_KEY_NON_EXISTANT != zend_hash_get_current_key(&list->free, NULL, &index, 0) && SUCCESS == zend_hash_get_current_data(&list->free, (void *) &handle_ptr)) {
			*handle = *handle_ptr;
			zend_hash_index_del(&list->free, index);
		} else {
			*handle = php_http_resource_factory_handle_ctor(&provider->rf TSRMLS_CC);
		}
#if PHP_HTTP_DEBUG_PHANDLES
		fprintf(stderr, "CREATED: %p\n", *handle);
#endif
		if (*handle) {
			++provider->list.used;
			++list->used;
			return SUCCESS;
		}
	} else {
		*handle = NULL;
	}
	
	return FAILURE;
}

static inline STATUS php_http_persistent_handle_do_release(php_http_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len, void **handle TSRMLS_DC)
{
	php_http_persistent_handle_list_t *list;
	
	if ((list = php_http_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
		if (provider->list.used >= PHP_HTTP_G->persistent_handle.limit) {
#if PHP_HTTP_DEBUG_PHANDLES
			fprintf(stderr, "DESTROY: %p\n", *handle);
#endif
			php_http_resource_factory_handle_dtor(&provider->rf, *handle TSRMLS_CC);
		} else {
			if (SUCCESS != zend_hash_next_index_insert(&list->free, (void *) handle, sizeof(void *), NULL)) {
				return FAILURE;
			}
		}
		
		*handle = NULL;
		--provider->list.used;
		--list->used;
		return SUCCESS;
	}
	
	return FAILURE;
}

static inline STATUS php_http_persistent_handle_do_accrete(php_http_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len, void *old_handle, void **new_handle TSRMLS_DC)
{
	php_http_persistent_handle_list_t *list;
	
	if ((*new_handle = php_http_resource_factory_handle_copy(&provider->rf, old_handle TSRMLS_CC))) {
		if ((list = php_http_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
			++list->used;
		}
		++provider->list.used;
		return SUCCESS;
	}
	return FAILURE;
}

static void php_http_persistent_handles_hash_dtor(void *p)
{
	php_http_persistent_handle_provider_t *provider = (php_http_persistent_handle_provider_t *) p;
	TSRMLS_FETCH();
	
	zend_hash_apply_with_argument(&provider->list.free, php_http_persistent_handle_list_apply_dtor, provider TSRMLS_CC);
	zend_hash_destroy(&provider->list.free);
	php_http_resource_factory_dtor(&provider->rf);
}

PHP_MINIT_FUNCTION(http_persistent_handle)
{
	zend_hash_init(&php_http_persistent_handles_hash, 0, NULL, php_http_persistent_handles_hash_dtor, 1);
#ifdef ZTS
	php_http_persistent_handles_lock = tsrm_mutex_alloc();
#endif
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_persistent_handle)
{
	zend_hash_destroy(&php_http_persistent_handles_hash);
#ifdef ZTS
	tsrm_mutex_free(php_http_persistent_handles_lock);
#endif
	return SUCCESS;
}

PHP_HTTP_API STATUS php_http_persistent_handle_provide(const char *name_str, size_t name_len, php_http_resource_factory_ops_t *fops, void *data, void (*dtor)(void *))
{
	STATUS status = FAILURE;
	php_http_persistent_handle_provider_t provider;
	
	LOCK();
	if (php_http_persistent_handle_list_init(&provider.list)) {
		if (php_http_resource_factory_init(&provider.rf, fops, data, dtor)) {
#if PHP_HTTP_DEBUG_PHANDLES
			fprintf(stderr, "PROVIDE: %s\n", name_str);
#endif
		
			if (SUCCESS == zend_symtable_update(&php_http_persistent_handles_hash, name_str, name_len+1, (void *) &provider, sizeof(php_http_persistent_handle_provider_t), NULL)) {
				status = SUCCESS;
			} else {
				php_http_resource_factory_dtor(&provider.rf);
			}
		}
	}
	UNLOCK();
	
	return status;
}

PHP_HTTP_API php_http_persistent_handle_factory_t *php_http_persistent_handle_concede(php_http_persistent_handle_factory_t *a, const char *name_str, size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC)
{
	STATUS status = FAILURE;
	php_http_persistent_handle_factory_t *free_a = NULL;

	if (!a) {
		free_a = a = emalloc(sizeof(*a));
	}
	memset(a, 0, sizeof(*a));

	LOCK();
	status = zend_symtable_find(&php_http_persistent_handles_hash, name_str, name_len+1, (void *) &a->provider);
	UNLOCK();

	if (SUCCESS == status) {
		a->ident.str = estrndup(ident_str, a->ident.len = ident_len);
		if (free_a) {
			a->free_on_abandon = 1;
		}
	} else {
		if (free_a) {
			efree(free_a);
		}
		a = NULL;
	}

#if PHP_HTTP_DEBUG_PHANDLES
	fprintf(stderr, "CONCEDE: %p (%s) (%s)\n", a ? a->provider : NULL, name_str, ident_str);
#endif

	return a;
}

PHP_HTTP_API void php_http_persistent_handle_abandon(php_http_persistent_handle_factory_t *a)
{
	zend_bool f = a->free_on_abandon;

#if PHP_HTTP_DEBUG_PHANDLES
	fprintf(stderr, "ABANDON: %p\n", a->provider);
#endif

	STR_FREE(a->ident.str);
	memset(a, 0, sizeof(*a));
	if (f) {
		efree(a);
	}
}

PHP_HTTP_API void *php_http_persistent_handle_acquire(php_http_persistent_handle_factory_t *a TSRMLS_DC)
{
	void *handle = NULL;

	LOCK();
	php_http_persistent_handle_do_acquire(a->provider, a->ident.str, a->ident.len, &handle TSRMLS_CC);
	UNLOCK();

	return handle;
}

PHP_HTTP_API void *php_http_persistent_handle_accrete(php_http_persistent_handle_factory_t *a, void *handle TSRMLS_DC)
{
	void *new_handle = NULL;

	LOCK();
	php_http_persistent_handle_do_accrete(a->provider, a->ident.str, a->ident.len, handle, &new_handle TSRMLS_CC);
	UNLOCK();

	return new_handle;
}

PHP_HTTP_API void php_http_persistent_handle_release(php_http_persistent_handle_factory_t *a, void *handle TSRMLS_DC)
{
	LOCK();
	php_http_persistent_handle_do_release(a->provider, a->ident.str, a->ident.len, &handle TSRMLS_CC);
	UNLOCK();
}

PHP_HTTP_API void php_http_persistent_handle_cleanup(const char *name_str, size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC)
{
	php_http_persistent_handle_provider_t *provider;
	php_http_persistent_handle_list_t *list;
	
	LOCK();
	if (name_str && name_len) {
		if (SUCCESS == zend_symtable_find(&php_http_persistent_handles_hash, name_str, name_len+1, (void *) &provider)) {
			if (ident_str && ident_len) {
				if ((list = php_http_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
					zend_hash_apply_with_argument(&list->free, php_http_persistent_handle_apply_cleanup_ex, &provider->rf TSRMLS_CC);
				}
			} else {
				zend_hash_apply_with_argument(&provider->list.free, php_http_persistent_handle_apply_cleanup, &provider->rf TSRMLS_CC);
			}
		}
	} else {
		HashPosition pos;

		FOREACH_HASH_VAL(pos, &php_http_persistent_handles_hash, provider) {
			if (ident_str && ident_len) {
				if ((list = php_http_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
					zend_hash_apply_with_argument(&list->free, php_http_persistent_handle_apply_cleanup_ex, &provider->rf TSRMLS_CC);
				}
			} else {
				zend_hash_apply_with_argument(&provider->list.free, php_http_persistent_handle_apply_cleanup, &provider->rf TSRMLS_CC);
			}
		}
	}
	UNLOCK();
}

PHP_HTTP_API HashTable *php_http_persistent_handle_statall(HashTable *ht TSRMLS_DC)
{
	zval *zentry[2];
	HashPosition pos1, pos2;
	php_http_array_hashkey_t key1 = php_http_array_hashkey_init(0), key2 = php_http_array_hashkey_init(0);
	php_http_persistent_handle_provider_t *provider;
	php_http_persistent_handle_list_t **list;
	
	LOCK();
	if (zend_hash_num_elements(&php_http_persistent_handles_hash)) {
		if (!ht) {
			ALLOC_HASHTABLE(ht);
			zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		}
		
		FOREACH_HASH_KEYVAL(pos1, &php_http_persistent_handles_hash, key1, provider) {
			MAKE_STD_ZVAL(zentry[0]);
			array_init(zentry[0]);
			
			FOREACH_HASH_KEYVAL(pos2, &provider->list.free, key2, list) {
				MAKE_STD_ZVAL(zentry[1]);
				array_init(zentry[1]);
				add_assoc_long_ex(zentry[1], ZEND_STRS("used"), (*list)->used);
				add_assoc_long_ex(zentry[1], ZEND_STRS("free"), zend_hash_num_elements(&(*list)->free));
				add_assoc_zval_ex(zentry[0], key2.str, key2.len, zentry[1]);
			}
			
			zend_symtable_update(ht, key1.str, key1.len, &zentry[0], sizeof(zval *), NULL);
		}
	} else if (ht) {
		ht = NULL;
	}
	UNLOCK();
	
	return ht;
}

static php_http_resource_factory_ops_t php_http_persistent_handle_rf_ops = {
	(php_http_resource_factory_handle_ctor_t) php_http_persistent_handle_acquire,
	(php_http_resource_factory_handle_copy_t) php_http_persistent_handle_accrete,
	(php_http_resource_factory_handle_dtor_t) php_http_persistent_handle_release
};

PHP_HTTP_API php_http_resource_factory_ops_t *php_http_persistent_handle_resource_factory_ops(void)
{
	return &php_http_persistent_handle_rf_ops;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

