/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#include "php_http.h"
#include "php_http_api.h"

#ifdef HTTP_HAVE_PERSISTENT_HANDLES
#include "php_http_persistent_handle_api.h"

static HashTable http_persistent_handles_hash;
#ifdef ZTS
#	define LOCK() tsrm_mutex_lock(http_persistent_handles_lock)
#	define UNLOCK() tsrm_mutex_unlock(http_persistent_handles_lock)
static MUTEX_T http_persistent_handles_lock;
#else
#	define LOCK()
#	define UNLOCK()
#endif

typedef struct _http_persistent_handles_hash_entry_t {
	HashTable list;
	http_persistent_handle_ctor ctor;
	http_persistent_handle_dtor dtor;
} http_persistent_handles_hash_entry;

typedef struct _http_persistent_handles_list_entry_t {
	void *handle;
} http_persistent_handles_list_entry;

static inline void http_persistent_handles_hash_dtor_ex(http_persistent_handles_hash_entry *hentry, void (*list_dtor)(HashTable*))
{
	http_persistent_handles_list_entry *lentry;
	
	for (	zend_hash_internal_pointer_reset(&hentry->list);
			SUCCESS == zend_hash_get_current_data(&hentry->list, (void *) &lentry);
			zend_hash_move_forward(&hentry->list)) {
		hentry->dtor(lentry->handle);
	}
	
	if (list_dtor) {
		list_dtor(&hentry->list);
	}
}

static void http_persistent_handles_hash_dtor(void *h)
{
	http_persistent_handles_hash_dtor_ex(h, zend_hash_destroy);
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
	STATUS status = SUCCESS;
	http_persistent_handles_hash_entry e;
	
	zend_hash_init(&e.list, 0, NULL, NULL, 1);
	e.ctor = ctor;
	e.dtor = dtor;
	
	LOCK();
	if (SUCCESS != zend_hash_add(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &e, sizeof(http_persistent_handles_hash_entry), NULL)) {
		zend_hash_destroy(&e.list);
		status = FAILURE;
	}
	UNLOCK();
	
	return status;
}

PHP_HTTP_API void _http_persistent_handle_cleanup_ex(const char *name_str, size_t name_len)
{
	http_persistent_handles_hash_entry *hentry;
	
	LOCK();
	if (name_str && name_len) {
		if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &hentry)) {
			http_persistent_handles_hash_dtor_ex(hentry, zend_hash_clean);
		}
	} else {
		for (	zend_hash_internal_pointer_reset(&http_persistent_handles_hash);
				SUCCESS == zend_hash_get_current_data(&http_persistent_handles_hash, (void *) &hentry);
				zend_hash_move_forward(&http_persistent_handles_hash)) {
			http_persistent_handles_hash_dtor_ex(hentry, zend_hash_clean);
		}
	}
	UNLOCK();
}

PHP_HTTP_API HashTable *_http_persistent_handle_statall_ex(HashTable *ht)
{
	zval *tmp;
	HashPosition pos;
	HashKey key = initHashKey(0);
	http_persistent_handles_hash_entry *hentry;
	
	LOCK();
	if (zend_hash_num_elements(&http_persistent_handles_hash)) {
		if (!ht) {
			ALLOC_HASHTABLE(ht);
			zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		}
		
		FOREACH_HASH_KEYVAL(pos, &http_persistent_handles_hash, key, hentry) {
			MAKE_STD_ZVAL(tmp);
			ZVAL_LONG(tmp, zend_hash_num_elements(&hentry->list));
			zend_hash_add(ht, key.str, key.len, (void *) &tmp, sizeof(zval *), NULL);
		}
	} else if (ht) {
		ht = NULL;
	}
	UNLOCK();
	
	return ht;
}

PHP_HTTP_API STATUS _http_persistent_handle_acquire_ex(const char *name_str, size_t name_len, void **handle)
{
	STATUS status = FAILURE;
	ulong index;
	http_persistent_handles_hash_entry *hentry;
	http_persistent_handles_list_entry *lentry;
	
	LOCK();
	if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &hentry)) {
		zend_hash_internal_pointer_end(&hentry->list);
		if (	HASH_KEY_NON_EXISTANT != zend_hash_get_current_key(&hentry->list, NULL, &index, 0) &&
				SUCCESS == zend_hash_get_current_data(&hentry->list, (void *) &lentry)) {
			*handle = lentry->handle;
			zend_hash_index_del(&hentry->list, index);
			status = SUCCESS;
		} else if ((*handle = hentry->ctor())) {
			status = SUCCESS;
		}
	}
	UNLOCK();
	
	return status;
}

PHP_HTTP_API STATUS _http_persistent_handle_release_ex(const char *name_str, size_t name_len, void **handle)
{
	STATUS status = FAILURE;
	http_persistent_handles_hash_entry *hentry;
	http_persistent_handles_list_entry lentry;
	
	LOCK();
	if (SUCCESS == zend_hash_find(&http_persistent_handles_hash, (char *) name_str, name_len+1, (void *) &hentry)) {
		lentry.handle = *handle;
		if (SUCCESS == zend_hash_next_index_insert(&hentry->list, (void *) &lentry, sizeof(http_persistent_handles_list_entry), NULL)) {
			status = SUCCESS;
		}
	}
	UNLOCK();
	
	return status;
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

