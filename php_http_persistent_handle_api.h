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

#ifndef HTTP_PERSISTENT_HANDLE_H
#define HTTP_PERSISTENT_HANDLE_H

#ifdef HTTP_HAVE_PERSISTENT_HANDLES
typedef void *(*http_persistent_handle_ctor)(void);
typedef void (*http_persistent_handle_dtor)(void *handle);

PHP_MINIT_FUNCTION(http_persistent_handle);
PHP_MSHUTDOWN_FUNCTION(http_persistent_handle);

#define http_persistent_handle_provide(n, c, d) _http_persistent_handle_provide_ex((n), strlen(n), (c), (d))
#define http_persistent_handle_provide_ex(n, l, c, d) _http_persistent_handle_provide_ex((n), (l), (c), (d))
PHP_HTTP_API STATUS _http_persistent_handle_provide_ex(const char *name_str, size_t name_len, http_persistent_handle_ctor ctor, http_persistent_handle_dtor dtor);

#define http_persistent_handle_cleanup(n) _http_persistent_handle_cleanup_ex((n), strlen(n))
#define http_persistent_handle_cleanup_ex(n, l) _http_persistent_handle_cleanup_ex((n), (l))
PHP_HTTP_API void _http_persistent_handle_cleanup_ex(const char *name_str, size_t name_len);

#define http_persistent_handle_statall(n, c) _http_persistent_handle_statall_ex((n), (c), 0)
#define http_persistent_handle_statall_ex(n, c, p) _http_persistent_handle_statall_ex((n), (c), (p))
PHP_HTTP_API int _http_persistent_handle_statall_ex(char ***names, int **counts, int persistent);

#define http_persistent_handle_acquire(n, h) _http_persistent_handle_acquire_ex((n), strlen(n), (h))
#define http_persistent_handle_acquire_ex(n, l, h) _http_persistent_handle_acquire_ex((n), (l), (h))
PHP_HTTP_API STATUS _http_persistent_handle_acquire_ex(const char *name_str, size_t name_len, void **handle);

#define http_persistent_handle_release(n, h) _http_persistent_handle_release_ex((n), strlen(n), (h))
#define http_persistent_handle_release_ex(n, l, h) _http_persistent_handle_release_ex((n), (l), (h))
PHP_HTTP_API STATUS _http_persistent_handle_release_ex(const char *name_str, size_t name_len, void **handle);

#endif /* HTTP_HAVE_PERSISTENT_HANDLES */
#endif /* HTTP_PERSISTENT_HANDLE_H */
