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

#ifndef PHP_HTTP_CURL_API_H
#define PHP_HTTP_CURL_API_H

#include <curl/curl.h>

#define http_get(u, o, i, d, l) _http_get_ex(NULL, (u), (o), (i), (d), (l) TSRMLS_CC)
#define http_get_ex(c, u, o, i, d, l) _http_get_ex((c), (u), (o), (i), (d), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_get_ex(CURL *ch, const char *URL, HashTable *options, HashTable *info, char **data, size_t *data_len TSRMLS_DC);

#define http_head(u, o, i, d, l) _http_head_ex(NULL, (u), (o), (i), (d), (l) TSRMLS_CC)
#define http_head_ex(c, u, o, i, d, l) _http_head_ex((c), (u), (o), (i), (d), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_head_ex(CURL *ch, const char *URL, HashTable *options, HashTable *info, char **data, size_t *data_len TSRMLS_DC);

#define http_post_data(u, pd, pl, o, i, d, l) _http_post_data_ex(NULL, (u), (pd), (pl), (o), (i), (d), (l) TSRMLS_CC)
#define http_post_data_ex(c, u, pd, pl, o, i, d, l) _http_post_data_ex((c), (u), (pd), (pl), (o), (i), (d), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_post_data_ex(CURL *ch, const char *URL, char *postdata, size_t postdata_len, HashTable *options, HashTable *info, char **data, size_t *data_len TSRMLS_DC);

#define http_post_array(u, p, o, i, d, l) _http_post_array_ex(NULL, (u), (p), (o), (i), (d), (l) TSRMLS_CC)
#define http_post_array_ex(c, u, p, o, i, d, l) _http_post_array_ex((c), (u), (p), (o), (i), (d), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_post_array_ex(CURL *ch, const char *URL, HashTable *postarray, HashTable *options, HashTable *info, char **data, size_t *data_len TSRMLS_DC);

#define http_post_curldata(u, h, o, i, d, l) _http_post_curldata_ex(NULL, (u), (h), (o), (i), (d), (l) TSRMLS_CC)
#define http_post_curldata_ex(c, u, h, o, i, d, l) _http_post_curldata_ex((c), (u), (h), (o), (i), (d), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_post_curldata_ex(CURL *ch, const char *URL, struct curl_httppost *curldata, HashTable *options, HashTable *info, char **data, size_t *data_len TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */