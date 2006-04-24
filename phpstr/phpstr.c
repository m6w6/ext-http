
/* $Id$ */

#include "php.h"
#include "phpstr.h"

PHPSTR_API phpstr *phpstr_init_ex(phpstr *buf, size_t chunk_size, int flags)
{
	if (!buf) {
		buf = pemalloc(sizeof(phpstr), flags & PHPSTR_INIT_PERSISTENT);
	}

	if (buf) {
		buf->size = (chunk_size) ? chunk_size : PHPSTR_DEFAULT_SIZE;
		buf->pmem = (flags & PHPSTR_INIT_PERSISTENT) ? 1 : 0;
		buf->data = (flags & PHPSTR_INIT_PREALLOC) ? pemalloc(buf->size, buf->pmem) : NULL;
		buf->free = (flags & PHPSTR_INIT_PREALLOC) ? buf->size : 0;
		buf->used = 0;
	}
	
	return buf;
}

PHPSTR_API phpstr *phpstr_from_string_ex(phpstr *buf, const char *string, size_t length)
{
	if ((buf = phpstr_init(buf))) {
		if (PHPSTR_NOMEM == phpstr_append(buf, string, length)) {
			pefree(buf, buf->pmem);
			buf = NULL;
		}
	}
	return buf;
}

PHPSTR_API size_t phpstr_resize_ex(phpstr *buf, size_t len, size_t override_size, int allow_error)
{
	char *ptr = NULL;
#if 0
	fprintf(stderr, "RESIZE: size=%lu, used=%lu, free=%lu\n", buf->size, buf->used, buf->free);
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
			return PHPSTR_NOMEM;
		}
		
		buf->free += size;
		return size;
	}
	return 0;
}

PHPSTR_API size_t phpstr_shrink(phpstr *buf)
{
	/* avoid another realloc on fixation */
	if (buf->free > 1) {
		char *ptr = perealloc(buf->data, buf->used + 1, buf->pmem);
		
		if (ptr) {
			buf->data = ptr;
		} else {
			return PHPSTR_NOMEM;
		}
		buf->free = 1;
	}
	return buf->used;
}

PHPSTR_API size_t phpstr_append(phpstr *buf, const char *append, size_t append_len)
{
	if (PHPSTR_NOMEM == phpstr_resize(buf, append_len)) {
		return PHPSTR_NOMEM;
	}
	memcpy(buf->data + buf->used, append, append_len);
	buf->used += append_len;
	buf->free -= append_len;
	return append_len;
}

PHPSTR_API size_t phpstr_appendf(phpstr *buf, const char *format, ...)
{
	va_list argv;
	char *append;
	size_t append_len, alloc;

	va_start(argv, format);
	append_len = vspprintf(&append, 0, format, argv);
	va_end(argv);

	alloc = phpstr_append(buf, append, append_len);
	efree(append);

	if (PHPSTR_NOMEM == alloc) {
		return PHPSTR_NOMEM;
	}
	return append_len;
}

PHPSTR_API size_t phpstr_insert(phpstr *buf, const char *insert, size_t insert_len, size_t offset)
{
	if (PHPSTR_NOMEM == phpstr_resize(buf, insert_len)) {
		return PHPSTR_NOMEM;
	}
	memmove(buf->data + offset + insert_len, buf->data + offset, insert_len);
	memcpy(buf->data + offset, insert, insert_len);
	buf->used += insert_len;
	buf->free -= insert_len;
	return insert_len;
}

PHPSTR_API size_t phpstr_insertf(phpstr *buf, size_t offset, const char *format, ...)
{
	va_list argv;
	char *insert;
	size_t insert_len, alloc;

	va_start(argv, format);
	insert_len = vspprintf(&insert, 0, format, argv);
	va_end(argv);

	alloc = phpstr_insert(buf, insert, insert_len, offset);
	efree(insert);

	if (PHPSTR_NOMEM == alloc) {
		return PHPSTR_NOMEM;
	}
	return insert_len;
}

PHPSTR_API size_t phpstr_prepend(phpstr *buf, const char *prepend, size_t prepend_len)
{
	if (PHPSTR_NOMEM == phpstr_resize(buf, prepend_len)) {
		return PHPSTR_NOMEM;
	}
	memmove(buf->data + prepend_len, buf->data, buf->used);
	memcpy(buf->data, prepend, prepend_len);
	buf->used += prepend_len;
	buf->free -= prepend_len;
	return prepend_len;
}

PHPSTR_API size_t phpstr_prependf(phpstr *buf, const char *format, ...)
{
	va_list argv;
	char *prepend;
	size_t prepend_len, alloc;

	va_start(argv, format);
	prepend_len = vspprintf(&prepend, 0, format, argv);
	va_end(argv);

	alloc = phpstr_prepend(buf, prepend, prepend_len);
	efree(prepend);

	if (PHPSTR_NOMEM == alloc) {
		return PHPSTR_NOMEM;
	}
	return prepend_len;
}

PHPSTR_API char *phpstr_data(const phpstr *buf, char **into, size_t *len)
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

PHPSTR_API phpstr *phpstr_dup(const phpstr *buf)
{
	phpstr *dup = phpstr_clone(buf);
	if (PHPSTR_NOMEM == phpstr_append(dup, buf->data, buf->used)) {
		phpstr_free(&dup);
	}
	return dup;
}

PHPSTR_API size_t phpstr_cut(phpstr *buf, size_t offset, size_t length)
{
	if (offset >= buf->used) {
		return 0;
	}
	if (offset + length > buf->used) {
		length = buf->used - offset;
	}
	memmove(buf->data + offset, buf->data + offset + length, buf->used - length);
	buf->used -= length;
	buf->free += length;
	return length;
}

PHPSTR_API phpstr *phpstr_sub(const phpstr *buf, size_t offset, size_t length)
{
	if (offset >= buf->used) {
		return NULL;
	} else {
		size_t need = 1 + ((length + offset) > buf->used ? (buf->used - offset) : (length - offset));
		phpstr *sub = phpstr_init_ex(NULL, need, PHPSTR_INIT_PREALLOC | (buf->pmem ? PHPSTR_INIT_PERSISTENT:0));
		if (sub) {
			if (PHPSTR_NOMEM == phpstr_append(sub, buf->data + offset, need)) {
				phpstr_free(&sub);
			} else {
				sub->size = buf->size;
			}
		}
		return sub;
	}
}

PHPSTR_API phpstr *phpstr_right(const phpstr *buf, size_t length)
{
	if (length < buf->used) {
		return phpstr_sub(buf, buf->used - length, length);
	} else {
		return phpstr_sub(buf, 0, buf->used);
	}
}


PHPSTR_API phpstr *phpstr_merge_va(phpstr *buf, unsigned argc, va_list argv)
{
	unsigned i = 0;
	buf = phpstr_init(buf);

	if (buf) {
		while (argc > i++) {
			phpstr_free_t f = va_arg(argv, phpstr_free_t);
			phpstr *current = va_arg(argv, phpstr *);
			phpstr_append(buf, current->data, current->used);
			FREE_PHPSTR(f, current);
		}
	}

	return buf;
}

PHPSTR_API phpstr *phpstr_merge_ex(phpstr *buf, unsigned argc, ...)
{
	va_list argv;
	phpstr *ret;

	va_start(argv, argc);
	ret = phpstr_merge_va(buf, argc, argv);
	va_end(argv);
	return ret;
}

PHPSTR_API phpstr *phpstr_merge(unsigned argc, ...)
{
	va_list argv;
	phpstr *ret;

	va_start(argv, argc);
	ret = phpstr_merge_va(NULL, argc, argv);
	va_end(argv);
	return ret;
}

PHPSTR_API phpstr *phpstr_fix(phpstr *buf)
{
	if (PHPSTR_NOMEM == phpstr_resize_ex(buf, 1, 1, 0)) {
		return NULL;
	}
	buf->data[buf->used] = '\0';
	return buf;
}

PHPSTR_API int phpstr_cmp(phpstr *left, phpstr *right)
{
	if (left->used > right->used) {
		return -1;
	} else if (right->used > left->used) {
		return 1;
	} else {
		return memcmp(left->data, right->data, left->used);
	}
}

PHPSTR_API void phpstr_reset(phpstr *buf)
{
	buf->free += buf->used;
	buf->used = 0;
}

PHPSTR_API void phpstr_dtor(phpstr *buf)
{
	if (buf->data) {
		pefree(buf->data, buf->pmem);
		buf->data = NULL;
	}
	buf->used = 0;
	buf->free = 0;
}

PHPSTR_API void phpstr_free(phpstr **buf)
{
	if (*buf) {
		phpstr_dtor(*buf);
		pefree(*buf, (*buf)->pmem);
		*buf = NULL;
	}
}

PHPSTR_API size_t phpstr_chunk_buffer(phpstr **s, const char *data, size_t data_len, char **chunk, size_t chunk_size)
{
	phpstr *storage;
	
	*chunk = NULL;
	
	if (!*s) {
		*s = phpstr_init_ex(NULL, chunk_size << 1, chunk_size ? PHPSTR_INIT_PREALLOC : 0);
	}
	storage = *s;
	
	if (data_len) {
		phpstr_append(storage, data, data_len);
	}
	
	if (!chunk_size) {
		phpstr_data(storage, chunk, &chunk_size);
		phpstr_free(s);
		return chunk_size;
	}
	
	if (storage->used >= (chunk_size = storage->size >> 1)) {
		*chunk = estrndup(storage->data, chunk_size);
		phpstr_cut(storage, 0, chunk_size);
		return chunk_size;
	}
	
	return 0;
}

PHPSTR_API void phpstr_chunked_output(phpstr **s, const char *data, size_t data_len, size_t chunk_len, phpstr_passthru_func passthru, void *opaque TSRMLS_DC)
{
	char *chunk = NULL;
	size_t got = 0;
	
	while ((got = phpstr_chunk_buffer(s, data, data_len, &chunk, chunk_len))) {
		passthru(opaque, chunk, got TSRMLS_CC);
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */

