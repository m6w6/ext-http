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

#include "php_http_persistent_handle_api.h"

#ifndef HTTP_DEBUG_PHANDLES
#	define HTTP_DEBUG_PHANDLES 0
#endif
#if HTTP_DEBUG_PHANDLES
#	undef inline
#	define inline
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

typedef struct _http_persistent_handle_list_t {
	HashTable free;
	ulong used;
} http_persistent_handle_list;

typedef struct _http_persistent_handle_provider_t {
	http_persistent_handle_list list; /* "ident" => array(handles) entries */
	http_persistent_handle_ctor ctor;
	http_persistent_handle_dtor dtor;
	http_persistent_handle_copy copy;
} http_persistent_handle_provider;

static inline http_persistent_handle_list *http_persistent_handle_list_init(http_persistent_handle_list *list)
{
	int free_list;
	
	if ((free_list = !list)) {
		list = pemalloc(sizeof(http_persistent_handle_list), 1);
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

static inline void http_persistent_handle_list_dtor(http_persistent_handle_list *list, http_persistent_handle_dtor dtor)
{
	HashPosition pos;
	void **handle;
	
#if HTTP_DEBUG_PHANDLES
	fprintf(stderr, "LSTDTOR: %p\n", list);
#endif
	FOREACH_HASH_VAL(pos, &list->free, handle) {
#if HTTP_DEBUG_PHANDLES
		fprintf(stderr, "DESTROY: %p\n", *handle);
#endif
		
		dtor(*handle);
	}
	zend_hash_destroy(&list->free);
}

static inline void http_persistent_handle_list_free(http_persistent_handle_list **list, http_persistent_handle_dtor dtor)
{
	http_persistent_handle_list_dtor(*list, dtor);
#if HTTP_DEBUG_PHANDLES
	fprintf(stderr, "LSTFREE: %p\n", *list);
#endif
	pefree(*list, 1);
	*list = NULL;
}

static inline http_persistent_handle_list *http_persistent_handle_list_find(http_persistent_handle_provider *provider TSRMLS_DC)
{
	http_persistent_handle_list **list, *new_list;
	
	if (SUCCESS == zend_hash_quick_find(&provider->list.free, HTTP_G->persistent.handles.ident.s, HTTP_G->persistent.handles.ident.l, HTTP_G->persistent.handles.ident.h, (void *) &list)) {
#if HTTP_DEBUG_PHANDLES
		fprintf(stderr, "LSTFIND: %p\n", *list);
#endif
		return *list;
	}
	
	if ((new_list = http_persistent_handle_list_init(NULL))) {
		if (SUCCESS == zend_hash_quick_add(&provider->list.free, HTTP_G->persistent.handles.ident.s, HTTP_G->persistent.handles.ident.l, HTTP_G->persistent.handles.ident.h, (void *) &new_list, sizeof(http_persistent_handle_list *), (void *) &list)) {
#if HTTP_DEBUG_PHANDLES
			fprintf(stderr, "LSTFIND: %p (new)\n", *list);
#endif
			return *list;
		}
		http_persistent_handle_list_free(&new_list, provider->dtor);
	}
	
	return NULL;
}

static inline STATUS http_persistent_handle_do_acquire(http_persistent_handle_provider *provider, void **handle TSRMLS_DC)
{
	ulong index;
	void **handle_ptr;
	http_persistent_handle_list *list;
	
	if ((list = http_persistent_handle_list_find(provider TSRMLS_CC))) {
		zend_hash_internal_pointer_end(&list->free);
		if (HASH_KEY_NON_EXISTANT != zend_hash_get_current_key(&list->free, NULL, &index, 0) && SUCCESS == zend_hash_get_current_data(&list->free, (void *) &handle_ptr)) {
			*handle = *handle_ptr;
			zend_hash_index_del(&list->free, index);
		} else {
			*handle = provider->ctor();
		}
		
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

static inline STATUS http_persistent_handle_do_release(http_persistent_handle_provider *provider, void **handle TSRMLS_DC)
{
	http_persistent_handle_list *list;
	
	if ((list = http_persistent_handle_list_find(provider TSRMLS_CC))) {
		if (provider->list.used >= HTTP_G->persistent.handles.limit) {
			provider->dtor(*handle);
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

static inline STATUS http_persistent_handle_do_accrete(http_persistent_handle_provider *provider, void *old_handle, void **new_handle TSRMLS_DC)
{
	http_persistent_handle_list *list;
	
	if (provider->copy && (*new_handle = provider->copy(old_handle))) {
		if ((list = http_persistent_handle_list_find(provider TSRMLS_CC))) {
			++list->used;
		}
		++provider->list.used;
		return SUCCESS;
	}
	return FAILURE;
}

static void http_persistent_handles_hash_dtor(void *p)
{
	http_persistent_handle_provider *provider = (http_persistent_handle_provider *) p;
	http_persistent_handle_list **list, *list_tmp;
	HashPosition pos;
	
	FOREACH_HASH_VAL(pos, &provider->list.free, list) {
		/* fix shutdown crash in PHP4 */
		list_tmp = *list;
		http_persistent_handle_list_free(&list_tmp, provider->dtor);
	}
	
	zend_hash_destroy(&provider->list.free);
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

PHP_HTTP_API STATUS _http_persistent_handle_provide_ex(const char *name_str, size_t name_len, http_persistent_handle_ctor ctor, http_persistent_handle_dtor dtor, http_persistent_handle_copy copy)
{
	STATUS status = FAILURE;
	http_persistent_handle_provider provider;
	
	LOCK();
	if (http_persistent_handle_list_init(&provider.list)) {
		provider.ctor = ctor;
		provider.dtor = dtor;
		provider.copy = copy;
		
#if HTTP_DEBUG_PHANDLES
		fprintf(stderr, "PROVIDE: %s\n", name_str);
#endif
		
		if (SUCCESS == zend_hash_add(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider, sizeof(http_persistent_handle_provider), NULL)) {
			status = SUCCESS;
		}
	}
	UNLOCK();
	
	return status;
}

PHP_HTTP_API STATUS _http_persistent_handle_acquire_ex(const char *name_str, size_t name_len, void **handle TSRMLS_DC)
{
	STATUS status = FAILURE;
	http_persistent_handle_provider *provider;
	
	*handle = NULL;
	LOCK();
	if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider)) {
		status = http_persistent_handle_do_acquire(provider, handle TSRMLS_CC);
	}
	UNLOCK();
	
#if HTTP_DEBUG_PHANDLES
	fprintf(stderr, "ACQUIRE: %p (%s)\n", *handle, name_str);
#endif
	
	return status;
}

PHP_HTTP_API STATUS _http_persistent_handle_release_ex(const char *name_str, size_t name_len, void **handle TSRMLS_DC)
{
	STATUS status = FAILURE;
	http_persistent_handle_provider *provider;
#if HTTP_DEBUG_PHANDLES
	void *handle_tmp = *handle;
#endif
	
	LOCK();
	if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider)) {
		status = http_persistent_handle_do_release(provider, handle TSRMLS_CC);
	}
	UNLOCK();
	
#if HTTP_DEBUG_PHANDLES
	fprintf(stderr, "RELEASE: %p (%s)\n", handle_tmp, name_str);
#endif
	
	return status;
}

PHP_HTTP_API STATUS _http_persistent_handle_accrete_ex(const char *name_str, size_t name_len, void *old_handle, void **new_handle TSRMLS_DC)
{
	STATUS status = FAILURE;
	http_persistent_handle_provider *provider;
	
	*new_handle = NULL;
	LOCK();
	if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider)) {
		status = http_persistent_handle_do_accrete(provider, old_handle, new_handle TSRMLS_CC);
	}
	UNLOCK();
	
#if HTTP_DEBUG_PHANDLES
	fprintf(stderr, "ACCRETE: %p > %p (%s)\n", old_handle, *new_handle, name_str);
#endif
	
	return status;
}

PHP_HTTP_API void _http_persistent_handle_cleanup_ex(const char *name_str, size_t name_len, int current_ident_only TSRMLS_DC)
{
	http_persistent_handle_provider *provider;
	http_persistent_handle_list *list, **listp;
	HashPosition pos1, pos2;
	
	LOCK();
	if (name_str && name_len) {
		if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &provider)) {
			if (current_ident_only) {
				if ((list = http_persistent_handle_list_find(provider TSRMLS_CC))) {
					http_persistent_handle_list_dtor(list, provider->dtor);
					http_persistent_handle_list_init(list);
				}
			} else {
				FOREACH_HASH_VAL(pos1, &provider->list.free, listp) {
					http_persistent_handle_list_dtor(*listp, provider->dtor);
					http_persistent_handle_list_init(*listp);
				}
			}
		}
	} else {
		FOREACH_HASH_VAL(pos1, &http_persistent_handles_hash, provider) {
			if (current_ident_only) {
				if ((list = http_persistent_handle_list_find(provider TSRMLS_CC))) {
					http_persistent_handle_list_dtor(list, provider->dtor);
					http_persistent_handle_list_init(list);
				}
			} else {
				FOREACH_HASH_VAL(pos2, &provider->list.free, listp) {
					http_persistent_handle_list_dtor(*listp, provider->dtor);
					http_persistent_handle_list_init(*listp);
				}
			}
		}
	}
	UNLOCK();
}

PHP_HTTP_API HashTable *_http_persistent_handle_statall_ex(HashTable *ht TSRMLS_DC)
{
	zval *zentry[2];
	HashPosition pos1, pos2;
	HashKey key1 = initHashKey(0), key2 = initHashKey(0);
	http_persistent_handle_provider *provider;
	http_persistent_handle_list **list;
	
	LOCK();
	if (zend_hash_num_elements(&http_persistent_handles_hash)) {
		if (!ht) {
			ALLOC_HASHTABLE(ht);
			zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		}
		
		FOREACH_HASH_KEYVAL(pos1, &http_persistent_handles_hash, key1, provider) {
			MAKE_STD_ZVAL(zentry[0]);
			array_init(zentry[0]);
			
			FOREACH_HASH_KEYVAL(pos2, &provider->list.free, key2, list) {
				MAKE_STD_ZVAL(zentry[1]);
				array_init(zentry[1]);
				add_assoc_long_ex(zentry[1], ZEND_STRS("used"), (*list)->used);
				add_assoc_long_ex(zentry[1], ZEND_STRS("free"), zend_hash_num_elements(&(*list)->free));
				
				/* use zend_hash_* not add_assoc_* (which is zend_symtable_*) as we want a string even for numbers */
				zend_hash_add(Z_ARRVAL_P(zentry[0]), key2.str, key2.len, &zentry[1], sizeof(zval *), NULL);
			}
			
			zend_hash_add(ht, key1.str, key1.len, &zentry[0], sizeof(zval *), NULL);
		}
	} else if (ht) {
		ht = NULL;
	}
	UNLOCK();
	
	return ht;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

