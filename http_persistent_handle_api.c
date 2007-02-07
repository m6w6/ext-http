/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#include "php_http.h"
#include "php_http_api.h"

#ifdef HTTP_HAVE_PERSISTENT_HANDLES
#include "php_http_persistent_handle_api.h"

#ifndef HTTP_DEBUG_PHANDLES
#	define HTTP_DEBUG_PHANDLES 0
#endif

static HashTable http_persistent_handles_hash;
#ifdef ZTS
#	define LOCK() tsrm_mutex_lock(http_persistent_handles_lock)
#	define UNLOCK() tsrm_mutex_unlock(http_persistent_handles_lock)
static MUTEX_T http_persistent_handles_lock;
#else
#	define LOCK()
#	define UNLOCK()
#endif

typedef struct _http_persistent_handle_t {
	void *ptr;
} http_persistent_handle;

typedef HashTable *http_persistent_handle_list;

typedef struct _http_persistent_handle_provider_t {
	http_persistent_handle_list list; /* "ident" => array(handles) entries */
	http_persistent_handle_ctor ctor;
	http_persistent_handle_dtor dtor;
} http_persistent_handle_provider;


static inline STATUS http_persistent_handle_list_find(http_persistent_handle_list parent_list, http_persistent_handle_list **ident_list, int create TSRMLS_DC)
{
	http_persistent_handle_list new_list;
	
	if (SUCCESS == zend_hash_quick_find(parent_list, HTTP_G->persistent.handles.ident.s, HTTP_G->persistent.handles.ident.l, HTTP_G->persistent.handles.ident.h, (void *) ident_list)) {
		return SUCCESS;
	}
	
	if (create) {
		if ((new_list = pemalloc(sizeof(HashTable), 1))) {
			if (SUCCESS == zend_hash_init(new_list, 0, NULL, NULL, 1)) {
				if (SUCCESS == zend_hash_quick_add(parent_list, HTTP_G->persistent.handles.ident.s, HTTP_G->persistent.handles.ident.l, HTTP_G->persistent.handles.ident.h, (void *) &new_list, sizeof(http_persistent_handle_list), (void *) ident_list)) {
					return SUCCESS;
				}
				zend_hash_destroy(new_list);
			}
			pefree(new_list, 1);
		}
	}
	
	return FAILURE;
}

static inline void http_persistent_handle_list_dtor(http_persistent_handle_list list, http_persistent_handle_dtor dtor)
{
	HashPosition pos;
	http_persistent_handle *handle;
	
	FOREACH_HASH_VAL(pos, list, handle) {
#if HTTP_DEBUG_PHANDLES
		fprintf(stderr, "DESTROY: %p\n", handle->ptr);
#endif
		
		dtor(handle->ptr);
	}
	zend_hash_clean(list);
}

static void http_persistent_handles_hash_dtor(void *p)
{
	http_persistent_handle_provider *provider = (http_persistent_handle_provider *) p;
	http_persistent_handle_list *list;
	HashPosition pos;
	
	FOREACH_HASH_VAL(pos, provider->list, list) {
		http_persistent_handle_list_dtor(*list, provider->dtor);
		zend_hash_destroy(*list);
		pefree(*list, 1);
	}
	zend_hash_destroy(provider->list);
	pefree(provider->list, 1);
}

PHP_MINIT_FUNCTION(http_persistent_handle)
{
	zend_hash_init(&http_persistent_handles_hash, 0, NULL, http_persistent_handles_hash_dtor, 1);
#ifdef ZTS
	http_persistent_handles_lock = tsrm_mutex_alloc();
#endif
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_persistent_handle)
{
	zend_hash_destroy(&http_persistent_handles_hash);
#ifdef ZTS
	tsrm_mutex_free(http_persistent_handles_lock);
#endif
	return SUCCESS;
}

PHP_HTTP_API STATUS _http_persistent_handle_provide_ex(const char *name_str, size_t name_len, http_persistent_handle_ctor ctor, http_persistent_handle_dtor dtor)
{
	STATUS status = FAILURE;
	http_persistent_handle_provider provider;
	
	LOCK();
	provider.list = pemalloc(sizeof(HashTable), 1);
	if (provider.list) {
		provider.ctor = ctor;
		provider.dtor = dtor;
		zend_hash_init(provider.list, 0, NULL, NULL, 1);
		
#if HTTP_DEBUG_PHANDLES
		fprintf(stderr, "PROVIDE: %p (%s)\n", provider.list, name_str);
#endif
		
		if (SUCCESS == zend_hash_add(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider, sizeof(http_persistent_handle_provider), NULL)) {
			status = SUCCESS;
		} else {
			pefree(provider.list, 1);
		}
	}
	UNLOCK();
	
	return status;
}

PHP_HTTP_API STATUS _http_persistent_handle_acquire_ex(const char *name_str, size_t name_len, void **handle_ptr TSRMLS_DC)
{
	STATUS status = FAILURE;
	ulong index;
	http_persistent_handle_provider *provider;
	http_persistent_handle_list *list;
	http_persistent_handle *handle;
	
	*handle_ptr = NULL;
	LOCK();
	if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider)) {
		if (SUCCESS == http_persistent_handle_list_find(provider->list, &list, 0 TSRMLS_CC)) {
			zend_hash_internal_pointer_end(*list);
			if (HASH_KEY_NON_EXISTANT != zend_hash_get_current_key(*list, NULL, &index, 0) && SUCCESS == zend_hash_get_current_data(*list, (void *) &handle)) {
				*handle_ptr = handle->ptr;
				zend_hash_index_del(*list, index);
			}
		}
		if (*handle_ptr || (*handle_ptr = provider->ctor())) {
			status = SUCCESS;
		}
	}
	UNLOCK();
	
#if HTTP_DEBUG_PHANDLES
	fprintf(stderr, "ACQUIRE: %p (%s)\n", *handle_ptr, name_str);
#endif
	
	return status;
}

PHP_HTTP_API STATUS _http_persistent_handle_release_ex(const char *name_str, size_t name_len, void **handle_ptr TSRMLS_DC)
{
	STATUS status = FAILURE;
	http_persistent_handle_provider *provider;
	http_persistent_handle_list *list;
	http_persistent_handle handle = {*handle_ptr};
	
	LOCK();
	if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider)) {
		if (	SUCCESS == http_persistent_handle_list_find(provider->list, &list, 1 TSRMLS_CC) &&
				SUCCESS == zend_hash_next_index_insert(*list, (void *) &handle, sizeof(http_persistent_handle), NULL)) {
			status = SUCCESS;
		}
	}
	UNLOCK();
	
#if HTTP_DEBUG_PHANDLES
	fprintf(stderr, "RELEASE: %p (%s)\n", *handle_ptr, name_str);
#endif
	
	return status;
}
	
PHP_HTTP_API void _http_persistent_handle_cleanup_ex(const char *name_str, size_t name_len, int current_ident_only TSRMLS_DC)
{
	http_persistent_handle_provider *provider;
	http_persistent_handle_list *list;
	HashPosition pos1, pos2;
	
	LOCK();
	if (name_str && name_len) {
		if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider)) {
			if (current_ident_only) {
				if (SUCCESS == http_persistent_handle_list_find(provider->list, &list, 0 TSRMLS_CC)) {
					http_persistent_handle_list_dtor(*list, provider->dtor);
				}
			} else {
				FOREACH_HASH_VAL(pos1, provider->list, list) {
					http_persistent_handle_list_dtor(*list, provider->dtor);
				}
			}
		}
	} else {
		FOREACH_HASH_VAL(pos1, &http_persistent_handles_hash, provider) {
			if (current_ident_only) {
				if (SUCCESS == http_persistent_handle_list_find(provider->list, &list, 0 TSRMLS_CC)) {
					http_persistent_handle_list_dtor(*list, provider->dtor);
				}
			} else {
				FOREACH_HASH_VAL(pos2, provider->list, list) {
					http_persistent_handle_list_dtor(*list, provider->dtor);
				}
			}
		}
	}
	UNLOCK();
}

PHP_HTTP_API HashTable *_http_persistent_handle_statall_ex(HashTable *ht)
{
	zval *zlist, *zentry;
	HashPosition pos1, pos2;
	HashKey key1 = initHashKey(0), key2 = initHashKey(0);
	http_persistent_handle_provider *provider;
	http_persistent_handle_list *list;
	
	LOCK();
	if (zend_hash_num_elements(&http_persistent_handles_hash)) {
		if (!ht) {
			ALLOC_HASHTABLE(ht);
			zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		}
		
		FOREACH_HASH_KEYVAL(pos1, &http_persistent_handles_hash, key1, provider) {
			MAKE_STD_ZVAL(zlist);
			array_init(zlist);
			FOREACH_HASH_KEYVAL(pos2, provider->list, key2, list) {
				MAKE_STD_ZVAL(zentry);
				ZVAL_LONG(zentry, zend_hash_num_elements(*list));
				zend_hash_add(Z_ARRVAL_P(zlist), key2.str, key2.len, (void *) &zentry, sizeof(zval *), NULL);
			}
			zend_hash_add(ht, key1.str, key1.len, (void *) &zlist, sizeof(zval *), NULL);
		}
	} else if (ht) {
		ht = NULL;
	}
	UNLOCK();
	
	return ht;
}

#endif /* HTTP_HAVE_PERSISTENT_HANDLES */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

