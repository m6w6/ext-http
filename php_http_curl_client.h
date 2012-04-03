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

#ifndef PHP_HTTP_CURL_CLIENT_H
#define PHP_HTTP_CURL_CLIENT_H

#if PHP_HTTP_HAVE_CURL

PHP_HTTP_API php_http_client_ops_t *php_http_curl_client_get_ops(void);

typedef struct php_http_curl_client {
	CURL *handle;

	struct {
		HashTable cache;

		struct curl_slist *headers;
		struct curl_slist *resolve;
		php_http_buffer_t cookies;

		long redirects;
		unsigned range_request:1;

		struct {
			uint count;
			double delay;
		} retry;

	} options;

	php_http_client_progress_t progress;

} php_http_curl_client_t;

typedef struct php_http_curl_client_storage {
	char *url;
	char *cookiestore;
	char errorbuffer[0x100];
} php_http_curl_client_storage_t;

static inline php_http_curl_client_storage_t *get_storage(CURL *ch) {
	php_http_curl_client_storage_t *st = NULL;

	curl_easy_getinfo(ch, CURLINFO_PRIVATE, &st);

	if (!st) {
		st = pecalloc(1, sizeof(*st), 1);
		curl_easy_setopt(ch, CURLOPT_PRIVATE, st);
		curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, st->errorbuffer);
	}

	return st;
}

PHP_HTTP_API STATUS php_http_curl_client_prepare(php_http_client_t *h, php_http_message_t *msg);

zend_class_entry *php_http_curl_client_get_class_entry(void);

zend_object_value php_http_curl_client_object_new(zend_class_entry *ce TSRMLS_DC);
zend_object_value php_http_curl_client_object_new_ex(zend_class_entry *ce, php_http_client_t *r, php_http_client_object_t **ptr TSRMLS_DC);

PHP_MINIT_FUNCTION(http_curl_client);

#endif /* PHP_HTTP_HAVE_CURL */
#endif /* PHP_HTTP_CURL_CLIENT_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
