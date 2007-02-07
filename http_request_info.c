/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_CURL
#include "php_http.h"

#ifdef HTTP_HAVE_CURL
#include "php_http_request_api.h"

/* {{{ void http_request_info(http_request *, HashTable *) */
PHP_HTTP_API void _http_request_info(http_request *request, HashTable *info)
{
	char *c;
	long l;
	double d;
	struct curl_slist *s, *p;
	zval *subarray, array;
	INIT_ZARR(array, info);
	
	/* BEGIN */
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_EFFECTIVE_URL, &c)) {
		add_assoc_string_ex(&array, "effective_url", sizeof("effective_url"), c ? c : "", 1);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_RESPONSE_CODE, &l)) {
		add_assoc_long_ex(&array, "response_code", sizeof("response_code"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_TOTAL_TIME, &d)) {
		add_assoc_double_ex(&array, "total_time", sizeof("total_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_NAMELOOKUP_TIME, &d)) {
		add_assoc_double_ex(&array, "namelookup_time", sizeof("namelookup_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_CONNECT_TIME, &d)) {
		add_assoc_double_ex(&array, "connect_time", sizeof("connect_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_PRETRANSFER_TIME, &d)) {
		add_assoc_double_ex(&array, "pretransfer_time", sizeof("pretransfer_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_SIZE_UPLOAD, &d)) {
		add_assoc_double_ex(&array, "size_upload", sizeof("size_upload"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_SIZE_DOWNLOAD, &d)) {
		add_assoc_double_ex(&array, "size_download", sizeof("size_download"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_SPEED_DOWNLOAD, &d)) {
		add_assoc_double_ex(&array, "speed_download", sizeof("speed_download"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_SPEED_UPLOAD, &d)) {
		add_assoc_double_ex(&array, "speed_upload", sizeof("speed_upload"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_HEADER_SIZE, &l)) {
		add_assoc_long_ex(&array, "header_size", sizeof("header_size"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_REQUEST_SIZE, &l)) {
		add_assoc_long_ex(&array, "request_size", sizeof("request_size"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_SSL_VERIFYRESULT, &l)) {
		add_assoc_long_ex(&array, "ssl_verifyresult", sizeof("ssl_verifyresult"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_FILETIME, &l)) {
		add_assoc_long_ex(&array, "filetime", sizeof("filetime"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &d)) {
		add_assoc_double_ex(&array, "content_length_download", sizeof("content_length_download"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_CONTENT_LENGTH_UPLOAD, &d)) {
		add_assoc_double_ex(&array, "content_length_upload", sizeof("content_length_upload"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_STARTTRANSFER_TIME, &d)) {
		add_assoc_double_ex(&array, "starttransfer_time", sizeof("starttransfer_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_CONTENT_TYPE, &c)) {
		add_assoc_string_ex(&array, "content_type", sizeof("content_type"), c ? c : "", 1);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_REDIRECT_TIME, &d)) {
		add_assoc_double_ex(&array, "redirect_time", sizeof("redirect_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_REDIRECT_COUNT, &l)) {
		add_assoc_long_ex(&array, "redirect_count", sizeof("redirect_count"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_HTTP_CONNECTCODE, &l)) {
		add_assoc_long_ex(&array, "connect_code", sizeof("connect_code"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_HTTPAUTH_AVAIL, &l)) {
		add_assoc_long_ex(&array, "httpauth_avail", sizeof("httpauth_avail"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_PROXYAUTH_AVAIL, &l)) {
		add_assoc_long_ex(&array, "proxyauth_avail", sizeof("proxyauth_avail"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_OS_ERRNO, &l)) {
		add_assoc_long_ex(&array, "os_errno", sizeof("os_errno"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_NUM_CONNECTS, &l)) {
		add_assoc_long_ex(&array, "num_connects", sizeof("num_connects"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_SSL_ENGINES, &s)) {
		MAKE_STD_ZVAL(subarray);
		array_init(subarray);
		for (p = s; p; p = p->next) {
			add_next_index_string(subarray, p->data, 1);
		}
		add_assoc_zval_ex(&array, "ssl_engines", sizeof("ssl_engines"), subarray);
		curl_slist_free_all(s);
	}
#if HTTP_CURL_VERSION(7,14,1)
	if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_COOKIELIST, &s)) {
		MAKE_STD_ZVAL(subarray);
		array_init(subarray);
		for (p = s; p; p = p->next) {
			add_next_index_string(subarray, p->data, 1);
		}
		add_assoc_zval_ex(&array, "cookies", sizeof("cookies"), subarray);
		curl_slist_free_all(s);
	}
#endif
/* END */
	add_assoc_string_ex(&array, "error", sizeof("error"), request->_error, 1);
}
/* }}} */

#endif /* HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
