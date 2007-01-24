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

#include "php_http.h"

#include "php_http_api.h"
#include "php_http_info_api.h"

PHP_HTTP_API void _http_info_default_callback(void **nothing, HashTable **headers, http_info *info TSRMLS_DC)
{
	zval array;
	
	INIT_ZARR(array, *headers);
	
	switch (info->type) {
		case IS_HTTP_REQUEST:
			add_assoc_string(&array, "Request Method", HTTP_INFO(info).request.method, 1);
			add_assoc_string(&array, "Request Url", HTTP_INFO(info).request.url, 1);
			break;
		
		case IS_HTTP_RESPONSE:
			add_assoc_long(&array, "Response Code", (long) HTTP_INFO(info).response.code);
			add_assoc_string(&array, "Response Status", HTTP_INFO(info).response.status, 1);
			break;
	}
}

PHP_HTTP_API void _http_info_dtor(http_info *i)
{
	switch (i->type) {
		case IS_HTTP_REQUEST:
			STR_SET(HTTP_INFO(i).request.method, NULL);
			STR_SET(HTTP_INFO(i).request.url, NULL);
			break;
		
		case IS_HTTP_RESPONSE:
			STR_SET(HTTP_INFO(i).response.status, NULL);
			break;
		
		default:
			break;
	}
}

PHP_HTTP_API STATUS _http_info_parse_ex(const char *pre_header, http_info *info, zend_bool silent TSRMLS_DC)
{
	const char *end, *http;
	
	/* sane parameter */
	if ((!pre_header) || (!*pre_header)) {
		return FAILURE;
	}
	
	/* where's the end of the line */
	if (!(end = http_locate_eol(pre_header, NULL))) {
		end = pre_header + strlen(pre_header);
	}
	
	/* there must be HTTP/1.x in the line */
	if (!(http = http_locate_str(pre_header, end - pre_header, "HTTP/1.", lenof("HTTP/1.")))) {
		return FAILURE;
	}
	
	/* and nothing than SPACE or NUL after HTTP/1.x */
	if (	(!HTTP_IS_CTYPE(digit, http[lenof("HTTP/1.")])) ||
			(http[lenof("HTTP/1.1")] && (!HTTP_IS_CTYPE(space, http[lenof("HTTP/1.1")])))) {
		if (!silent) {
			http_error(HE_WARNING, HTTP_E_MALFORMED_HEADERS, "Invalid HTTP/1.x protocol identification");
		}
		return FAILURE;
	}

#if 0
	{
		char *line = estrndup(pre_header, end - pre_header);
		fprintf(stderr, "http_parse_info('%s')\n", line);
		efree(line);
	}
#endif

	info->http.version = atof(http + lenof("HTTP/"));
	
	/* is response */
	if (pre_header == http) {
		char *status = NULL;
		const char *code = http + sizeof("HTTP/1.1");
		
		info->type = IS_HTTP_RESPONSE;
		while (' ' == *code) ++code;
		if (code && end > code) {
			HTTP_INFO(info).response.code = strtol(code, &status, 10);
		} else {
			HTTP_INFO(info).response.code = 0;
		}
		if (status && end > status) {
			while (' ' == *status) ++status;
			HTTP_INFO(info).response.status = estrndup(status, end - status);
		} else {
			HTTP_INFO(info).response.status = NULL;
		}
		
		return SUCCESS;
	}
	
	/* is request */
	else if (!http[lenof("HTTP/1.x")] || http[lenof("HTTP/1.x")] == '\r' || http[lenof("HTTP/1.x")] == '\n') {
		const char *url = strchr(pre_header, ' ');
		
		info->type = IS_HTTP_REQUEST;
		if (url && http > url) {
			HTTP_INFO(info).request.method = estrndup(pre_header, url - pre_header);
			while (' ' == *url) ++url;
			while (' ' == *(http-1)) --http;
			if (http > url) {
				HTTP_INFO(info).request.url = estrndup(url, http - url);
			} else {
				efree(HTTP_INFO(info).request.method);
				return FAILURE;
			}
		} else {
			HTTP_INFO(info).request.method = NULL;
			HTTP_INFO(info).request.url = NULL;
		}
		
		return SUCCESS;
	}

	/* some darn header containing HTTP/1.x */
	else {
		return FAILURE;
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

