/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_ENCODING_API_H
#define PHP_HTTP_ENCODING_API_H

#include "php_http_std_defs.h"

#define http_encoding_dechunk(e, el, d, dl) _http_encoding_dechunk((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API const char *_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#ifdef HTTP_HAVE_ZLIB

#define http_encoding_gzencode(l, d, dl, r, rl) _http_encoding_gzencode((l), (d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_gzencode(int level, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC);
#define http_encoding_gzdecode(d, dl, r, rl) _http_encoding_gzdecode((d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_gzdecode(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC);
#define http_encoding_deflate(l, d, dl, r, rl) _http_encoding_deflate((l), (d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_deflate(int level, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC);
#define http_encoding_inflate(d, dl, r, rl) _http_encoding_inflate((d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC);
#define http_encoding_compress(l, d, dl, r, rl) _http_encoding_compress((l), (d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_compress(int level, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC);
#define http_encoding_uncompress(d, dl, r, rl) _http_encoding_uncompress((d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_uncompress(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC);

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

