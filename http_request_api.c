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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#ifdef PHP_WIN32
#	include <winsock2.h>
#endif

#include <curl/curl.h>

#include "phpstr/phpstr.h"

#include "php.h"
#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_request_api.h"
#include "php_http_url_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http)

#if LIBCURL_VERSION_NUM < 0x070c00
#	define curl_easy_strerror(code) HTTP_G(request).curl.error
#endif

#define HTTP_CURL_INFO(I) HTTP_CURL_INFO_EX(I, I)
#define HTTP_CURL_INFO_EX(I, X) \
	switch (CURLINFO_ ##I & ~CURLINFO_MASK) \
	{ \
		case CURLINFO_STRING: \
		{ \
			char *c; \
			if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_ ##I, &c)) { \
				add_assoc_string(&array, pretty_key(http_curl_copystr(#X), sizeof(#X)-1, 0, 0), c ? c : "", 1); \
			} \
		} \
		break; \
\
		case CURLINFO_DOUBLE: \
		{ \
			double d; \
			if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_ ##I, &d)) { \
				add_assoc_double(&array, pretty_key(http_curl_copystr(#X), sizeof(#X)-1, 0, 0), d); \
			} \
		} \
		break; \
\
		case CURLINFO_LONG: \
		{ \
			long l; \
			if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_ ##I, &l)) { \
				add_assoc_long(&array, pretty_key(http_curl_copystr(#X), sizeof(#X)-1, 0, 0), l); \
			} \
		} \
		break; \
	}

#define HTTP_CURL_OPT(OPTION, p) curl_easy_setopt(ch, CURLOPT_##OPTION, (p))
#define HTTP_CURL_OPT_STRING(keyname) HTTP_CURL_OPT_STRING_EX(keyname, keyname)
#define HTTP_CURL_OPT_SSL_STRING(keyname) HTTP_CURL_OPT_STRING_EX(keyname, SSL##keyname)
#define HTTP_CURL_OPT_SSL_STRING_(keyname) HTTP_CURL_OPT_STRING_EX(keyname, SSL_##keyname)
#define HTTP_CURL_OPT_STRING_EX(keyname, optname) \
	if (!strcasecmp(key, #keyname)) { \
		convert_to_string_ex(param); \
		HTTP_CURL_OPT(optname, http_curl_copystr(Z_STRVAL_PP(param))); \
		key = NULL; \
		continue; \
	}
#define HTTP_CURL_OPT_LONG(keyname) HTTP_OPT_SSL_LONG_EX(keyname, keyname)
#define HTTP_CURL_OPT_SSL_LONG(keyname) HTTP_CURL_OPT_LONG_EX(keyname, SSL##keyname)
#define HTTP_CURL_OPT_SSL_LONG_(keyname) HTTP_CURL_OPT_LONG_EX(keyname, SSL_##keyname)
#define HTTP_CURL_OPT_LONG_EX(keyname, optname) \
	if (!strcasecmp(key, #keyname)) { \
		convert_to_long_ex(param); \
		HTTP_CURL_OPT(optname, Z_LVAL_PP(param)); \
		key = NULL; \
		continue; \
	}


static const char *const http_request_methods[HTTP_MAX_REQUEST_METHOD + 1];
#define http_curl_copystr(s) _http_curl_copystr((s) TSRMLS_CC)
static inline char *_http_curl_copystr(const char *str TSRMLS_DC);
#define http_curl_getopt(o, k, t) _http_curl_getopt_ex((o), (k), sizeof(k), (t) TSRMLS_CC)
#define http_curl_getopt_ex(o, k, l, t) _http_curl_getopt_ex((o), (k), (l), (t) TSRMLS_CC)
static inline zval *_http_curl_getopt_ex(HashTable *options, char *key, size_t keylen, int type TSRMLS_DC);
static size_t http_curl_write_callback(char *, size_t, size_t, void *);
static size_t http_curl_read_callback(void *, size_t, size_t, void *);
static int http_curl_progress_callback(void *, double, double, double, double);
static int http_curl_debug_callback(CURL *, curl_infotype, char *, size_t, void *);


/* {{{ STATUS http_request_body_fill(http_request_body *body, HashTable *, HashTable *) */
PHP_HTTP_API STATUS _http_request_body_fill(http_request_body *body, HashTable *fields, HashTable *files TSRMLS_DC)
{
	if (files && (zend_hash_num_elements(files) > 0)) {
		char *key = NULL;
		ulong idx;
		zval **data;
		struct curl_httppost *http_post_data[2] = {NULL, NULL};

		/* normal data */
		FOREACH_HASH_KEYVAL(fields, key, idx, data) {
			CURLcode err;
			if (key) {
				convert_to_string_ex(data);
				err = curl_formadd(&http_post_data[0], &http_post_data[1],
					CURLFORM_COPYNAME,			key,
					CURLFORM_COPYCONTENTS,		Z_STRVAL_PP(data),
					CURLFORM_CONTENTSLENGTH,	Z_STRLEN_PP(data),
					CURLFORM_END
				);
				if (CURLE_OK != err) {
					http_error_ex(E_WARNING, HTTP_E_CURL, "Could not encode post fields: %s", curl_easy_strerror(err));
					curl_formfree(http_post_data[0]);
					return FAILURE;
				}

				/* reset */
				key = NULL;
			}
		}

		/* file data */
		FOREACH_HASH_VAL(files, data) {
			CURLcode err;
			zval **file, **type, **name;
			if (	SUCCESS == zend_hash_find(Z_ARRVAL_PP(data), "name", sizeof("name"), (void **) &name) &&
					SUCCESS == zend_hash_find(Z_ARRVAL_PP(data), "type", sizeof("type"), (void **) &type) &&
					SUCCESS == zend_hash_find(Z_ARRVAL_PP(data), "file", sizeof("file"), (void **) &file)) {
				err = curl_formadd(&http_post_data[0], &http_post_data[1],
					CURLFORM_COPYNAME,		Z_STRVAL_PP(name),
					CURLFORM_FILE,			Z_STRVAL_PP(file),
					CURLFORM_CONTENTTYPE,	Z_STRVAL_PP(type),
					CURLFORM_END
				);
				if (CURLE_OK != err) {
					http_error_ex(E_WARNING, HTTP_E_CURL, "Could not encode post files: %s", curl_easy_strerror(err));
					curl_formfree(http_post_data[0]);
					return FAILURE;
				}
			} else {
				http_error(E_NOTICE, HTTP_E_PARAM, "Post file array entry misses either 'name', 'type' or 'file' entry");
			}
		}

		body->type = HTTP_REQUEST_BODY_CURLPOST;
		body->data = http_post_data[0];
		body->size = 0;

	} else {
		char *encoded;
		size_t encoded_len;

		if (SUCCESS != http_urlencode_hash_ex(fields, 1, NULL, 0, &encoded, &encoded_len)) {
			http_error(E_WARNING, HTTP_E_ENCODE, "Could not encode post data");
			return FAILURE;
		}

		body->type = HTTP_REQUEST_BODY_CSTRING;
		body->data = encoded;
		body->size = encoded_len;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ void http_request_body_dtor(http_request_body *) */
PHP_HTTP_API void _http_request_body_dtor(http_request_body *body TSRMLS_DC)
{
	switch (body->type)
	{
		case HTTP_REQUEST_BODY_CSTRING:
			efree(body->data);
		break;

		case HTTP_REQUEST_BODY_CURLPOST:
			curl_formfree(body->data);
		break;

		case HTTP_REQUEST_BODY_UPLOADFILE:
			php_stream_close(body->data);
		break;
	}
}
/* }}} */

/* {{{ STATUS http_request_ex(CURL *, http_request_method, char *, http_request_body, HashTable, HashTable, phpstr *) */
PHP_HTTP_API STATUS _http_request_ex(CURL *ch, http_request_method meth, const char *url, http_request_body *body, HashTable *options, HashTable *info, phpstr *response TSRMLS_DC)
{
	CURLcode result = CURLE_OK;
	STATUS status = SUCCESS;
	zend_bool clean_curl = 0, range_req = 0;
	zval *zoption;

	/* check/init CURL handle */
	if (ch) {
#if LIBCURL_VERSION_NUM >= 0x070c01
		curl_easy_reset(ch);
#endif
	} else {
		if (ch = curl_easy_init()) {
			clean_curl = 1;
		} else {
			http_error(E_WARNING, HTTP_E_CURL, "Could not initialize curl");
			return FAILURE;
		}
	}

	/* set options */
	if (url) {
		HTTP_CURL_OPT(URL, url);
	}

	HTTP_CURL_OPT(HEADER, 0);
	HTTP_CURL_OPT(FILETIME, 1);
	HTTP_CURL_OPT(AUTOREFERER, 1);
	HTTP_CURL_OPT(READFUNCTION, http_curl_read_callback);
	HTTP_CURL_OPT(WRITEFUNCTION, http_curl_write_callback);
	HTTP_CURL_OPT(HEADERFUNCTION, http_curl_write_callback);

	if (response) {
		HTTP_CURL_OPT(WRITEDATA, response);
		HTTP_CURL_OPT(WRITEHEADER, response);
	}

#if defined(ZTS) && (LIBCURL_VERSION_NUM >= 0x070a00)
	HTTP_CURL_OPT(NOSIGNAL, 1);
#endif
#if LIBCURL_VERSION_NUM < 0x070c00
	HTTP_CURL_OPT(ERRORBUFFER, HTTP_G(request).curl.error);
#endif

	/* progress callback */
	if (zoption = http_curl_getopt(options, "onprogress", 0)) {
		HTTP_CURL_OPT(NOPROGRESS, 0);
		HTTP_CURL_OPT(PROGRESSFUNCTION, http_curl_progress_callback);
		HTTP_CURL_OPT(PROGRESSDATA, zoption);
	} else {
		HTTP_CURL_OPT(NOPROGRESS, 1);
	}

	/* debug callback */
	if (zoption = http_curl_getopt(options, "ondebug", 0)) {
		HTTP_CURL_OPT(VERBOSE, 1);
		HTTP_CURL_OPT(DEBUGFUNCTION, http_curl_debug_callback);
		HTTP_CURL_OPT(DEBUGDATA, zoption);
	} else {
		HTTP_CURL_OPT(VERBOSE, 0);
	}

	/* proxy */
	if (zoption = http_curl_getopt(options, "proxyhost", IS_STRING)) {
		HTTP_CURL_OPT(PROXY, http_curl_copystr(Z_STRVAL_P(zoption)));
		/* port */
		if (zoption = http_curl_getopt(options, "proxyport", IS_LONG)) {
			HTTP_CURL_OPT(PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if (zoption = http_curl_getopt(options, "proxyauth", IS_STRING)) {
			HTTP_CURL_OPT(PROXYUSERPWD, http_curl_copystr(Z_STRVAL_P(zoption)));
		}
#if LIBCURL_VERSION_NUM >= 0x070a07
		/* auth method */
		if (zoption = http_curl_getopt(options, "proxyauthtype", IS_LONG)) {
			HTTP_CURL_OPT(PROXYAUTH, Z_LVAL_P(zoption));
		}
#endif
	}

	/* outgoing interface */
	if (zoption = http_curl_getopt(options, "interface", IS_STRING)) {
		HTTP_CURL_OPT(INTERFACE, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* another port */
	if (zoption = http_curl_getopt(options, "port", IS_LONG)) {
		HTTP_CURL_OPT(PORT, Z_LVAL_P(zoption));
	}

	/* auth */
	if (zoption = http_curl_getopt(options, "httpauth", IS_STRING)) {
		HTTP_CURL_OPT(USERPWD, http_curl_copystr(Z_STRVAL_P(zoption)));
	}
#if LIBCURL_VERSION_NUM >= 0x070a06
	if (zoption = http_curl_getopt(options, "httpauthtype", IS_LONG)) {
		HTTP_CURL_OPT(HTTPAUTH, Z_LVAL_P(zoption));
	}
#endif

	/* compress, empty string enables deflate and gzip */
	if (zoption = http_curl_getopt(options, "compress", IS_BOOL)) {
		if (Z_LVAL_P(zoption)) {
			HTTP_CURL_OPT(ENCODING, http_curl_copystr(""));
		}
	}

	/* redirects, defaults to 0 */
	if (zoption = http_curl_getopt(options, "redirect", IS_LONG)) {
		HTTP_CURL_OPT(FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1 : 0);
		HTTP_CURL_OPT(MAXREDIRS, Z_LVAL_P(zoption));
		if (zoption = http_curl_getopt(options, "unrestrictedauth", IS_BOOL)) {
			HTTP_CURL_OPT(UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
	} else {
		HTTP_CURL_OPT(FOLLOWLOCATION, 0);
	}

	/* referer */
	if (zoption = http_curl_getopt(options, "referer", IS_STRING)) {
		HTTP_CURL_OPT(REFERER, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if (zoption = http_curl_getopt(options, "useragent", IS_STRING)) {
		HTTP_CURL_OPT(USERAGENT, http_curl_copystr(Z_STRVAL_P(zoption)));
	} else {
		HTTP_CURL_OPT(USERAGENT, "PECL::HTTP/" HTTP_PEXT_VERSION " (PHP/" PHP_VERSION ")");
	}

	/* additional headers, array('name' => 'value') */
	if (zoption = http_curl_getopt(options, "headers", IS_ARRAY)) {
		char *header_key;
		ulong header_idx;
		struct curl_slist *headers = NULL;

		FOREACH_KEY(zoption, header_key, header_idx) {
			if (header_key) {
				zval **header_val;
				if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &header_val)) {
					char header[1024] = {0};
					snprintf(header, 1023, "%s: %s", header_key, Z_STRVAL_PP(header_val));
					headers = curl_slist_append(headers, http_curl_copystr(header));
				}

				/* reset */
				header_key = NULL;
			}
		}

		if (headers) {
			HTTP_CURL_OPT(HTTPHEADER, headers);
		}
	}

	/* cookies, array('name' => 'value') */
	if (zoption = http_curl_getopt(options, "cookies", IS_ARRAY)) {
		char *cookie_key = NULL;
		ulong cookie_idx = 0;
		phpstr *qstr = phpstr_new();

		FOREACH_KEY(zoption, cookie_key, cookie_idx) {
			if (cookie_key) {
				zval **cookie_val;
				if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &cookie_val)) {
					phpstr_appendf(qstr, "%s=%s; ", cookie_key, Z_STRVAL_PP(cookie_val));
				}

				/* reset */
				cookie_key = NULL;
			}
		}

		if (qstr->used) {
			phpstr_fix(qstr);
			HTTP_CURL_OPT(COOKIE, http_curl_copystr(qstr->data));
		}
		phpstr_free(qstr);
	}

	/* cookiestore */
	if (zoption = http_curl_getopt(options, "cookiestore", IS_STRING)) {
		HTTP_CURL_OPT(COOKIEFILE, http_curl_copystr(Z_STRVAL_P(zoption)));
		HTTP_CURL_OPT(COOKIEJAR, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* resume */
	if (zoption = http_curl_getopt(options, "resume", IS_LONG)) {
		range_req = 1;
		HTTP_CURL_OPT(RESUME_FROM, Z_LVAL_P(zoption));
	}

	/* maxfilesize */
	if (zoption = http_curl_getopt(options, "maxfilesize", IS_LONG)) {
		HTTP_CURL_OPT(MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* lastmodified */
	if (zoption = http_curl_getopt(options, "lastmodified", IS_LONG)) {
		HTTP_CURL_OPT(TIMECONDITION, range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE);
		HTTP_CURL_OPT(TIMEVALUE, Z_LVAL_P(zoption));
	}

	/* timeout */
	if (zoption = http_curl_getopt(options, "timeout", IS_LONG)) {
		HTTP_CURL_OPT(TIMEOUT, Z_LVAL_P(zoption));
	}

	/* connecttimeout, defaults to 1 */
	if (zoption = http_curl_getopt(options, "connecttimeout", IS_LONG)) {
		HTTP_CURL_OPT(CONNECTTIMEOUT, Z_LVAL_P(zoption));
	} else {
		HTTP_CURL_OPT(CONNECTTIMEOUT, 1);
	}

	/* ssl */
	if (zoption = http_curl_getopt(options, "ssl", IS_ARRAY)) {
		ulong idx;
		char *key = NULL;
		zval **param;

		FOREACH_KEYVAL(zoption, key, idx, param) {
			if (key) {
				HTTP_CURL_OPT_SSL_STRING(CERT);
#if LIBCURL_VERSION_NUM >= 0x070903
				HTTP_CURL_OPT_SSL_STRING(CERTTYPE);
#endif
				HTTP_CURL_OPT_SSL_STRING(CERTPASSWD);

				HTTP_CURL_OPT_SSL_STRING(KEY);
				HTTP_CURL_OPT_SSL_STRING(KEYTYPE);
				HTTP_CURL_OPT_SSL_STRING(KEYPASSWD);

				HTTP_CURL_OPT_SSL_STRING(ENGINE);
				HTTP_CURL_OPT_SSL_LONG(VERSION);

				HTTP_CURL_OPT_SSL_LONG_(VERIFYPEER);
				HTTP_CURL_OPT_SSL_LONG_(VERIFYHOST);
				HTTP_CURL_OPT_SSL_STRING_(CIPHER_LIST);


				HTTP_CURL_OPT_STRING(CAINFO);
#if LIBCURL_VERSION_NUM >= 0x070908
				HTTP_CURL_OPT_STRING(CAPATH);
#endif
				HTTP_CURL_OPT_STRING(RANDOM_FILE);
				HTTP_CURL_OPT_STRING(EGDSOCKET);

				/* reset key */
				key = NULL;
			}
		}
	} else {
		/* disable SSL verification by default */
		HTTP_CURL_OPT(SSL_VERIFYPEER, 0);
		HTTP_CURL_OPT(SSL_VERIFYHOST, 0);
	}

	/* request method */
	switch (meth)
	{
		case HTTP_GET:
			curl_easy_setopt(ch, CURLOPT_HTTPGET, 1);
		break;

		case HTTP_HEAD:
			curl_easy_setopt(ch, CURLOPT_NOBODY, 1);
		break;

		case HTTP_POST:
			curl_easy_setopt(ch, CURLOPT_POST, 1);
		break;

		case HTTP_PUT:
			curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
		break;

		default:
			if (http_request_method_exists(0, meth, NULL)) {
				curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, http_request_method_name(meth));
			} else {
				http_error_ex(E_WARNING, HTTP_E_CURL, "Unsupported request method: %d", meth);
				status = FAILURE;
				goto http_request_end;
			}
		break;
	}

	/* attach request body */
	if (body && (meth != HTTP_GET) && (meth != HTTP_HEAD)) {
		switch (body->type)
		{
			case HTTP_REQUEST_BODY_CSTRING:
				curl_easy_setopt(ch, CURLOPT_POSTFIELDS, (char *) body->data);
				curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, body->size);
			break;

			case HTTP_REQUEST_BODY_CURLPOST:
				curl_easy_setopt(ch, CURLOPT_HTTPPOST, (struct curl_httppost *) body->data);
			break;

			case HTTP_REQUEST_BODY_UPLOADFILE:
			case HTTP_REQUEST_BODY_UPLOADDATA:
				curl_easy_setopt(ch, CURLOPT_READDATA, body);
				curl_easy_setopt(ch, CURLOPT_INFILESIZE, body->size);
			break;

			default:
				http_error_ex(E_WARNING, HTTP_E_CURL, "Unkown request body type: %d", body->type);
				status = FAILURE;
				goto http_request_end;
			break;
		}
	}

	/* perform request */
	if (CURLE_OK == (result = curl_easy_perform(ch))) {
		/* get curl info */
		if (info) {
			zval array;
			Z_ARRVAL(array) = info;

			HTTP_CURL_INFO(EFFECTIVE_URL);
#if LIBCURL_VERSION_NUM >= 0x070a07
			HTTP_CURL_INFO(RESPONSE_CODE);
#else
			HTTP_CURL_INFO_EX(HTTP_CODE, RESPONSE_CODE);
#endif
			HTTP_CURL_INFO(HTTP_CONNECTCODE);
#if LIBCURL_VERSION_NUM >= 0x070500
			HTTP_CURL_INFO(FILETIME);
#endif
			HTTP_CURL_INFO(TOTAL_TIME);
			HTTP_CURL_INFO(NAMELOOKUP_TIME);
			HTTP_CURL_INFO(CONNECT_TIME);
			HTTP_CURL_INFO(PRETRANSFER_TIME);
			HTTP_CURL_INFO(STARTTRANSFER_TIME);
#if LIBCURL_VERSION_NUM >= 0x070907
			HTTP_CURL_INFO(REDIRECT_TIME);
			HTTP_CURL_INFO(REDIRECT_COUNT);
#endif
			HTTP_CURL_INFO(SIZE_UPLOAD);
			HTTP_CURL_INFO(SIZE_DOWNLOAD);
			HTTP_CURL_INFO(SPEED_DOWNLOAD);
			HTTP_CURL_INFO(SPEED_UPLOAD);
			HTTP_CURL_INFO(HEADER_SIZE);
			HTTP_CURL_INFO(REQUEST_SIZE);
			HTTP_CURL_INFO(SSL_VERIFYRESULT);
#if LIBCURL_VERSION_NUM >= 0x070c03
			/*HTTP_CURL_INFO(SSL_ENGINES); todo: CURLINFO_SLIST */
#endif
			HTTP_CURL_INFO(CONTENT_LENGTH_DOWNLOAD);
			HTTP_CURL_INFO(CONTENT_LENGTH_UPLOAD);
			HTTP_CURL_INFO(CONTENT_TYPE);
#if LIBCURL_VERSION_NUM >= 0x070a03
			/*HTTP_CURL_INFO(PRIVATE);*/
#endif
#if LIBCURL_VERSION_NUM >= 0x070a08
			HTTP_CURL_INFO(HTTPAUTH_AVAIL);
			HTTP_CURL_INFO(PROXYAUTH_AVAIL);
#endif
#if LIBCURL_VERSION_NUM >= 0x070c02
			/*HTTP_CURL_INFO(OS_ERRNO);*/
#endif
#if LIBCURL_VERSION_NUM >= 0x070c03
			HTTP_CURL_INFO(NUM_CONNECTS);
#endif
		}
	} else {
		http_error_ex(E_WARNING, HTTP_E_CURL, "Could not perform request: %s", curl_easy_strerror(result));
		status = FAILURE;
	}

http_request_end:
	/* free strings copied with http_curl_copystr() */
	zend_llist_clean(&HTTP_G(request).curl.copies);

	/* clean curl handle if acquired */
	if (clean_curl) {
		curl_easy_cleanup(ch);
		ch = NULL;
	}

	/* finalize response */
	if (response) {
		phpstr_fix(PHPSTR(response));
	}

	return status;
}
/* }}} */

/* {{{ char *http_request_method_name(http_request_method) */
PHP_HTTP_API const char *_http_request_method_name(http_request_method m TSRMLS_DC)
{
	zval **meth;

	if (HTTP_STD_REQUEST_METHOD(m)) {
		return http_request_methods[m];
	}

	if (SUCCESS == zend_hash_index_find(&HTTP_G(request).methods.custom, HTTP_CUSTOM_REQUEST_METHOD(m), (void **) &meth)) {
		return Z_STRVAL_PP(meth);
	}

	return http_request_methods[0];
}
/* }}} */

/* {{{ unsigned long http_request_method_exists(zend_bool, unsigned long, char *) */
PHP_HTTP_API unsigned long _http_request_method_exists(zend_bool by_name, unsigned long id, const char *name TSRMLS_DC)
{
	if (by_name) {
		unsigned i;

		for (i = HTTP_NO_REQUEST_METHOD + 1; i < HTTP_MAX_REQUEST_METHOD; ++i) {
			if (!strcmp(name, http_request_methods[i])) {
				return i;
			}
		}
		{
			zval **data;
			char *key;
			ulong idx;

			FOREACH_HASH_KEYVAL(&HTTP_G(request).methods.custom, key, idx, data) {
				if (!strcmp(name, Z_STRVAL_PP(data))) {
					return idx + HTTP_MAX_REQUEST_METHOD;
				}
			}
		}
		return 0;
	} else {
		return HTTP_STD_REQUEST_METHOD(id) || zend_hash_index_exists(&HTTP_G(request).methods.custom, HTTP_CUSTOM_REQUEST_METHOD(id)) ? id : 0;
	}
}
/* }}} */

/* {{{ unsigned long http_request_method_register(char *) */
PHP_HTTP_API unsigned long _http_request_method_register(const char *method TSRMLS_DC)
{
	zval array;
	unsigned long meth_num = HTTP_G(request).methods.custom.nNextFreeElement + HTTP_MAX_REQUEST_METHOD;

	Z_ARRVAL(array) = &HTTP_G(request).methods.custom;
	add_next_index_string(&array, estrdup(method), 0);
	return meth_num;
}
/* }}} */

/* {{{ STATUS http_request_method_unregister(usngigned long) */
PHP_HTTP_API STATUS _http_request_method_unregister(unsigned long method TSRMLS_DC)
{
	return zend_hash_index_del(&HTTP_G(request).methods.custom, HTTP_CUSTOM_REQUEST_METHOD(method));
}
/* }}} */

/* {{{ char *http_request_methods[] */
static const char *const http_request_methods[] = {
	"UNKOWN",
	/* HTTP/1.1 */
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"DELETE",
	"OPTIONS",
	"TRACE",
	"CONNECT",
	/* WebDAV - RFC 2518 */
	"PROPFIND",
	"PROPPATCH",
	"MKCOL",
	"COPY",
	"MOVE",
	"LOCK",
	"UNLOCK",
	/* WebDAV Versioning - RFC 3253 */
	"VERSION-CONTROL",
	"REPORT",
	"CHECKOUT",
	"CHECKIN",
	"UNCHECKOUT",
	"MKWORKSPACE",
	"UPDATE",
	"LABEL",
	"MERGE",
	"BASELINE-CONTROL",
	"MKACTIVITY",
	/* WebDAV Access Control - RFC 3744 */
	"ACL",
	NULL
};
/* }}} */

/* {{{ static inline char *http_curl_copystr(char *) */
static inline char *_http_curl_copystr(const char *str TSRMLS_DC)
{
	char *new_str = estrdup(str);
	zend_llist_add_element(&HTTP_G(request).curl.copies, &new_str);
	return new_str;
}
/* }}} */

/* {{{ static size_t http_curl_write_callback(char *, size_t, size_t, void *) */
static size_t http_curl_write_callback(char *buf, size_t len, size_t n, void *s)
{
	return s ? phpstr_append(PHPSTR(s), buf, len * n) : len * n;
}
/* }}} */

/* {{{ static size_t http_curl_read_callback(void *, size_t, size_t, void *) */
static size_t http_curl_read_callback(void *data, size_t len, size_t n, void *s)
{
	static char *offset = NULL, *original = NULL;
	http_request_body *body = (http_request_body *) s;

	switch (body->type)
	{
		case HTTP_REQUEST_BODY_UPLOADFILE:
		{
			TSRMLS_FETCH();
			return php_stream_read((php_stream *) body->data, data, len * n);
		}
		break;

		case HTTP_REQUEST_BODY_UPLOADDATA:
		{
			size_t avail;
			if (original != s) {
				original = offset = s;
			}
			if ((avail = body->size - (offset - original)) < 1) {
				return 0;
			}
			if (avail < (len * n)) {
				memcpy(data, offset, avail);
				offset += avail;
				return avail;
			} else {
				memcpy(data, offset, len * n);
				offset += len * n;
				return len * n;
			}
		}
		break;

		default:
			return 0;
		break;
	}
}
/* }}} */

/* {{{ static int http_curl_progress_callback(void *, double, double, double, double) */
static int http_curl_progress_callback(void *data, double dltotal, double dlnow, double ultotal, double ulnow)
{
	zval *params_pass[4], params_local[4], retval, *func = (zval *) data;
	TSRMLS_FETCH();

	params_pass[0] = &params_local[0];
	params_pass[1] = &params_local[1];
	params_pass[2] = &params_local[2];
	params_pass[3] = &params_local[3];

	ZVAL_DOUBLE(params_pass[0], dltotal);
	ZVAL_DOUBLE(params_pass[1], dlnow);
	ZVAL_DOUBLE(params_pass[2], ultotal);
	ZVAL_DOUBLE(params_pass[3], ulnow);

	return call_user_function(EG(function_table), NULL, func, &retval, 4, params_pass TSRMLS_CC);
}
/* }}} */

static int http_curl_debug_callback(CURL *ch, curl_infotype type, char *string, size_t length, void *data)
{
	zval *params_pass[2], params_local[2], retval, *func = (zval *) data;
	TSRMLS_FETCH();

	params_pass[0] = &params_local[0];
	params_pass[1] = &params_local[1];

	ZVAL_LONG(params_pass[0], type);
	ZVAL_STRINGL(params_pass[1], string, length, 1);

	call_user_function(EG(function_table), NULL, func, &retval, 2, params_pass TSRMLS_CC);

	return 0;
}
/* {{{ static inline zval *http_curl_getopt(HashTable *, char *, size_t, int) */
static inline zval *_http_curl_getopt_ex(HashTable *options, char *key, size_t keylen, int type TSRMLS_DC)
{
	zval **zoption;

	if (!options || (SUCCESS != zend_hash_find(options, key, keylen, (void **) &zoption))) {
		return NULL;
	}

	if (Z_TYPE_PP(zoption) != type) {
		switch (type)
		{
			case IS_BOOL:	convert_to_boolean_ex(zoption);	break;
			case IS_LONG:	convert_to_long_ex(zoption);	break;
			case IS_DOUBLE:	convert_to_double_ex(zoption);	break;
			case IS_STRING:	convert_to_string_ex(zoption);	break;
			case IS_ARRAY:	convert_to_array_ex(zoption);	break;
			case IS_OBJECT: convert_to_object_ex(zoption);	break;
			default:
			break;
		}
	}

	return *zoption;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

