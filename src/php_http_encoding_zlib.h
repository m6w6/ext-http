/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_ENCODING_ZLIB_H
#define PHP_HTTP_ENCODING_ZLIB_H

#include "php_http_encoding.h"

#include <zlib.h>

#ifndef Z_FIXED
/* Z_FIXED does not exist prior 1.2.2.2 */
#	define Z_FIXED 0
#endif

extern PHP_MINIT_FUNCTION(http_encoding_zlib);

zend_class_entry *php_http_get_deflate_stream_class_entry(void);
zend_class_entry *php_http_get_inflate_stream_class_entry(void);

PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_deflate_ops(void);
PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_inflate_ops(void);

PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_deflate(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len);

#define PHP_HTTP_DEFLATE_LEVEL_DEF			0x00000000
#define PHP_HTTP_DEFLATE_LEVEL_MIN			0x00000001
#define PHP_HTTP_DEFLATE_LEVEL_MAX			0x00000009
#define PHP_HTTP_DEFLATE_TYPE_ZLIB			0x00000000
#define PHP_HTTP_DEFLATE_TYPE_GZIP			0x00000010
#define PHP_HTTP_DEFLATE_TYPE_RAW			0x00000020
#define PHP_HTTP_DEFLATE_STRATEGY_DEF		0x00000000
#define PHP_HTTP_DEFLATE_STRATEGY_FILT		0x00000100
#define PHP_HTTP_DEFLATE_STRATEGY_HUFF		0x00000200
#define PHP_HTTP_DEFLATE_STRATEGY_RLE		0x00000300
#define PHP_HTTP_DEFLATE_STRATEGY_FIXED		0x00000400

#define PHP_HTTP_INFLATE_TYPE_ZLIB			0x00000000
#define PHP_HTTP_INFLATE_TYPE_GZIP			0x00000000
#define PHP_HTTP_INFLATE_TYPE_RAW			0x00000001

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

