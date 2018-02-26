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

#ifndef PHP_HTTP_ENCODING_BROTLI_H
#define PHP_HTTP_ENCODING_BROTLI_H
#if PHP_HTTP_HAVE_LIBBROTLI

#include <brotli/decode.h>
#include <brotli/encode.h>

extern PHP_MINIT_FUNCTION(http_encoding_brotli);

PHP_HTTP_API zend_class_entry *php_http_get_enbrotli_stream_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_debrotli_stream_class_entry(void);

PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_enbrotli_ops(void);
PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_debrotli_ops(void);

PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_enbrotli(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_debrotli(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len);

#define PHP_HTTP_ENBROTLI_LEVEL_MIN			0x00000001
#define PHP_HTTP_ENBROTLI_LEVEL_DEF			0x00000004
#define PHP_HTTP_ENBROTLI_LEVEL_MAX			0x0000000b

#define PHP_HTTP_ENBROTLI_WBITS_MIN			0x000000a0
#define PHP_HTTP_ENBROTLI_WBITS_DEF			0x00000160
#define PHP_HTTP_ENBROTLI_WBITS_MAX			0x00000180

#define PHP_HTTP_ENBROTLI_MODE_GENERIC		0x00000000
#define PHP_HTTP_ENBROTLI_MODE_TEXT			0x00001000
#define PHP_HTTP_ENBROTLI_MODE_FONT			0x00002000

#endif
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

