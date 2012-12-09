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

#include <php.h>
#include "php_http_buffer.h"

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_init_ex(php_http_buffer_t *buf, size_t chunk_size, int flags)
{
	if (!buf) {
		buf = pemalloc(sizeof(*buf), flags & PHP_HTTP_BUFFER_INIT_PERSISTENT);
	}

	if (buf) {
		buf->size = (chunk_size) ? chunk_size : PHP_HTTP_BUFFER_DEFAULT_SIZE;
		buf->pmem = (flags & PHP_HTTP_BUFFER_INIT_PERSISTENT) ? 1 : 0;
		buf->data = (flags & PHP_HTTP_BUFFER_INIT_PREALLOC) ? pemalloc(buf->size, buf->pmem) : NULL;
		buf->free = (flags & PHP_HTTP_BUFFER_INIT_PREALLOC) ? buf->size : 0;
		buf->used = 0;
	}
	
	return buf;
}

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_from_string_ex(php_http_buffer_t *buf, const char *string, size_t length)
{
	if ((buf = php_http_buffer_init(buf))) {
		if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_append(buf, string, length)) {
			pefree(buf, buf->pmem);
			buf = NULL;
		}
	}
	return buf;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_resize_ex(php_http_buffer_t *buf, size_t len, size_t override_size, int allow_error)
{
	char *ptr = NULL;
#if 0
	fprintf(stderr, "RESIZE: len=%lu, size=%lu, used=%lu, free=%lu\n", len, buf->size, buf->used, buf->free);
#endif
	if (buf->free < len) {
		size_t size = override_size ? override_size : buf->size;
		
		while ((size + buf->free) < len) {
			size <<= 1;
		}
		
		if (allow_error) {
			ptr = perealloc_recoverable(buf->data, buf->used + buf->free + size, buf->pmem);
		} else {
			ptr = perealloc(buf->data, buf->used + buf->free + size, buf->pmem);
		}
		
		if (ptr) {
			buf->data = ptr;
		} else {
			return PHP_HTTP_BUFFER_NOMEM;
		}
		
		buf->free += size;
		return size;
	}
	return 0;
}

PHP_HTTP_BUFFER_API char *php_http_buffer_account(php_http_buffer_t *buf, size_t to_account)
{
	/* it's probably already too late but check anyway */
	if (to_account > buf->free) {
		return NULL;
	}

	buf->free -= to_account;
	buf->used += to_account;

	return buf->data + buf->used;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_shrink(php_http_buffer_t *buf)
{
	/* avoid another realloc on fixation */
	if (buf->free > 1) {
		char *ptr = perealloc(buf->data, buf->used + 1, buf->pmem);
		
		if (ptr) {
			buf->data = ptr;
		} else {
			return PHP_HTTP_BUFFER_NOMEM;
		}
		buf->free = 1;
	}
	return buf->used;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_append(php_http_buffer_t *buf, const char *append, size_t append_len)
{
	if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_resize(buf, append_len)) {
		return PHP_HTTP_BUFFER_NOMEM;
	}
	memcpy(buf->data + buf->used, append, append_len);
	buf->used += append_len;
	buf->free -= append_len;
	return append_len;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_appendf(php_http_buffer_t *buf, const char *format, ...)
{
	va_list argv;
	char *append;
	size_t append_len, alloc;

	va_start(argv, format);
	append_len = vspprintf(&append, 0, format, argv);
	va_end(argv);

	alloc = php_http_buffer_append(buf, append, append_len);
	efree(append);

	if (PHP_HTTP_BUFFER_NOMEM == alloc) {
		return PHP_HTTP_BUFFER_NOMEM;
	}
	return append_len;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_insert(php_http_buffer_t *buf, const char *insert, size_t insert_len, size_t offset)
{
	if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_resize(buf, insert_len)) {
		return PHP_HTTP_BUFFER_NOMEM;
	}
	memmove(buf->data + offset + insert_len, buf->data + offset, insert_len);
	memcpy(buf->data + offset, insert, insert_len);
	buf->used += insert_len;
	buf->free -= insert_len;
	return insert_len;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_insertf(php_http_buffer_t *buf, size_t offset, const char *format, ...)
{
	va_list argv;
	char *insert;
	size_t insert_len, alloc;

	va_start(argv, format);
	insert_len = vspprintf(&insert, 0, format, argv);
	va_end(argv);

	alloc = php_http_buffer_insert(buf, insert, insert_len, offset);
	efree(insert);

	if (PHP_HTTP_BUFFER_NOMEM == alloc) {
		return PHP_HTTP_BUFFER_NOMEM;
	}
	return insert_len;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_prepend(php_http_buffer_t *buf, const char *prepend, size_t prepend_len)
{
	if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_resize(buf, prepend_len)) {
		return PHP_HTTP_BUFFER_NOMEM;
	}
	memmove(buf->data + prepend_len, buf->data, buf->used);
	memcpy(buf->data, prepend, prepend_len);
	buf->used += prepend_len;
	buf->free -= prepend_len;
	return prepend_len;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_prependf(php_http_buffer_t *buf, const char *format, ...)
{
	va_list argv;
	char *prepend;
	size_t prepend_len, alloc;

	va_start(argv, format);
	prepend_len = vspprintf(&prepend, 0, format, argv);
	va_end(argv);

	alloc = php_http_buffer_prepend(buf, prepend, prepend_len);
	efree(prepend);

	if (PHP_HTTP_BUFFER_NOMEM == alloc) {
		return PHP_HTTP_BUFFER_NOMEM;
	}
	return prepend_len;
}

PHP_HTTP_BUFFER_API char *php_http_buffer_data(const php_http_buffer_t *buf, char **into, size_t *len)
{
	char *copy = ecalloc(1, buf->used + 1);
	memcpy(copy, buf->data, buf->used);
	if (into) {
		*into = copy;
	}
	if (len) {
		*len = buf->used;
	}
	return copy;
}

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_copy(const php_http_buffer_t *from, php_http_buffer_t *to)
{
	int free_to = !to;

	to = php_http_buffer_clone(from, to);

	if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_append(to, from->data, from->used)) {
		if (free_to) {
			php_http_buffer_free(&to);
		} else {
			php_http_buffer_dtor(to);
		}
	}
	return to;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_cut(php_http_buffer_t *buf, size_t offset, size_t length)
{
	if (offset > buf->used) {
		return 0;
	}
	if (offset + length > buf->used) {
		length = buf->used - offset;
	}
	memmove(buf->data + offset, buf->data + offset + length, buf->used - length - offset);
	buf->used -= length;
	buf->free += length;
	return length;
}

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_sub(const php_http_buffer_t *buf, size_t offset, size_t length)
{
	if (offset >= buf->used) {
		return NULL;
	} else {
		size_t need = 1 + ((length + offset) > buf->used ? (buf->used - offset) : (length - offset));
		php_http_buffer_t *sub = php_http_buffer_init_ex(NULL, need, PHP_HTTP_BUFFER_INIT_PREALLOC | (buf->pmem ? PHP_HTTP_BUFFER_INIT_PERSISTENT:0));
		if (sub) {
			if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_append(sub, buf->data + offset, need)) {
				php_http_buffer_free(&sub);
			} else {
				sub->size = buf->size;
			}
		}
		return sub;
	}
}

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_right(const php_http_buffer_t *buf, size_t length)
{
	if (length < buf->used) {
		return php_http_buffer_sub(buf, buf->used - length, length);
	} else {
		return php_http_buffer_sub(buf, 0, buf->used);
	}
}


PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_merge_va(php_http_buffer_t *buf, unsigned argc, va_list argv)
{
	unsigned i = 0;
	buf = php_http_buffer_init(buf);

	if (buf) {
		while (argc > i++) {
			php_http_buffer_free_t f = va_arg(argv, php_http_buffer_free_t);
			php_http_buffer_t *current = va_arg(argv, php_http_buffer_t *);
			php_http_buffer_append(buf, current->data, current->used);
			FREE_PHP_HTTP_BUFFER(f, current);
		}
	}

	return buf;
}

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_merge_ex(php_http_buffer_t *buf, unsigned argc, ...)
{
	va_list argv;
	php_http_buffer_t *ret;

	va_start(argv, argc);
	ret = php_http_buffer_merge_va(buf, argc, argv);
	va_end(argv);
	return ret;
}

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_merge(unsigned argc, ...)
{
	va_list argv;
	php_http_buffer_t *ret;

	va_start(argv, argc);
	ret = php_http_buffer_merge_va(NULL, argc, argv);
	va_end(argv);
	return ret;
}

PHP_HTTP_BUFFER_API php_http_buffer_t *php_http_buffer_fix(php_http_buffer_t *buf)
{
	if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_resize_ex(buf, 1, 1, 0)) {
		return NULL;
	}
	buf->data[buf->used] = '\0';
	return buf;
}

PHP_HTTP_BUFFER_API int php_http_buffer_cmp(php_http_buffer_t *left, php_http_buffer_t *right)
{
	if (left->used > right->used) {
		return -1;
	} else if (right->used > left->used) {
		return 1;
	} else {
		return memcmp(left->data, right->data, left->used);
	}
}

PHP_HTTP_BUFFER_API void php_http_buffer_reset(php_http_buffer_t *buf)
{
	buf->free += buf->used;
	buf->used = 0;
}

PHP_HTTP_BUFFER_API void php_http_buffer_dtor(php_http_buffer_t *buf)
{
	if (buf->data) {
		pefree(buf->data, buf->pmem);
		buf->data = NULL;
	}
	buf->used = 0;
	buf->free = 0;
}

PHP_HTTP_BUFFER_API void php_http_buffer_free(php_http_buffer_t **buf)
{
	if (*buf) {
		php_http_buffer_dtor(*buf);
		pefree(*buf, (*buf)->pmem);
		*buf = NULL;
	}
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_chunk_buffer(php_http_buffer_t **s, const char *data, size_t data_len, char **chunk, size_t chunk_size)
{
	php_http_buffer_t *storage;
	
	*chunk = NULL;
	
	if (!*s) {
		*s = php_http_buffer_init_ex(NULL, chunk_size << 1, chunk_size ? PHP_HTTP_BUFFER_INIT_PREALLOC : 0);
	}
	storage = *s;
	
	if (data_len) {
		php_http_buffer_append(storage, data, data_len);
	}
	
	if (!chunk_size) {
		php_http_buffer_data(storage, chunk, &chunk_size);
		php_http_buffer_free(s);
		return chunk_size;
	}
	
	if (storage->used >= chunk_size) {
		*chunk = estrndup(storage->data, chunk_size);
		php_http_buffer_cut(storage, 0, chunk_size);
		return chunk_size;
	}
	
	return 0;
}

PHP_HTTP_BUFFER_API void php_http_buffer_chunked_output(php_http_buffer_t **s, const char *data, size_t data_len, size_t chunk_len, php_http_buffer_pass_func_t passout, void *opaque TSRMLS_DC)
{
	char *chunk = NULL;
	size_t got = 0;

	while ((got = php_http_buffer_chunk_buffer(s, data, data_len, &chunk, chunk_len))) {
		passout(opaque, chunk, got TSRMLS_CC);
		if (!chunk_len) {
			/* 	we already got the last chunk,
				and freed all resources */
			break;
		}
		data = NULL;
		data_len = 0;
		STR_SET(chunk, NULL);
	}
	STR_FREE(chunk);
}

PHP_HTTP_BUFFER_API ssize_t php_http_buffer_passthru(php_http_buffer_t **s, size_t chunk_size, php_http_buffer_pass_func_t passin, void *passin_arg, php_http_buffer_pass_func_t passon, void *passon_arg TSRMLS_DC)
{
	size_t passed_on = 0, passed_in = php_http_buffer_chunked_input(s, chunk_size, passin, passin_arg TSRMLS_CC);

	if (passed_in == PHP_HTTP_BUFFER_PASS0) {
		return passed_in;
	}
	if (passed_in || (*s)->used) {
		passed_on = passon(passon_arg, (*s)->data, (*s)->used TSRMLS_CC);

		if (passed_on == PHP_HTTP_BUFFER_PASS0) {
			return passed_on;
		}

		if (passed_on) {
			php_http_buffer_cut(*s, 0, passed_on);
		}
	}

	return passed_on - passed_in;
}

PHP_HTTP_BUFFER_API size_t php_http_buffer_chunked_input(php_http_buffer_t **s, size_t chunk_size, php_http_buffer_pass_func_t passin, void *opaque TSRMLS_DC)
{
	php_http_buffer_t *str;
	size_t passed;

	if (!*s) {
		*s = php_http_buffer_init_ex(NULL, chunk_size, chunk_size ? PHP_HTTP_BUFFER_INIT_PREALLOC : 0);
	}
	str = *s;

	php_http_buffer_resize(str, chunk_size);
	passed = passin(opaque, str->data + str->used, chunk_size TSRMLS_CC);

	if (passed != PHP_HTTP_BUFFER_PASS0) {
		str->used += passed;
		str->free -= passed;
	}

	php_http_buffer_fix(str);

	return passed;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */

