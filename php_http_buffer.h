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

#ifndef PHP_HTTP_BUFFER_H
#define PHP_HTTP_BUFFER_H

#ifndef PHP_HTTP_BUFFER_DEFAULT_SIZE
#	define PHP_HTTP_BUFFER_DEFAULT_SIZE 256
#endif

#define PHP_HTTP_BUFFER_ERROR ((size_t) -1)
#define PHP_HTTP_BUFFER_NOMEM PHP_HTTP_BUFFER_ERROR
#define PHP_HTTP_BUFFER_PASS0 PHP_HTTP_BUFFER_ERROR

#ifndef STR_FREE
#	define STR_FREE(STR) \
	{ \
		if (STR) { \
			efree(STR); \
		} \
	}
#endif
#ifndef STR_SET
#	define STR_SET(STR, SET) \
	{ \
		STR_FREE(STR); \
		STR = SET; \
	}
#endif
#ifndef TSRMLS_D
#	define TSRMLS_D
#	define TSRMLS_DC
#	define TSRMLS_CC
#	define TSRMLS_C
#endif
#ifdef PHP_ATTRIBUTE_FORMAT
#	define PHP_HTTP_BUFFER_ATTRIBUTE_FORMAT(f, a, b) PHP_ATTRIBUTE_FORMAT(f, a, b)
#else
#	define PHP_HTTP_BUFFER_ATTRIBUTE_FORMAT(f, a, b)
#endif
#ifndef pemalloc
#	define pemalloc(s,p)	malloc(s)
#	define pefree(x,p)		free(x)
#	define perealloc(x,s,p)	realloc(x,s)
#	define perealloc_recoverable perealloc
#	define ecalloc calloc
static inline void *estrndup(void *p, size_t s)
{
	char *r = (char *) malloc(s+1);
	if (r) memcpy((void *) r, p, s), r[s] = '\0';
	return (void *) r;
}
#endif

#if defined(PHP_WIN32)
#	if defined(PHP_HTTP_BUFFER_EXPORTS)
#		define PHP_HTTP_BUFFER_API __declspec(dllexport)
#	elif defined(COMPILE_DL_PHP_HTTP_BUFFER)
#		define PHP_HTTP_BUFFER_API __declspec(dllimport)
#	else
#		define PHP_HTTP_BUFFER_API
#	endif
#else
#	define PHP_HTTP_BUFFER_API
#endif

#define PHP_HTTP_BUFFER(p) ((php_http_buffer_t *) (p))

#define FREE_PHP_HTTP_BUFFER_PTR(STR) pefree(STR, STR->pmem)
#define FREE_PHP_HTTP_BUFFER_VAL(STR) php_http_buffer_dtor(STR)
#define FREE_PHP_HTTP_BUFFER_ALL(STR) php_http_buffer_free(&(STR))
#define FREE_PHP_HTTP_BUFFER(free, STR) \
	switch (free) \
	{ \
	case PHP_HTTP_BUFFER_FREE_NOT: \
		break; \
	case PHP_HTTP_BUFFER_FREE_PTR: \
		pefree(STR, STR->pmem);	break; \
		break; \
	case PHP_HTTP_BUFFER_FREE_VAL: \
		php_http_buffer_dtor(STR); \
		break; \
	case PHP_HTTP_BUFFER_FREE_ALL: { \
		php_http_buffer_t *PTR = (STR); \
		php_http_buffer_free(&PTR); \
		break; \
	} \
	default:\
		break; \
	}

#define RETURN_PHP_HTTP_BUFFER_PTR(STR) RETURN_PHP_HTTP_BUFFER((STR), PHP_HTTP_BUFFER_FREE_PTR, 0)
#define RETURN_PHP_HTTP_BUFFER_VAL(STR) RETURN_PHP_HTTP_BUFFER((STR), PHP_HTTP_BUFFER_FREE_NOT, 0)
#define RETURN_PHP_HTTP_BUFFER_DUP(STR) RETURN_PHP_HTTP_BUFFER((STR), PHP_HTTP_BUFFER_FREE_NOT, 1)
#define RETVAL_PHP_HTTP_BUFFER_PTR(STR) RETVAL_PHP_HTTP_BUFFER((STR), PHP_HTTP_BUFFER_FREE_PTR, 0)
#define RETVAL_PHP_HTTP_BUFFER_VAL(STR) RETVAL_PHP_HTTP_BUFFER((STR), PHP_HTTP_BUFFER_FREE_NOT, 0)
#define RETVAL_PHP_HTTP_BUFFER_DUP(STR) RETVAL_PHP_HTTP_BUFFER((STR), PHP_HTTP_BUFFER_FREE_NOT, 1)
/* RETURN_PHP_HTTP_BUFFER(buf, PHP_HTTP_BUFFER_FREE_PTR, 0) */
#define RETURN_PHP_HTTP_BUFFER(STR, free, dup) \
	RETVAL_PHP_HTTP_BUFFER((STR), (free), (dup)); \
	return;

#define RETVAL_PHP_HTTP_BUFFER(STR, free, dup) \
	php_http_buffer_fix(STR); \
	RETVAL_STRINGL((STR)->data, (STR)->used, (dup)); \
	FREE_PHP_HTTP_BUFFER((free), (STR));

typedef struct php_http_buffer {
	char  *data;
	size_t used;
	size_t free;
	size_t size;
	unsigned pmem:1;
	unsigned reserved:31;
} php_http_buffer_t;

typedef enum php_http_buffer_free {
	PHP_HTTP_BUFFER_FREE_NOT = 0,
	PHP_HTTP_BUFFER_FREE_PTR,	/* pefree() */
	PHP_HTTP_BUFFER_FREE_VAL,	/* php_http_buffer_dtor() */
	PHP_HTTP_BUFFER_FREE_ALL	/* php_http_buffer_free() */
} php_http_buffer_free_t;

#define PHP_HTTP_BUFFER_ALL_FREE(STR) PHP_HTTP_BUFFER_FREE_ALL,(STR)
#define PHP_HTTP_BUFFER_PTR_FREE(STR) PHP_HTTP_BUFFER_FREE_PTR,(STR)
#define PHP_HTTP_BUFFER_VAL_FREE(STR) PHP_HTTP_BUFFER_FREE_VAL,(STR)
#define PHP_HTTP_BUFFER_NOT_FREE(STR) PHP_HTTP_BUFFER_FREE_NOT,(STR)

#define PHP_HTTP_BUFFER_INIT_PREALLOC	0x01
#define PHP_HTTP_BUFFER_INIT_PERSISTENT	0x02

/* create a new php_http_buffer_t */
#define php_http_buffer_new() php_http_buffer_init(NULL)
#define php_http_buffer_init(b) php_http_buffer_init_ex(b, PHP_HTTP_BUFFER_DEFAULT_SIZE, 0)
#define php_http_buffer_clone(from, to) php_http_buffer_init_ex((to), (from)->size, (from)->pmem ? PHP_HTTP_BUFFER_INIT_PERSISTENT:0)
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_init_ex(php_http_buffer_t *buf, size_t chunk_size, int flags);

/* create a php_http_buffer_t from a zval or c-string */
#define php_http_buffer_from_zval(z) php_http_buffer_from_string(Z_STRVAL(z), Z_STRLEN(z))
#define php_http_buffer_from_zval_ex(b, z) php_http_buffer_from_string_ex(b, Z_STRVAL(z), Z_STRLEN(z))
#define php_http_buffer_from_string(s, l) php_http_buffer_from_string_ex(NULL, (s), (l))
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_from_string_ex(php_http_buffer_t *buf, const char *string, size_t length);

/* usually only called from within the internal functions */
#define php_http_buffer_resize(b, s) php_http_buffer_resize_ex((b), (s), 0, 0)
PHP_HTTP_BUFFER_API size_t php_http_buffer_resize_ex(php_http_buffer_t *buf, size_t len, size_t override_size, int allow_error);

PHP_HTTP_BUFFER_API char *php_http_buffer_account(php_http_buffer_t *buf, size_t to_account);

/* shrink memory chunk to actually used size (+1) */
PHP_HTTP_BUFFER_API size_t php_http_buffer_shrink(php_http_buffer_t *buf);

/* append data to the php_http_buffer_t */
#define php_http_buffer_appends(b, a) php_http_buffer_append((b), (a), sizeof(a)-1)
#define php_http_buffer_appendl(b, a) php_http_buffer_append((b), (a), strlen(a))
PHP_HTTP_BUFFER_API size_t php_http_buffer_append(php_http_buffer_t *buf, const char *append, size_t append_len);
PHP_HTTP_BUFFER_API size_t php_http_buffer_appendf(php_http_buffer_t *buf, const char *format, ...) PHP_HTTP_BUFFER_ATTRIBUTE_FORMAT(printf, 2, 3);

/* get a zero-terminated string */
PHP_HTTP_BUFFER_API char *php_http_buffer_data(const php_http_buffer_t *buf, char **into, size_t *len);

/* remove a substring */
PHP_HTTP_BUFFER_API size_t php_http_buffer_cut(php_http_buffer_t *buf, size_t offset, size_t length);

/* sets a trailing NUL byte */
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_fix(php_http_buffer_t *buf);

/* reset php_http_buffer_t object */
PHP_HTTP_BUFFER_API void php_http_buffer_reset(php_http_buffer_t *buf);

/* free a php_http_buffer_t objects contents */
PHP_HTTP_BUFFER_API void php_http_buffer_dtor(php_http_buffer_t *buf);

/* free a php_http_buffer_t object completely */
PHP_HTTP_BUFFER_API void php_http_buffer_free(php_http_buffer_t **buf);

/* stores data in a php_http_buffer_t until it reaches chunk_size */
PHP_HTTP_BUFFER_API size_t php_http_buffer_chunk_buffer(php_http_buffer_t **s, const char *data, size_t data_len, char **chunk, size_t chunk_size);

typedef size_t (*php_http_buffer_pass_func_t)(void *opaque, char *, size_t TSRMLS_DC);

PHP_HTTP_BUFFER_API ssize_t php_http_buffer_passthru(php_http_buffer_t **s, size_t chunk_size, php_http_buffer_pass_func_t passin, void *passin_arg, php_http_buffer_pass_func_t passon, void *passon_arg TSRMLS_DC);

/* wrapper around php_http_buffer_chunk_buffer, which passes available chunks to passthru() */
PHP_HTTP_BUFFER_API void php_http_buffer_chunked_output(php_http_buffer_t **s, const char *data, size_t data_len, size_t chunk_size, php_http_buffer_pass_func_t passout, void *opaque TSRMLS_DC);

/* write chunks directly into php_http_buffer_t buffer */
PHP_HTTP_BUFFER_API size_t php_http_buffer_chunked_input(php_http_buffer_t **s, size_t chunk_size, php_http_buffer_pass_func_t passin, void *opaque TSRMLS_DC);


#	ifdef PHP_HTTP_BUFFER_EXTENDED

/* memcmp for php_http_buffer_t objects */
PHP_HTTP_BUFFER_API int php_http_buffer_cmp(php_http_buffer_t *left, php_http_buffer_t *right);

/* get a complete php_http_buffer_t duplicate */
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_copy(const php_http_buffer_t *from, php_http_buffer_t *to);

/* merge several php_http_buffer_t objects
   use like:

	php_http_buffer_t *final = php_http_buffer_merge(3,
		PHP_HTTP_BUFFER_NOT_FREE(&keep),
		PHP_HTTP_BUFFER_ALL_FREE(middle_ptr),
		PHP_HTTP_BUFFER_VAL_FREE(&local);
*/
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_merge(unsigned argc, ...);
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_merge_ex(php_http_buffer_t *buf, unsigned argc, ...);
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_merge_va(php_http_buffer_t *buf, unsigned argc, va_list argv);

/* insert data at a specific position of the php_http_buffer_t */
#define php_http_buffer_inserts(b, i, o) php_http_buffer_insert((b), (i), sizeof(i)-1, (o))
#define php_http_buffer_insertl(b, i, o) php_http_buffer_insert((b), (i), strlen(i), (o))
PHP_HTTP_BUFFER_API size_t php_http_buffer_insert(php_http_buffer_t *buf, const char *insert, size_t insert_len, size_t offset);
PHP_HTTP_BUFFER_API size_t php_http_buffer_insertf(php_http_buffer_t *buf, size_t offset, const char *format, ...) PHP_HTTP_BUFFER_ATTRIBUTE_FORMAT(printf, 3, 4);

/* prepend data */
#define php_http_buffer_prepends(b, p) php_http_buffer_prepend((b), (p), sizeof(p)-1)
#define php_http_buffer_prependl(b, p) php_http_buffer_prepend((b), (p), strlen(p))
PHP_HTTP_BUFFER_API size_t php_http_buffer_prepend(php_http_buffer_t *buf, const char *prepend, size_t prepend_len);
PHP_HTTP_BUFFER_API size_t php_http_buffer_prependf(php_http_buffer_t *buf, const char *format, ...) PHP_HTTP_BUFFER_ATTRIBUTE_FORMAT(printf, 2, 3);

/* get a part of the php_http_buffer_t */
#define php_http_buffer_mid(b, o, l) php_http_buffer_sub((b), (o), (l))
#define php_http_buffer_left(b, l) php_http_buffer_sub((b), 0, (l))
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_right(const php_http_buffer_t *buf, size_t length);
PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_sub(const php_http_buffer_t *buf, size_t offset, size_t len);

#	endif /* PHP_HTTP_BUFFER_EXTENDED */

#endif /* PHP_HTTP_BUFFER_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
