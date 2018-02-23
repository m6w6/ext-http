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

#include "php_http_api.h"

php_http_info_t *php_http_info_init(php_http_info_t *i)
{
	if (!i) {
		i = emalloc(sizeof(*i));
	}

	memset(i, 0, sizeof(*i));

	return i;
}

void php_http_info_dtor(php_http_info_t *i)
{
	switch (i->type) {
		case PHP_HTTP_REQUEST:
			PTR_SET(PHP_HTTP_INFO(i).request.method, NULL);
			PTR_SET(PHP_HTTP_INFO(i).request.url, NULL);
			break;

		case PHP_HTTP_RESPONSE:
			PTR_SET(PHP_HTTP_INFO(i).response.status, NULL);
			break;

		default:
			break;
	}
}

void php_http_info_free(php_http_info_t **i)
{
	if (*i) {
		php_http_info_dtor(*i);
		efree(*i);
		*i = NULL;
	}
}

void php_http_info_to_string(php_http_info_t *info, char **str, size_t *len, const char *eol)
{
	char *tmp = NULL;

	if (info->http.version.major == 2) {
		if (info->type == PHP_HTTP_REQUEST) {
			*len = spprintf(str, 0, "%s %s HTTP/2%s",
					info->http.info.request.method?info->http.info.request.method:"UNKNOWN",
					info->http.info.request.method&&!strcasecmp(info->http.info.request.method,"CONNECT")?(
					info->http.info.request.url?php_http_url_authority_to_string(info->http.info.request.url, &(tmp), NULL):"0"):(
					info->http.info.request.url?php_http_url_to_string(info->http.info.request.url, &(tmp), NULL, 0):"/"),
					eol);
		} else if (info->type == PHP_HTTP_RESPONSE) {
			*len = spprintf(str, 0, "HTTP/2 %d%s%s%s",
					info->http.info.response.code?info->http.info.response.code:200,
					info->http.info.response.status&&*info->http.info.response.status ? " ":"",
					STR_PTR(info->http.info.response.status),
					eol);
		}
	} else if (info->type == PHP_HTTP_REQUEST) {
		*len = spprintf(str, 0, "%s %s HTTP/%u.%u%s",
				info->http.info.request.method?info->http.info.request.method:"UNKNOWN",
				info->http.info.request.method&&!strcasecmp(info->http.info.request.method,"CONNECT")?(
				info->http.info.request.url?php_http_url_authority_to_string(info->http.info.request.url, &(tmp), NULL):"0"):(
				info->http.info.request.url?php_http_url_to_string(info->http.info.request.url, &(tmp), NULL, 0):"/"),
				info->http.version.major||info->http.version.minor?info->http.version.major:1,
				info->http.version.major||info->http.version.minor?info->http.version.minor:1,
				eol);
	} else if (info->type == PHP_HTTP_RESPONSE){
		*len = spprintf(str, 0, "HTTP/%u.%u %d%s%s%s",
				info->http.version.major||info->http.version.minor?info->http.version.major:1,
				info->http.version.major||info->http.version.minor?info->http.version.minor:1,
				info->http.info.response.code?info->http.info.response.code:200,
				info->http.info.response.status&&*info->http.info.response.status ? " ":"",
				STR_PTR(info->http.info.response.status),
				eol);
	}

	PTR_FREE(tmp);
}

php_http_info_t *php_http_info_parse(php_http_info_t *info, const char *pre_header)
{
	const char *end, *http, *off;
	zend_bool free_info = !info;

	/* sane parameter */
	if (UNEXPECTED((!pre_header) || (!*pre_header))) {
		return NULL;
	}

	/* where's the end of the line */
	if (UNEXPECTED(!(end = php_http_locate_eol(pre_header, NULL)))) {
		end = pre_header + strlen(pre_header);
	}

	/* there must be HTTP/1.x in the line */
	if (!(http = php_http_locate_str(pre_header, end - pre_header, "HTTP/", lenof("HTTP/")))) {
		return NULL;
	}

	info = php_http_info_init(info);

	if (UNEXPECTED(!php_http_version_parse(&info->http.version, http))) {
		if (free_info) {
			php_http_info_free(&info);
		}
		return NULL;
	}

	/* clumsy fix for changed libcurl behaviour in 7.49.1, see https://github.com/curl/curl/issues/888 */
	off = &http[lenof("HTTP/X")];
	if (info->http.version.major < 2 || (info->http.version.major == 2 && *off == '.')) {
		off += 2;
	}

	/* and nothing than SPACE or NUL after HTTP/X(.x) */
	if (UNEXPECTED(*off && (!PHP_HTTP_IS_CTYPE(space, *off)))) {
		if (free_info) {
			php_http_info_free(&info);
		}
		return NULL;
	}

#if 0
	{
		char *line = estrndup(pre_header, end - pre_header);
		fprintf(stderr, "http_parse_info('%s')\n", line);
		efree(line);
	}
#endif

	/* is response */
	if (pre_header == http) {
		const char *status = NULL, *code = off;

		info->type = PHP_HTTP_RESPONSE;
		while (code < end && ' ' == *code) ++code;
		if (EXPECTED(end > code)) {
			/* rfc7230#3.1.2 The status-code element is a 3-digit integer code */
			PHP_HTTP_INFO(info).response.code = 100*(*code++ - '0');
			PHP_HTTP_INFO(info).response.code += 10*(*code++ - '0');
			PHP_HTTP_INFO(info).response.code +=     *code++ - '0';
			if (PHP_HTTP_INFO(info).response.code < 100 || PHP_HTTP_INFO(info).response.code > 599) {
				if (free_info) {
					php_http_info_free(&info);
				}
				return NULL;
			}
			status = code;
		} else {
			PHP_HTTP_INFO(info).response.code = 0;
		}
		if (EXPECTED(status && end > status)) {
			while (' ' == *status && end > status) ++status;
			PHP_HTTP_INFO(info).response.status = estrndup(status, end - status);
		} else {
			PHP_HTTP_INFO(info).response.status = NULL;
		}

		return info;
	}

	/* is request */
	else if (*(http - 1) == ' ' && (!*off || *off == '\r' || *off == '\n')) {
		const char *url = strchr(pre_header, ' ');

		info->type = PHP_HTTP_REQUEST;
		if (EXPECTED(url && http > url)) {
			size_t url_len = url - pre_header;

			PHP_HTTP_INFO(info).request.method = estrndup(pre_header, url_len);

			while (' ' == *url && http > url) ++url;
			while (' ' == *(http-1)) --http;

			if (EXPECTED(http > url)) {
				/* CONNECT presents an authority only */
				if (UNEXPECTED(strcasecmp(PHP_HTTP_INFO(info).request.method, "CONNECT"))) {
					PHP_HTTP_INFO(info).request.url = php_http_url_parse(url, http - url, PHP_HTTP_URL_STDFLAGS);
				} else {
					PHP_HTTP_INFO(info).request.url = php_http_url_parse_authority(url, http - url, PHP_HTTP_URL_STDFLAGS);
				}
				if (!PHP_HTTP_INFO(info).request.url) {
					PTR_SET(PHP_HTTP_INFO(info).request.method, NULL);
					return NULL;
				}
			} else {
				PTR_SET(PHP_HTTP_INFO(info).request.method, NULL);
				return NULL;
			}
		} else {
			PHP_HTTP_INFO(info).request.method = NULL;
			PHP_HTTP_INFO(info).request.url = NULL;
		}

		return info;
	}

	/* some darn header containing HTTP/X(.x) */
	else {
		if (free_info) {
			php_http_info_free(&info);
		}
		return NULL;
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

