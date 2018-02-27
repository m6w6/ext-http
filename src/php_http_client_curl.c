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
#include "php_http_client.h"
#include "php_http_client_curl_event.h"
#include "php_http_client_curl_user.h"

#if PHP_HTTP_HAVE_LIBCURL

#if PHP_HTTP_HAVE_LIBCURL_OPENSSL
#	include <openssl/ssl.h>
#endif
#if PHP_HTTP_HAVE_LIBCURL_GNUTLS
#	include <gnutls/gnutls.h>
#endif

typedef struct php_http_client_curl_handler {
	CURL *handle;
	php_resource_factory_t *rf;
	php_http_client_t *client;
	php_http_client_progress_state_t progress;
	php_http_client_enqueue_t queue;

	struct {
		php_http_buffer_t headers;
		php_http_message_body_t *body;
	} response;

	struct {
		HashTable cache;

		struct curl_slist *proxyheaders;
		struct curl_slist *headers;
		struct curl_slist *resolve;
		php_http_buffer_t cookies;
		php_http_buffer_t ranges;

		struct {
			uint count;
			double delay;
		} retry;

		long redirects;
		unsigned range_request:1;
		unsigned encode_cookies:1;

	} options;

} php_http_client_curl_handler_t;

typedef struct php_http_curle_storage {
	char *url;
	char *cookiestore;
	CURLcode errorcode;
	char errorbuffer[0x100];
} php_http_curle_storage_t;

static inline php_http_curle_storage_t *php_http_curle_get_storage(CURL *ch) {
	php_http_curle_storage_t *st = NULL;

	curl_easy_getinfo(ch, CURLINFO_PRIVATE, &st);

	if (!st) {
		st = pecalloc(1, sizeof(*st), 1);
		curl_easy_setopt(ch, CURLOPT_PRIVATE, st);
		curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, st->errorbuffer);
	}

	return st;
}

static void *php_http_curle_ctor(void *opaque, void *init_arg)
{
	void *ch;

	if ((ch = curl_easy_init())) {
		php_http_curle_get_storage(ch);
		return ch;
	}
	return NULL;
}

static void *php_http_curle_copy(void *opaque, void *handle)
{
	void *ch;

	if ((ch = curl_easy_duphandle(handle))) {
		curl_easy_reset(ch);
		php_http_curle_get_storage(ch);
		return ch;
	}
	return NULL;
}

static void php_http_curle_dtor(void *opaque, void *handle)
{
	php_http_curle_storage_t *st = php_http_curle_get_storage(handle);

	curl_easy_cleanup(handle);

	if (st) {
		if (st->url) {
			pefree(st->url, 1);
		}
		if (st->cookiestore) {
			pefree(st->cookiestore, 1);
		}
		pefree(st, 1);
	}
}

static php_resource_factory_ops_t php_http_curle_resource_factory_ops = {
	php_http_curle_ctor,
	php_http_curle_copy,
	php_http_curle_dtor
};

static void *php_http_curlm_ctor(void *opaque, void *init_arg)
{
	php_http_client_curl_handle_t *curl = calloc(1, sizeof(*curl));

	if (!(curl->multi = curl_multi_init())) {
		free(curl);
		return NULL;
	}
	if (!(curl->share = curl_share_init())) {
		curl_multi_cleanup(curl->multi);
		free(curl);
		return NULL;
	}
	curl_share_setopt(curl->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
	curl_share_setopt(curl->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	return curl;
}

static void php_http_curlm_dtor(void *opaque, void *handle)
{
	php_http_client_curl_handle_t *curl = handle;

	curl_share_cleanup(curl->share);
	curl_multi_cleanup(curl->multi);
	free(handle);
}

static php_resource_factory_ops_t php_http_curlm_resource_factory_ops = {
	php_http_curlm_ctor,
	NULL,
	php_http_curlm_dtor
};

/* curl callbacks */

static size_t php_http_curle_read_callback(void *data, size_t len, size_t n, void *ctx)
{
	php_stream *s = php_http_message_body_stream(ctx);

	if (s) {
		return php_stream_read(s, data, len * n);
	}
	return 0;
}

#if PHP_HTTP_CURL_VERSION(7,32,0)
static int php_http_curle_xferinfo_callback(void *ctx, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
#else
static int php_http_curle_progress_callback(void *ctx, double dltotal, double dlnow, double ultotal, double ulnow)
#endif
{
	php_http_client_curl_handler_t *h = ctx;

	if (h->progress.dl.total != dltotal
	||	h->progress.dl.now != dlnow
	||	h->progress.ul.total != ultotal
	||	h->progress.ul.now != ulnow
	) {
		h->progress.dl.total = dltotal;
		h->progress.dl.now = dlnow;
		h->progress.ul.total = ultotal;
		h->progress.ul.now = ulnow;
	}

	if (h->client->callback.progress.func) {
		h->client->callback.progress.func(h->client->callback.progress.arg, h->client, &h->queue, &h->progress);
	}

	return 0;
}

static int php_http_curle_seek_callback(void *userdata, curl_off_t offset, int origin)
{
	php_http_message_body_t *body = userdata;

	if (!body) {
		return 1;
	}
	if (0 == php_stream_seek(php_http_message_body_stream(body), offset, origin)) {
		return 0;
	}
	return 2;
}

static int php_http_curle_raw_callback(CURL *ch, curl_infotype type, char *data, size_t length, void *ctx)
{
	php_http_client_curl_handler_t *h = ctx;
	unsigned utype = PHP_HTTP_CLIENT_DEBUG_INFO;

	/* catch progress */
	switch (type) {
		case CURLINFO_TEXT:
			if (data[0] == '-') {
				goto text;
			} else if (php_memnstr(data, ZEND_STRL("Adding handle:"), data + length)) {
				h->progress.info = "setup";
			} else if (php_memnstr(data, ZEND_STRL("addHandle"), data + length)) {
				h->progress.info = "setup";
			} else if (php_memnstr(data, ZEND_STRL("About to connect"), data + length)) {
				h->progress.info = "resolve";
			} else if (php_memnstr(data, ZEND_STRL("Trying"), data + length)) {
				h->progress.info = "connect";
			} else if (php_memnstr(data, ZEND_STRL("Found bundle for host"), data + length)) {
				h->progress.info = "connect";
			} else if (php_memnstr(data, ZEND_STRL("Connected"), data + length)) {
				h->progress.info = "connected";
			} else if (php_memnstr(data, ZEND_STRL("Re-using existing connection!"), data + length)) {
				h->progress.info = "connected";
			} else if (php_memnstr(data, ZEND_STRL("blacklisted"), data + length)) {
				h->progress.info = "blacklist check";
			} else if (php_memnstr(data, ZEND_STRL("TLS"), data + length)) {
				h->progress.info = "ssl negotiation";
			} else if (php_memnstr(data, ZEND_STRL("SSL"), data + length)) {
				h->progress.info = "ssl negotiation";
			} else if (php_memnstr(data, ZEND_STRL("certificate"), data + length)) {
				h->progress.info = "ssl negotiation";
			} else if (php_memnstr(data, ZEND_STRL("ALPN"), data + length)) {
				h->progress.info = "alpn";
			} else if (php_memnstr(data, ZEND_STRL("NPN"), data + length)) {
				h->progress.info = "npn";
			} else if (php_memnstr(data, ZEND_STRL("upload"), data + length)) {
				h->progress.info = "uploaded";
			} else if (php_memnstr(data, ZEND_STRL("left intact"), data + length)) {
				h->progress.info = "not disconnected";
			} else if (php_memnstr(data, ZEND_STRL("closed"), data + length)) {
				h->progress.info = "disconnected";
			} else if (php_memnstr(data, ZEND_STRL("Issue another request"), data + length)) {
				h->progress.info = "redirect";
			} else if (php_memnstr(data, ZEND_STRL("Operation timed out"), data + length)) {
				h->progress.info = "timeout";
			} else {
				text:;
#if 0
				h->progress.info = data;
				data[length - 1] = '\0';
#endif
			}
			if (h->client->callback.progress.func) {
				h->client->callback.progress.func(h->client->callback.progress.arg, h->client, &h->queue, &h->progress);
			}
			break;

		case CURLINFO_HEADER_OUT:
			utype |= PHP_HTTP_CLIENT_DEBUG_HEADER;
			goto data_out;

		case CURLINFO_SSL_DATA_OUT:
			utype |= PHP_HTTP_CLIENT_DEBUG_SSL;
			goto data_out;

		case CURLINFO_DATA_OUT:
		data_out:
			utype |= PHP_HTTP_CLIENT_DEBUG_OUT;
			h->progress.info = "send";
			break;

		case CURLINFO_HEADER_IN:
			utype |= PHP_HTTP_CLIENT_DEBUG_HEADER;
			goto data_in;

		case CURLINFO_SSL_DATA_IN:
			utype |= PHP_HTTP_CLIENT_DEBUG_SSL;
			goto data_in;

		case CURLINFO_DATA_IN:
		data_in:
			utype |= PHP_HTTP_CLIENT_DEBUG_IN;
			h->progress.info = "receive";
			break;

		default:
			break;
	}

	if (h->client->callback.debug.func) {
		h->client->callback.debug.func(h->client->callback.debug.arg, h->client, &h->queue, utype, data, length);
	}

#if 0
	/* debug */
	_dpf(type, data, length);
#endif

	return 0;
}

static int php_http_curle_header_callback(char *data, size_t n, size_t l, void *arg)
{
	php_http_client_curl_handler_t *h = arg;

	return php_http_buffer_append(&h->response.headers, data, n * l);
}

static int php_http_curle_body_callback(char *data, size_t n, size_t l, void *arg)
{
	php_http_client_curl_handler_t *h = arg;

	return php_http_message_body_append(h->response.body, data, n*l);
}

static ZEND_RESULT_CODE php_http_curle_get_info(CURL *ch, HashTable *info)
{
	char *c = NULL;
	long l = 0;
	double d = 0;
	struct curl_slist *s = NULL, *p = NULL;
	zval tmp = {{0}};

	/* BEGIN::CURLINFO */
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_EFFECTIVE_URL, &c)) {
		ZVAL_STRING(&tmp, STR_PTR(c));
		zend_hash_str_update(info, "effective_url", lenof("effective_url"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "response_code", lenof("response_code"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_TOTAL_TIME, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "total_time", lenof("total_time"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_NAMELOOKUP_TIME, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "namelookup_time", lenof("namelookup_time"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONNECT_TIME, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "connect_time", lenof("connect_time"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PRETRANSFER_TIME, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "pretransfer_time", lenof("pretransfer_time"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SIZE_UPLOAD, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "size_upload", lenof("size_upload"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SIZE_DOWNLOAD, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "size_download", lenof("size_download"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SPEED_DOWNLOAD, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "speed_download", lenof("speed_download"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SPEED_UPLOAD, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "speed_upload", lenof("speed_upload"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_HEADER_SIZE, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "header_size", lenof("header_size"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REQUEST_SIZE, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "request_size", lenof("request_size"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SSL_VERIFYRESULT, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "ssl_verifyresult", lenof("ssl_verifyresult"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_FILETIME, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "filetime", lenof("filetime"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "content_length_download", lenof("content_length_download"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONTENT_LENGTH_UPLOAD, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "content_length_upload", lenof("content_length_upload"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_STARTTRANSFER_TIME, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "starttransfer_time", lenof("starttransfer_time"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONTENT_TYPE, &c)) {
		ZVAL_STRING(&tmp, STR_PTR(c));
		zend_hash_str_update(info, "content_type", lenof("content_type"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REDIRECT_TIME, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "redirect_time", lenof("redirect_time"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REDIRECT_COUNT, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "redirect_count", lenof("redirect_count"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_HTTP_CONNECTCODE, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "connect_code", lenof("connect_code"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_HTTPAUTH_AVAIL, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "httpauth_avail", lenof("httpauth_avail"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PROXYAUTH_AVAIL, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "proxyauth_avail", lenof("proxyauth_avail"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_OS_ERRNO, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "os_errno", lenof("os_errno"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_NUM_CONNECTS, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "num_connects", lenof("num_connects"), &tmp);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SSL_ENGINES, &s)) {
		array_init(&tmp);
		for (p = s; p; p = p->next) {
			if (p->data) {
				add_next_index_string(&tmp, p->data);
			}
		}
		zend_hash_str_update(info, "ssl_engines", lenof("ssl_engines"), &tmp);
		curl_slist_free_all(s);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REDIRECT_URL, &c)) {
		ZVAL_STRING(&tmp, STR_PTR(c));
		zend_hash_str_update(info, "redirect_url", lenof("redirect_url"), &tmp);
	}
#if PHP_HTTP_CURL_VERSION(7,19,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PRIMARY_IP, &c)) {
		ZVAL_STRING(&tmp, STR_PTR(c));
		zend_hash_str_update(info, "primary_ip", lenof("primary_ip"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,19,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_APPCONNECT_TIME, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "appconnect_time", lenof("appconnect_time"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,19,4)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONDITION_UNMET, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "condition_unmet", lenof("condition_unmet"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,21,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PRIMARY_PORT, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "primary_port", lenof("primary_port"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,21,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_LOCAL_IP, &c)) {
		ZVAL_STRING(&tmp, STR_PTR(c));
		zend_hash_str_update(info, "local_ip", lenof("local_ip"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,21,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_LOCAL_PORT, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "local_port", lenof("local_port"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,50,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_HTTP_VERSION, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "http_version", lenof("http_version"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,52,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PROXY_SSL_VERIFYRESULT, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "proxy_ssl_verifyresult", lenof("proxy_ssl_verifyresult"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,52,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PROTOCOL, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "protocol", lenof("protocol"), &tmp);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,52,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SCHEME, &c)) {
		ZVAL_STRING(&tmp, STR_PTR(c));
		zend_hash_str_update(info, "scheme", lenof("scheme"), &tmp);
	}
#endif

	/* END::CURLINFO */

#if PHP_HTTP_CURL_VERSION(7,34,0)
	{
		zval ti_array, subarray;
		struct curl_tlssessioninfo *ti;

		if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_TLS_SESSION, &ti)) {
			char *backend;

			ZVAL_NULL(&subarray);
			array_init(&ti_array);

			switch (ti->backend) {
			case CURLSSLBACKEND_NONE:
				backend = "none";
				break;
			case CURLSSLBACKEND_OPENSSL:
				backend = "openssl";
#if PHP_HTTP_HAVE_LIBCURL_OPENSSL
				{
					SSL_CTX *ctx = ti->internals;

					array_init(&subarray);
					add_assoc_long_ex(&subarray, ZEND_STRL("number"), SSL_CTX_sess_number(ctx));
					add_assoc_long_ex(&subarray, ZEND_STRL("connect"), SSL_CTX_sess_connect(ctx));
					add_assoc_long_ex(&subarray, ZEND_STRL("connect_good"), SSL_CTX_sess_connect_good(ctx));
					add_assoc_long_ex(&subarray, ZEND_STRL("connect_renegotiate"), SSL_CTX_sess_connect_renegotiate(ctx));
					add_assoc_long_ex(&subarray, ZEND_STRL("hits"), SSL_CTX_sess_hits(ctx));
					add_assoc_long_ex(&subarray, ZEND_STRL("cache_full"), SSL_CTX_sess_cache_full(ctx));
				}
#endif
				break;
			case CURLSSLBACKEND_GNUTLS:
				backend = "gnutls";
#if PHP_HTTP_HAVE_LIBCURL_GNUTLS
				{
					gnutls_session_t sess = ti->internals;
					char *desc;

					array_init(&subarray);
					if ((desc = gnutls_session_get_desc(sess))) {
						add_assoc_string_ex(&subarray, ZEND_STRL("desc"), desc);
						gnutls_free(desc);
					}
					add_assoc_bool_ex(&subarray, ZEND_STRL("resumed"), gnutls_session_is_resumed(sess));
				}
#endif
				break;
			case CURLSSLBACKEND_NSS:
				backend = "nss";
				break;
#if !PHP_HTTP_CURL_VERSION(7,39,0)
			case CURLSSLBACKEND_QSOSSL:
				backend = "qsossl";
				break;
#else
			case CURLSSLBACKEND_GSKIT:
				backend = "gskit";
				break;
#endif
			case CURLSSLBACKEND_POLARSSL:
				backend = "polarssl";
				break;
			case CURLSSLBACKEND_CYASSL:
				backend = "cyassl";
				break;
			case CURLSSLBACKEND_SCHANNEL:
				backend = "schannel";
				break;
			case CURLSSLBACKEND_DARWINSSL:
				backend = "darwinssl";
				break;
			default:
				backend = "unknown";
			}
			add_assoc_string_ex(&ti_array, ZEND_STRL("backend"), backend);
			add_assoc_zval_ex(&ti_array, ZEND_STRL("internals"), &subarray);
			zend_hash_str_update(info, "tls_session", lenof("tls_session"), &ti_array);
		}
	}
#endif

#if (PHP_HTTP_CURL_VERSION(7,19,1) && PHP_HTTP_HAVE_LIBCURL_OPENSSL) || \
	(PHP_HTTP_CURL_VERSION(7,34,0) && PHP_HTTP_HAVE_LIBCURL_NSS) || \
	(PHP_HTTP_CURL_VERSION(7,42,0) && PHP_HTTP_HAVE_LIBCURL_GNUTLS) || \
	(PHP_HTTP_CURL_VERSION(7,39,0) && PHP_HTTP_HAVE_LIBCURL_GSKIT)
	{
		int i;
		zval ci_array, subarray;
		struct curl_certinfo *ci;
		char *colon, *keyname;

		if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CERTINFO, &ci)) {
			array_init(&ci_array);

			for (i = 0; i < ci->num_of_certs; ++i) {
				s = ci->certinfo[i];

				array_init(&subarray);
				for (p = s; p; p = p->next) {
					if (p->data) {
						if ((colon = strchr(p->data, ':'))) {
							keyname = estrndup(p->data, colon - p->data);
							add_assoc_string_ex(&subarray, keyname, colon - p->data, colon + 1);
							efree(keyname);
						} else {
							add_next_index_string(&subarray, p->data);
						}
					}
				}
				add_next_index_zval(&ci_array, &subarray);
			}
			zend_hash_str_update(info, "certinfo", lenof("certinfo"), &ci_array);
		}
	}
#endif
	{
		php_http_curle_storage_t *st = php_http_curle_get_storage(ch);

		ZVAL_LONG(&tmp, st->errorcode);
		zend_hash_str_update(info, "curlcode", lenof("curlcode"), &tmp);
		ZVAL_STRING(&tmp, st->errorbuffer);
		zend_hash_str_update(info, "error", lenof("error"), &tmp);
	}

	return SUCCESS;
}

static int compare_queue(php_http_client_enqueue_t *e, void *handle)
{
	return handle == ((php_http_client_curl_handler_t *) e->opaque)->handle;
}

static php_http_message_t *php_http_curlm_responseparser(php_http_client_curl_handler_t *h)
{
	php_http_message_t *response;
	php_http_header_parser_t parser;
	zval *zh, tmp;

	response = php_http_message_init(NULL, 0, h->response.body);
	php_http_header_parser_init(&parser);
	while (h->response.headers.used) {
		php_http_header_parser_state_t st = php_http_header_parser_parse(&parser,
				&h->response.headers, PHP_HTTP_HEADER_PARSER_CLEANUP, &response->hdrs,
				(php_http_info_callback_t) php_http_message_info_callback, (void *) &response);
		if (PHP_HTTP_HEADER_PARSER_STATE_FAILURE == st) {
			break;
		}
	}
	php_http_header_parser_dtor(&parser);

	/* move body to right message */
	if (response->body != h->response.body) {
		php_http_message_t *ptr = response;

		while (ptr->parent) {
			ptr = ptr->parent;
		}
		php_http_message_body_free(&response->body);
		response->body = ptr->body;
		ptr->body = NULL;
	}
	php_http_message_body_addref(h->response.body);

	/* let's update the response headers */
	if ((zh = php_http_message_header(response, ZEND_STRL("Content-Length")))) {
		ZVAL_COPY(&tmp, zh);
		zend_hash_str_update(&response->hdrs, "X-Original-Content-Length", lenof("X-Original-Content-Length"), &tmp);
	}
	if ((zh = php_http_message_header(response, ZEND_STRL("Transfer-Encoding")))) {
		ZVAL_COPY(&tmp, zh);
		zend_hash_str_del(&response->hdrs, "Transfer-Encoding", lenof("Transfer-Encoding"));
		zend_hash_str_update(&response->hdrs, "X-Original-Transfer-Encoding", lenof("X-Original-Transfer-Encoding"), &tmp);
	}
	if ((zh = php_http_message_header(response, ZEND_STRL("Content-Range")))) {
		ZVAL_COPY(&tmp, zh);
		zend_hash_str_del(&response->hdrs, "Content-Range", lenof("Content-Range"));
		zend_hash_str_update(&response->hdrs, "X-Original-Content-Range", lenof("X-Original-Content-Range"), &tmp);
	}
	if ((zh = php_http_message_header(response, ZEND_STRL("Content-Encoding")))) {
		ZVAL_COPY(&tmp, zh);
		zend_hash_str_del(&response->hdrs, "Content-Encoding", lenof("Content-Encoding"));
		zend_hash_str_update(&response->hdrs, "X-Original-Content-Encoding", lenof("X-Original-Content-Encoding"), &tmp);
	}
	php_http_message_update_headers(response);

	return response;
}

void php_http_client_curl_responsehandler(php_http_client_t *context)
{
	int err_count = 0, remaining = 0;
	php_http_curle_storage_t *st, *err = NULL;
	php_http_client_enqueue_t *enqueue;
	php_http_client_curl_t *curl = context->ctx;

	do {
		CURLMsg *msg = curl_multi_info_read(curl->handle->multi, &remaining);

		if (msg && CURLMSG_DONE == msg->msg) {
			if (CURLE_OK != msg->data.result) {
				st = php_http_curle_get_storage(msg->easy_handle);
				st->errorcode = msg->data.result;

				/* defer the warnings/exceptions, so the callback is still called for this request */
				if (!err) {
					err = ecalloc(remaining + 1, sizeof(*err));
				}
				memcpy(&err[err_count], st, sizeof(*st));
				if (st->url) {
					err[err_count].url = estrdup(st->url);
				}
				err_count++;
			}

			if ((enqueue = php_http_client_enqueued(context, msg->easy_handle, compare_queue))) {
				php_http_client_curl_handler_t *handler = enqueue->opaque;
				php_http_message_t *response = php_http_curlm_responseparser(handler);

				if (response) {
					context->callback.response.func(context->callback.response.arg, context, &handler->queue, &response);
					php_http_message_free(&response);
				}
			}
		}
	} while (remaining);

	if (err_count) {
		int i = 0;

		do {
			php_error_docref(NULL, E_WARNING, "%s; %s (%s)", curl_easy_strerror(err[i].errorcode), err[i].errorbuffer, STR_PTR(err[i].url));
			if (err[i].url) {
				efree(err[i].url);
			}
		} while (++i < err_count);

		efree(err);
	}
}

void php_http_client_curl_loop(php_http_client_t *client, curl_socket_t s, int curl_action)
{
	CURLMcode rc;
	php_http_client_curl_t *curl = client->ctx;

#if DBG_EVENTS
	fprintf(stderr, "H");
#endif

	do {
		rc = curl_multi_socket_action(curl->handle->multi, s, curl_action, &curl->unfinished);
	} while (CURLM_CALL_MULTI_PERFORM == rc);

	if (CURLM_OK != rc) {
		php_error_docref(NULL, E_WARNING, "%s",  curl_multi_strerror(rc));
	}

	php_http_client_curl_responsehandler(client);
}

/* curl options */

static php_http_options_t php_http_curle_options, php_http_curlm_options;

#define PHP_HTTP_CURLE_OPTION_CHECK_STRLEN		0x0001
#define PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR		0x0002
#define PHP_HTTP_CURLE_OPTION_TRANSFORM_MS		0x0004

static ZEND_RESULT_CODE php_http_curle_option_set_ssl_verifyhost(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, Z_TYPE_P(val) == IS_TRUE ? 2 : 0)) {
		return FAILURE;
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_cookiesession(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_COOKIESESSION, (long) (Z_TYPE_P(val) == IS_TRUE))) {
		return FAILURE;
	}
	if (Z_TYPE_P(val) == IS_TRUE) {
		if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_COOKIELIST, "SESS")) {
			return FAILURE;
		}
	}

	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_cookiestore(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;
	php_http_curle_storage_t *storage = php_http_curle_get_storage(curl->handle);

	if (storage->cookiestore) {
		pefree(storage->cookiestore, 1);
	}
	if (val && Z_TYPE_P(val) == IS_STRING && Z_STRLEN_P(val)) {
		storage->cookiestore = pestrndup(Z_STRVAL_P(val), Z_STRLEN_P(val), 1);
	} else {
		storage->cookiestore = NULL;
	}
	if (	CURLE_OK != curl_easy_setopt(ch, CURLOPT_COOKIEFILE, storage->cookiestore)
		||	CURLE_OK != curl_easy_setopt(ch, CURLOPT_COOKIEJAR, storage->cookiestore)
	) {
		return FAILURE;
	}

	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_cookies(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (val && Z_TYPE_P(val) != IS_NULL) {
		HashTable *ht = HASH_OF(val);

		if (curl->options.encode_cookies) {
			if (SUCCESS == php_http_url_encode_hash_ex(ht, &curl->options.cookies, ZEND_STRL(";"), ZEND_STRL("="), NULL, 0)) {
				php_http_buffer_fix(&curl->options.cookies);
				if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_COOKIE, curl->options.cookies.data)) {
					return FAILURE;
				}
			} else {
				return FAILURE;
			}
		} else {
			php_http_arrkey_t cookie_key;
			zval *cookie_val;

			ZEND_HASH_FOREACH_KEY_VAL(ht, cookie_key.h, cookie_key.key, cookie_val)
			{
				zend_string *zs = zval_get_string(cookie_val);

				php_http_arrkey_stringify(&cookie_key, NULL);
				php_http_buffer_appendf(&curl->options.cookies, "%s=%s; ", cookie_key.key->val, zs->val);
				php_http_arrkey_dtor(&cookie_key);

				zend_string_release(zs);
			}
			ZEND_HASH_FOREACH_END();

			php_http_buffer_fix(&curl->options.cookies);
			if (curl->options.cookies.used) {
				if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_COOKIE, curl->options.cookies.data)) {
					return FAILURE;
				}
			}
		}
	} else {
		php_http_buffer_reset(&curl->options.cookies);
		if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_COOKIE, NULL)) {
			return FAILURE;
		}
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_encodecookies(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;

	curl->options.encode_cookies = Z_TYPE_P(val) == IS_TRUE;
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_lastmodified(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (Z_LVAL_P(val)) {
		if (Z_LVAL_P(val) > 0) {
			if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_TIMEVALUE, Z_LVAL_P(val))) {
				return FAILURE;
			}
		} else {
			if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_TIMEVALUE, (long) sapi_get_request_time() + Z_LVAL_P(val))) {
				return FAILURE;
			}
		}
		if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_TIMECONDITION, (long) (curl->options.range_request ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE))) {
			return FAILURE;
		}
	} else {
		if (	CURLE_OK != curl_easy_setopt(ch, CURLOPT_TIMEVALUE, 0)
			||	CURLE_OK != curl_easy_setopt(ch, CURLOPT_TIMECONDITION, 0)
		) {
			return FAILURE;
		}
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_compress(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

#if !PHP_HTTP_CURL_VERSION(7,21,6)
#	define CURLOPT_ACCEPT_ENCODING CURLOPT_ENCODING
#endif
	if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_ACCEPT_ENCODING, Z_TYPE_P(val) == IS_TRUE ? "" : NULL)) {
		return FAILURE;
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_etag(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	php_http_buffer_t header;

	if (val && Z_TYPE_P(val) == IS_STRING && Z_STRLEN_P(val)) {
		zend_bool is_quoted = !((Z_STRVAL_P(val)[0] != '"') || (Z_STRVAL_P(val)[Z_STRLEN_P(val)-1] != '"'));
		php_http_buffer_init(&header);
		php_http_buffer_appendf(&header, is_quoted?"%s: %s":"%s: \"%s\"", curl->options.range_request?"If-Match":"If-None-Match", Z_STRVAL_P(val));
		php_http_buffer_fix(&header);
		curl->options.headers = curl_slist_append(curl->options.headers, header.data);
		php_http_buffer_dtor(&header);
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_range(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	php_http_buffer_reset(&curl->options.ranges);

	if (val && Z_TYPE_P(val) != IS_NULL) {
		zval *rr, *rb, *re;
		HashTable *ht = HASH_OF(val);

		ZEND_HASH_FOREACH_VAL(ht, rr)
		{
			if (Z_TYPE_P(rr) == IS_ARRAY) {
				if (2 == php_http_array_list(Z_ARRVAL_P(rr), 2, &rb, &re)) {
					zend_long rbl = zval_get_long(rb), rel = zval_get_long(re);

					if (rbl >= 0) {
						if (rel > 0) {
							php_http_buffer_appendf(&curl->options.ranges, "%ld-%ld,", rbl, rel);
						} else {
							php_http_buffer_appendf(&curl->options.ranges, "%ld-", rbl);
						}
					} else if (rel > 0) {
						php_http_buffer_appendf(&curl->options.ranges, "-%ld", rel);
					}
				}
			}
		}
		ZEND_HASH_FOREACH_END();

		if (curl->options.ranges.used) {
			curl->options.range_request = 1;
			/* ditch last comma */
			curl->options.ranges.data[curl->options.ranges.used - 1] = '\0';
		}
	}

	if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_RANGE, curl->options.ranges.data)) {
		return FAILURE;
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_resume(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (Z_LVAL_P(val) > 0) {
		curl->options.range_request = 1;
	}
	if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_RESUME_FROM, Z_LVAL_P(val))) {
		return FAILURE;
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_retrydelay(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;

	curl->options.retry.delay = Z_DVAL_P(val);
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_retrycount(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;

	curl->options.retry.count = Z_LVAL_P(val);
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_redirect(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (	CURLE_OK != curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, Z_LVAL_P(val) ? 1L : 0L)
		||	CURLE_OK != curl_easy_setopt(ch, CURLOPT_MAXREDIRS, curl->options.redirects = Z_LVAL_P(val))
	) {
		return FAILURE;
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curle_option_set_portrange(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;
	long localport = 0, localportrange = 0;

	if (val && Z_TYPE_P(val) != IS_NULL) {
		zval *zps, *zpe;

		switch (php_http_array_list(Z_ARRVAL_P(val), 2, &zps, &zpe)) {
		case 2:
			localportrange = labs(zval_get_long(zps)-zval_get_long(zpe))+1L;
			/* no break */
		case 1:
			localport = (zval_get_long(zpe) > 0) ? MIN(zval_get_long(zps), zval_get_long(zpe)) : zval_get_long(zps);
			break;
		default:
			break;
		}
	}
	if (	CURLE_OK != curl_easy_setopt(ch, CURLOPT_LOCALPORT, localport)
		||	CURLE_OK != curl_easy_setopt(ch, CURLOPT_LOCALPORTRANGE, localportrange)
	) {
		return FAILURE;
	}
	return SUCCESS;
}

#if PHP_HTTP_CURL_VERSION(7,37,0)
static ZEND_RESULT_CODE php_http_curle_option_set_proxyheader(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;

	if (val && Z_TYPE_P(val) != IS_NULL) {
		php_http_arrkey_t header_key;
		zval *header_val;
		php_http_buffer_t header;

		php_http_buffer_init(&header);
		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(val), header_key.h, header_key.key, header_val)
		{
			if (header_key.key) {
				zend_string *zs = zval_get_string(header_val);

				php_http_buffer_appendf(&header, "%s: %s", header_key.key->val, zs->val);
				zend_string_release(zs);

				php_http_buffer_fix(&header);
				curl->options.proxyheaders = curl_slist_append(curl->options.proxyheaders, header.data);
				php_http_buffer_reset(&header);

			}
		}
		ZEND_HASH_FOREACH_END();
		php_http_buffer_dtor(&header);
	}
	if (CURLE_OK != curl_easy_setopt(curl->handle, CURLOPT_PROXYHEADER, curl->options.proxyheaders)) {
		return FAILURE;
	}
	if (CURLE_OK != curl_easy_setopt(curl->handle, CURLOPT_HEADEROPT, CURLHEADER_SEPARATE)) {
		curl_easy_setopt(curl->handle, CURLOPT_PROXYHEADER, NULL);
		return FAILURE;
	}
	return SUCCESS;
}
#endif

#if PHP_HTTP_CURL_VERSION(7,21,3)
static ZEND_RESULT_CODE php_http_curle_option_set_resolve(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (val && Z_TYPE_P(val) != IS_NULL) {
		HashTable *ht = HASH_OF(val);
		zval *data;

		ZEND_HASH_FOREACH_VAL(ht, data)
		{
			zend_string *zs = zval_get_string(data);
			curl->options.resolve = curl_slist_append(curl->options.resolve, zs->val);
			zend_string_release(zs);
		}
		ZEND_HASH_FOREACH_END();

		if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_RESOLVE, curl->options.resolve)) {
			return FAILURE;
		}
	} else {
		if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_RESOLVE, NULL)) {
			return FAILURE;
		}
	}
	return SUCCESS;
}
#endif

#if PHP_HTTP_CURL_VERSION(7,21,4) && PHP_HTTP_HAVE_LIBCURL_TLSAUTH_TYPE
static ZEND_RESULT_CODE php_http_curle_option_set_ssl_tlsauthtype(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;

	if (val && Z_LVAL_P(val)) {
		switch (Z_LVAL_P(val)) {
		case CURL_TLSAUTH_SRP:
			if (CURLE_OK == curl_easy_setopt(ch, CURLOPT_TLSAUTH_TYPE, PHP_HTTP_LIBCURL_TLSAUTH_SRP)) {
				return SUCCESS;
			}
			/* no break */
		default:
			return FAILURE;
		}
	}
	if (CURLE_OK != curl_easy_setopt(ch, CURLOPT_TLSAUTH_TYPE, PHP_HTTP_LIBCURL_TLSAUTH_DEF)) {
		return FAILURE;
	}
	return SUCCESS;
}
#endif

static void php_http_curle_options_init(php_http_options_t *registry)
{
	php_http_option_t *opt;

	/* url options */
#if PHP_HTTP_CURL_VERSION(7,42,0)
	php_http_option_register(registry, ZEND_STRL("path_as_is"), CURLOPT_PATH_AS_IS, _IS_BOOL);
#endif

	/* proxy */
	php_http_option_register(registry, ZEND_STRL("proxyhost"), CURLOPT_PROXY, IS_STRING);
	php_http_option_register(registry, ZEND_STRL("proxytype"), CURLOPT_PROXYTYPE, IS_LONG);
	php_http_option_register(registry, ZEND_STRL("proxyport"), CURLOPT_PROXYPORT, IS_LONG);
	if ((opt = php_http_option_register(registry, ZEND_STRL("proxyauth"), CURLOPT_PROXYUSERPWD, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("proxyauthtype"), CURLOPT_PROXYAUTH, IS_LONG))) {
		Z_LVAL(opt->defval) = CURLAUTH_ANYSAFE;
	}
	php_http_option_register(registry, ZEND_STRL("proxytunnel"), CURLOPT_HTTPPROXYTUNNEL, _IS_BOOL);
#if PHP_HTTP_CURL_VERSION(7,19,4)
	php_http_option_register(registry, ZEND_STRL("noproxy"), CURLOPT_NOPROXY, IS_STRING);
#endif

#if PHP_HTTP_CURL_VERSION(7,37,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("proxyheader"), CURLOPT_PROXYHEADER, IS_ARRAY))) {
		opt->setter = php_http_curle_option_set_proxyheader;
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,43,0)
	if (PHP_HTTP_CURL_FEATURE(CURL_VERSION_GSSAPI)
	&& (opt = php_http_option_register(registry, ZEND_STRL("proxy_service_name"), CURLOPT_PROXY_SERVICE_NAME, IS_STRING))
	) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
#endif

#if PHP_HTTP_CURL_VERSION(7,40,0)
	if (PHP_HTTP_CURL_FEATURE(CURL_VERSION_UNIX_SOCKETS)) {
		if ((opt = php_http_option_register(registry, ZEND_STRL("unix_socket_path"), CURLOPT_UNIX_SOCKET_PATH, IS_STRING))) {
			opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
			opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
		}
	}
#endif

	/* dns */
	if ((opt = php_http_option_register(registry, ZEND_STRL("dns_cache_timeout"), CURLOPT_DNS_CACHE_TIMEOUT, IS_LONG))) {
		Z_LVAL(opt->defval) = 60;
	}
	php_http_option_register(registry, ZEND_STRL("ipresolve"), CURLOPT_IPRESOLVE, IS_LONG);
#if PHP_HTTP_CURL_VERSION(7,21,3)
	if ((opt = php_http_option_register(registry, ZEND_STRL("resolve"), CURLOPT_RESOLVE, IS_ARRAY))) {
		opt->setter = php_http_curle_option_set_resolve;
	}
#endif
#if PHP_HTTP_HAVE_LIBCURL_ARES
# if PHP_HTTP_CURL_VERSION(7,24,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("dns_servers"), CURLOPT_DNS_SERVERS, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
# endif
# if PHP_HTTP_CURL_VERSION(7,33,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("dns_interface"), CURLOPT_DNS_INTERFACE, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("dns_local_ip4"), CURLOPT_DNS_LOCAL_IP4, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("dns_local_ip6"), CURLOPT_DNS_LOCAL_IP6, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
# endif
#endif

	/* limits */
	php_http_option_register(registry, ZEND_STRL("low_speed_limit"), CURLOPT_LOW_SPEED_LIMIT, IS_LONG);
	php_http_option_register(registry, ZEND_STRL("low_speed_time"), CURLOPT_LOW_SPEED_TIME, IS_LONG);

	/* LSF weirdance
	php_http_option_register(registry, ZEND_STRL("max_send_speed"), CURLOPT_MAX_SEND_SPEED_LARGE, IS_LONG);
	php_http_option_register(registry, ZEND_STRL("max_recv_speed"), CURLOPT_MAX_RECV_SPEED_LARGE, IS_LONG);
	*/

	/* connection handling */
	/* crashes
	if ((opt = php_http_option_register(registry, ZEND_STRL("maxconnects"), CURLOPT_MAXCONNECTS, IS_LONG))) {
		Z_LVAL(opt->defval) = 5;
	}
	*/
	php_http_option_register(registry, ZEND_STRL("fresh_connect"), CURLOPT_FRESH_CONNECT, _IS_BOOL);
	php_http_option_register(registry, ZEND_STRL("forbid_reuse"), CURLOPT_FORBID_REUSE, _IS_BOOL);

	/* outgoing interface */
	php_http_option_register(registry, ZEND_STRL("interface"), CURLOPT_INTERFACE, IS_STRING);
	if ((opt = php_http_option_register(registry, ZEND_STRL("portrange"), CURLOPT_LOCALPORT, IS_ARRAY))) {
		opt->setter = php_http_curle_option_set_portrange;
	}

	/* another endpoint port */
	php_http_option_register(registry, ZEND_STRL("port"), CURLOPT_PORT, IS_LONG);

	/* RFC4007 zone_id */
#if PHP_HTTP_CURL_VERSION(7,19,0)
	php_http_option_register(registry, ZEND_STRL("address_scope"), CURLOPT_ADDRESS_SCOPE, IS_LONG);
#endif

	/* auth */
	if ((opt = php_http_option_register(registry, ZEND_STRL("httpauth"), CURLOPT_USERPWD, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("httpauthtype"), CURLOPT_HTTPAUTH, IS_LONG))) {
		Z_LVAL(opt->defval) = CURLAUTH_ANYSAFE;
	}
#if PHP_HTTP_CURL_VERSION(7,43,0)
	if (PHP_HTTP_CURL_FEATURE(CURL_VERSION_SSPI) || PHP_HTTP_CURL_FEATURE(CURL_VERSION_GSSAPI))
	if ((opt = php_http_option_register(registry, ZEND_STRL("service_name"), CURLOPT_SERVICE_NAME, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
#endif

	/* redirects */
	if ((opt = php_http_option_register(registry, ZEND_STRL("redirect"), CURLOPT_FOLLOWLOCATION, IS_LONG))) {
		opt->setter = php_http_curle_option_set_redirect;
	}
	php_http_option_register(registry, ZEND_STRL("unrestricted_auth"), CURLOPT_UNRESTRICTED_AUTH, _IS_BOOL);
#if PHP_HTTP_CURL_VERSION(7,19,1)
	php_http_option_register(registry, ZEND_STRL("postredir"), CURLOPT_POSTREDIR, IS_LONG);
#endif

	/* retries */
	if ((opt = php_http_option_register(registry, ZEND_STRL("retrycount"), 0, IS_LONG))) {
		opt->setter = php_http_curle_option_set_retrycount;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("retrydelay"), 0, IS_DOUBLE))) {
		opt->setter = php_http_curle_option_set_retrydelay;
	}

	/* referer */
	if ((opt = php_http_option_register(registry, ZEND_STRL("referer"), CURLOPT_REFERER, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("autoreferer"), CURLOPT_AUTOREFERER, _IS_BOOL))) {
		ZVAL_BOOL(&opt->defval, 1);
	}

	/* useragent */
	if ((opt = php_http_option_register(registry, ZEND_STRL("useragent"), CURLOPT_USERAGENT, IS_STRING))) {
		/* don't check strlen, to allow sending no useragent at all */
		ZVAL_PSTRING(&opt->defval,
				"PECL_HTTP/" PHP_PECL_HTTP_VERSION " "
				"PHP/" PHP_VERSION " "
				"libcurl/" LIBCURL_VERSION);
	}

	/* resume */
	if ((opt = php_http_option_register(registry, ZEND_STRL("resume"), CURLOPT_RESUME_FROM, IS_LONG))) {
		opt->setter = php_http_curle_option_set_resume;
	}
	/* ranges */
	if ((opt = php_http_option_register(registry, ZEND_STRL("range"), CURLOPT_RANGE, IS_ARRAY))) {
		opt->setter = php_http_curle_option_set_range;
	}

	/* etag */
	if ((opt = php_http_option_register(registry, ZEND_STRL("etag"), 0, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
		opt->setter = php_http_curle_option_set_etag;
	}

	/* compression */
	if ((opt = php_http_option_register(registry, ZEND_STRL("compress"), 0, _IS_BOOL))) {
		opt->setter = php_http_curle_option_set_compress;
	}

	/* lastmodified */
	if ((opt = php_http_option_register(registry, ZEND_STRL("lastmodified"), 0, IS_LONG))) {
		opt->setter = php_http_curle_option_set_lastmodified;
	}

	/* cookies */
	if ((opt = php_http_option_register(registry, ZEND_STRL("encodecookies"), 0, _IS_BOOL))) {
		opt->setter = php_http_curle_option_set_encodecookies;
		ZVAL_BOOL(&opt->defval, 1);
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("cookies"), 0, IS_ARRAY))) {
		opt->setter = php_http_curle_option_set_cookies;
	}

	/* cookiesession, don't load session cookies from cookiestore */
	if ((opt = php_http_option_register(registry, ZEND_STRL("cookiesession"), CURLOPT_COOKIESESSION, _IS_BOOL))) {
		opt->setter = php_http_curle_option_set_cookiesession;
	}
	/* cookiestore, read initial cookies from that file and store cookies back into that file */
	if ((opt = php_http_option_register(registry, ZEND_STRL("cookiestore"), 0, IS_STRING))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
		opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
		opt->setter = php_http_curle_option_set_cookiestore;
	}

	/* maxfilesize */
	php_http_option_register(registry, ZEND_STRL("maxfilesize"), CURLOPT_MAXFILESIZE, IS_LONG);

	/* http protocol version */
	php_http_option_register(registry, ZEND_STRL("protocol"), CURLOPT_HTTP_VERSION, IS_LONG);

	/* timeouts */
	if ((opt = php_http_option_register(registry, ZEND_STRL("timeout"), CURLOPT_TIMEOUT_MS, IS_DOUBLE))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_TRANSFORM_MS;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("connecttimeout"), CURLOPT_CONNECTTIMEOUT_MS, IS_DOUBLE))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_TRANSFORM_MS;
		Z_DVAL(opt->defval) = 3;
	}
#if PHP_HTTP_CURL_VERSION(7,36,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("expect_100_timeout"), CURLOPT_EXPECT_100_TIMEOUT_MS, IS_DOUBLE))) {
		opt->flags |= PHP_HTTP_CURLE_OPTION_TRANSFORM_MS;
		Z_DVAL(opt->defval) = 1;
	}
#endif

	/* tcp */
	php_http_option_register(registry, ZEND_STRL("tcp_nodelay"), CURLOPT_TCP_NODELAY, _IS_BOOL);
#if PHP_HTTP_CURL_VERSION(7,25,0)
	php_http_option_register(registry, ZEND_STRL("tcp_keepalive"), CURLOPT_TCP_KEEPALIVE, _IS_BOOL);
	if ((opt = php_http_option_register(registry, ZEND_STRL("tcp_keepidle"), CURLOPT_TCP_KEEPIDLE, IS_LONG))) {
		Z_LVAL(opt->defval) = 60;
	}
	if ((opt = php_http_option_register(registry, ZEND_STRL("tcp_keepintvl"), CURLOPT_TCP_KEEPINTVL, IS_LONG))) {
		Z_LVAL(opt->defval) = 60;
	}
#endif

	/* ssl */
	if (PHP_HTTP_CURL_FEATURE(CURL_VERSION_SSL)) {
		if ((opt = php_http_option_register(registry, ZEND_STRL("ssl"), 0, IS_ARRAY))) {
			registry = &opt->suboptions;

			if ((opt = php_http_option_register(registry, ZEND_STRL("cert"), CURLOPT_SSLCERT, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("certtype"), CURLOPT_SSLCERTTYPE, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				ZVAL_PSTRING(&opt->defval, "PEM");
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("key"), CURLOPT_SSLKEY, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("keytype"), CURLOPT_SSLKEYTYPE, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				ZVAL_PSTRING(&opt->defval, "PEM");
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("keypasswd"), CURLOPT_SSLKEYPASSWD, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
			}
			php_http_option_register(registry, ZEND_STRL("engine"), CURLOPT_SSLENGINE, IS_STRING);
			php_http_option_register(registry, ZEND_STRL("version"), CURLOPT_SSLVERSION, IS_LONG);
			if ((opt = php_http_option_register(registry, ZEND_STRL("verifypeer"), CURLOPT_SSL_VERIFYPEER, _IS_BOOL))) {
				ZVAL_BOOL(&opt->defval, 1);
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("verifyhost"), CURLOPT_SSL_VERIFYHOST, _IS_BOOL))) {
				ZVAL_BOOL(&opt->defval, 1);
				opt->setter = php_http_curle_option_set_ssl_verifyhost;
			}
#if PHP_HTTP_CURL_VERSION(7,41,0) && (PHP_HTTP_HAVE_LIBCURL_OPENSSL || PHP_HTTP_HAVE_LIBCURL_NSS || PHP_HTTP_HAVE_LIBCURL_GNUTLS)
		php_http_option_register(registry, ZEND_STRL("verifystatus"), CURLOPT_SSL_VERIFYSTATUS, _IS_BOOL);
#endif
			php_http_option_register(registry, ZEND_STRL("cipher_list"), CURLOPT_SSL_CIPHER_LIST, IS_STRING);
#if PHP_HTTP_HAVE_LIBCURL_CAINFO
			if ((opt = php_http_option_register(registry, ZEND_STRL("cainfo"), CURLOPT_CAINFO, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
#ifdef PHP_HTTP_CAINFO
			ZVAL_PSTRING(&opt->defval, PHP_HTTP_CAINFO);
#endif
			}
#endif
#if PHP_HTTP_HAVE_LIBCURL_CAPATH
			if ((opt = php_http_option_register(registry, ZEND_STRL("capath"), CURLOPT_CAPATH, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
#ifdef PHP_HTTP_CAPATH
			ZVAL_PSTRING(&opt->defval, PHP_HTTP_CAPATH);
#endif
			}
#endif
			if ((opt = php_http_option_register(registry, ZEND_STRL("random_file"), CURLOPT_RANDOM_FILE, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("egdsocket"), CURLOPT_EGDSOCKET, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
			}
#if PHP_HTTP_CURL_VERSION(7,19,0)
			if ((opt = php_http_option_register(registry, ZEND_STRL("issuercert"), CURLOPT_ISSUERCERT, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
			}
#	if PHP_HTTP_HAVE_LIBCURL_OPENSSL
			if ((opt = php_http_option_register(registry, ZEND_STRL("crlfile"), CURLOPT_CRLFILE, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
			}
#	endif
#endif
#if (PHP_HTTP_CURL_VERSION(7,19,1) && PHP_HTTP_HAVE_LIBCURL_OPENSSL) || (PHP_HTTP_CURL_VERSION(7,34,0) && PHP_HTTP_HAVE_LIBCURL_NSS) || (PHP_HTTP_CURL_VERSION(7,42,0) && defined(PHP_HTTP_HAVE_LIBCURL_GNUTLS)) || (PHP_HTTP_CURL_VERSION(7,39,0) && defined(PHP_HTTP_HAVE_LIBCURL_GSKIT))
			if ((opt = php_http_option_register(registry, ZEND_STRL("certinfo"), CURLOPT_CERTINFO, _IS_BOOL))) {
				ZVAL_FALSE(&opt->defval);
			}
#endif
#if PHP_HTTP_CURL_VERSION(7,36,0)
			if ((opt = php_http_option_register(registry, ZEND_STRL("enable_npn"), CURLOPT_SSL_ENABLE_NPN, _IS_BOOL))) {
				ZVAL_BOOL(&opt->defval, 1);
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("enable_alpn"), CURLOPT_SSL_ENABLE_ALPN, _IS_BOOL))) {
				ZVAL_BOOL(&opt->defval, 1);
			}
#endif
#if PHP_HTTP_CURL_VERSION(7,39,0)
			/* FIXME: see http://curl.haxx.se/libcurl/c/CURLOPT_PINNEDPUBLICKEY.html#AVAILABILITY */
			if ((opt = php_http_option_register(registry, ZEND_STRL("pinned_publickey"), CURLOPT_PINNEDPUBLICKEY, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR;
			}
#endif
#if PHP_HTTP_CURL_VERSION(7,21,4) && PHP_HTTP_HAVE_LIBCURL_TLSAUTH_TYPE
			if ((opt = php_http_option_register(registry, ZEND_STRL("tlsauthtype"), CURLOPT_TLSAUTH_TYPE, IS_LONG))) {
				opt->setter = php_http_curle_option_set_ssl_tlsauthtype;
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("tlsauthuser"), CURLOPT_TLSAUTH_USERNAME, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
			}
			if ((opt = php_http_option_register(registry, ZEND_STRL("tlsauthpass"), CURLOPT_TLSAUTH_PASSWORD, IS_STRING))) {
				opt->flags |= PHP_HTTP_CURLE_OPTION_CHECK_STRLEN;
			}
#endif
#if PHP_HTTP_CURL_VERSION(7,42,0) && (PHP_HTTP_HAVE_LIBCURL_NSS || PHP_HTTP_HAVE_LIBCURL_SECURETRANSPORT)
		php_http_option_register(registry, ZEND_STRL("falsestart"), CURLOPT_SSL_FALSESTART, _IS_BOOL);
#endif
		}
	}
}

static zval *php_http_curle_get_option(php_http_option_t *opt, HashTable *options, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	zval *option;

	if ((option = php_http_option_get(opt, options, NULL))) {
		zval zopt;

		ZVAL_DUP(&zopt, option);
		convert_to_explicit_type(&zopt, opt->type);
		zend_hash_update(&curl->options.cache, opt->name, &zopt);
		return zend_hash_find(&curl->options.cache, opt->name);
	}
	return option;
}

static ZEND_RESULT_CODE php_http_curle_set_option(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_curl_handler_t *curl = userdata;
	CURL *ch = curl->handle;
	zval tmp;
	CURLcode rc = CURLE_UNKNOWN_OPTION;
	ZEND_RESULT_CODE rv = SUCCESS;

	if (!val) {
		val = &opt->defval;
	}

	switch (opt->type) {
	case _IS_BOOL:
		if (opt->setter) {
			rv = opt->setter(opt, val, curl);
		} else if (CURLE_OK != curl_easy_setopt(ch, opt->option, (long) (Z_TYPE_P(val) == IS_TRUE))) {
			rv = FAILURE;
		}
		break;

	case IS_LONG:
		if (opt->setter) {
			rv = opt->setter(opt, val, curl);
		} else if (CURLE_OK != curl_easy_setopt(ch, opt->option, Z_LVAL_P(val))) {
			rv = FAILURE;
		}
		break;

	case IS_STRING:
		if (opt->setter) {
			rv = opt->setter(opt, val, curl);
		} else if (!val || Z_TYPE_P(val) == IS_NULL) {
			if (CURLE_OK != (rc = curl_easy_setopt(ch, opt->option, NULL))) {
				rv = FAILURE;
			}
		} else if ((opt->flags & PHP_HTTP_CURLE_OPTION_CHECK_STRLEN) && !Z_STRLEN_P(val)) {
			if (CURLE_OK != (rc = curl_easy_setopt(ch, opt->option, NULL))) {
				rv = FAILURE;
			}
		} else if ((opt->flags & PHP_HTTP_CURLE_OPTION_CHECK_BASEDIR) && Z_STRVAL_P(val) && SUCCESS != php_check_open_basedir(Z_STRVAL_P(val))) {
			if (CURLE_OK != (rc = curl_easy_setopt(ch, opt->option, NULL))) {
				rv = FAILURE;
			}
		} else if (CURLE_OK != (rc = curl_easy_setopt(ch, opt->option, Z_STRVAL_P(val)))) {
			rv = FAILURE;
		}
		break;

	case IS_DOUBLE:
		if (opt->flags & PHP_HTTP_CURLE_OPTION_TRANSFORM_MS) {
			tmp = *val;
			Z_DVAL(tmp) *= 1000;
			val = &tmp;
		}
		if (opt->setter) {
			rv = opt->setter(opt, val, curl);
		} else if (CURLE_OK != curl_easy_setopt(ch, opt->option, (long) Z_DVAL_P(val))) {
			rv = FAILURE;
		}
		break;

	case IS_ARRAY:
		if (opt->setter) {
			rv = opt->setter(opt, val, curl);
		} else if (Z_TYPE_P(val) != IS_NULL) {
			rv = php_http_options_apply(&opt->suboptions, Z_ARRVAL_P(val), curl);
		}
		break;

	default:
		if (opt->setter) {
			rv = opt->setter(opt, val, curl);
		} else {
			rv = FAILURE;
		}
		break;
	}
	if (rv != SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "Could not set option %s (%s)", opt->name->val, curl_easy_strerror(rc));
	}
	return rv;
}

#if PHP_HTTP_CURL_VERSION(7,30,0)
static ZEND_RESULT_CODE php_http_curlm_option_set_pipelining_bl(php_http_option_t *opt, zval *value, void *userdata)
{
	php_http_client_t *client = userdata;
	php_http_client_curl_t *curl = client->ctx;
	CURLM *ch = curl->handle->multi;
	HashTable tmp_ht;
	char **bl = NULL;

	/* array of char *, ending with a NULL */
	if (value && Z_TYPE_P(value) != IS_NULL) {
		zval *entry;
		HashTable *ht = HASH_OF(value);
		int c = zend_hash_num_elements(ht);
		char **ptr = ecalloc(c + 1, sizeof(char *));

		bl = ptr;

		zend_hash_init(&tmp_ht, c, NULL, ZVAL_PTR_DTOR, 0);
		array_join(ht, &tmp_ht, 0, ARRAY_JOIN_STRINGIFY);

		ZEND_HASH_FOREACH_VAL(&tmp_ht, entry)
		{
			*ptr++ = Z_STRVAL_P(entry);
		}
		ZEND_HASH_FOREACH_END();
	}

	if (CURLM_OK != curl_multi_setopt(ch, opt->option, bl)) {
		if (bl) {
			efree(bl);
			zend_hash_destroy(&tmp_ht);
		}
		return FAILURE;
	}

	if (bl) {
		efree(bl);
		zend_hash_destroy(&tmp_ht);
	}
	return SUCCESS;
}
#endif

static inline ZEND_RESULT_CODE php_http_curlm_use_eventloop(php_http_client_t *h, php_http_client_curl_ops_t *ev_ops, zval *init_data)
{
	php_http_client_curl_t *curl = h->ctx;
	void *ev_ctx;

	if (ev_ops) {
		if (!(ev_ctx = ev_ops->init(h, init_data))) {
			return FAILURE;
		}
		curl->ev_ctx = ev_ctx;
		curl->ev_ops = ev_ops;
	} else {
		if (curl->ev_ops) {
			if (curl->ev_ctx) {
				curl->ev_ops->dtor(&curl->ev_ctx);
			}
			curl->ev_ops = NULL;
		}
	}

	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_curlm_option_set_use_eventloop(php_http_option_t *opt, zval *value, void *userdata)
{
	php_http_client_t *client = userdata;
	php_http_client_curl_ops_t *ev_ops = NULL;

	if (value && Z_TYPE_P(value) == IS_OBJECT && instanceof_function(Z_OBJCE_P(value), php_http_client_curl_user_get_class_entry())) {
		ev_ops = php_http_client_curl_user_ops_get();
#if PHP_HTTP_HAVE_LIBEVENT
	} else if (value && zend_is_true(value)) {
		ev_ops = php_http_client_curl_event_ops_get();
#endif
	}

	return php_http_curlm_use_eventloop(client, ev_ops, value);
}

static ZEND_RESULT_CODE php_http_curlm_option_set_share_cookies(php_http_option_t *opt, zval *value, void *userdata)
{
	php_http_client_t *client = userdata;
	php_http_client_curl_t *curl = client->ctx;
	CURLSHcode rc;

	if (Z_TYPE_P(value) == IS_TRUE) {
		rc = curl_share_setopt(curl->handle->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
	} else {
		rc = curl_share_setopt(curl->handle->share, CURLSHOPT_UNSHARE, CURL_LOCK_DATA_COOKIE);
	}

	if (CURLSHE_OK != rc) {
		php_error_docref(NULL, E_NOTICE, "Could not set option %s (%s)", opt->name->val, curl_share_strerror(rc));
		return FAILURE;
	}
	return SUCCESS;
}

#if PHP_HTTP_CURL_VERSION(7,23,0)
static ZEND_RESULT_CODE php_http_curlm_option_set_share_ssl(php_http_option_t *opt, zval *value, void *userdata)
{
	php_http_client_t *client = userdata;
	php_http_client_curl_t *curl = client->ctx;
	CURLSHcode rc;

	if (Z_TYPE_P(value) == IS_TRUE) {
		rc = curl_share_setopt(curl->handle->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	} else {
		rc = curl_share_setopt(curl->handle->share, CURLSHOPT_UNSHARE, CURL_LOCK_DATA_SSL_SESSION);
	}

	if (CURLSHE_OK != rc) {
		php_error_docref(NULL, E_NOTICE, "Could not set option %s (%s)", opt->name->val, curl_share_strerror(rc));
		return FAILURE;
	}
	return SUCCESS;
}
#endif

static void php_http_curlm_options_init(php_http_options_t *registry)
{
	php_http_option_t *opt;

	/* set size of connection cache */
	if ((opt = php_http_option_register(registry, ZEND_STRL("maxconnects"), CURLMOPT_MAXCONNECTS, IS_LONG))) {
		/* -1 == default, 0 == unlimited */
		ZVAL_LONG(&opt->defval, -1);
	}
	/* set max number of connections to a single host */
#if PHP_HTTP_CURL_VERSION(7,30,0)
	php_http_option_register(registry, ZEND_STRL("max_host_connections"), CURLMOPT_MAX_HOST_CONNECTIONS, IS_LONG);
#endif
	/* maximum number of requests in a pipeline */
#if PHP_HTTP_CURL_VERSION(7,30,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("max_pipeline_length"), CURLMOPT_MAX_PIPELINE_LENGTH, IS_LONG))) {
		ZVAL_LONG(&opt->defval, 5);
	}
#endif
	/* max simultaneously open connections */
#if PHP_HTTP_CURL_VERSION(7,30,0)
	php_http_option_register(registry, ZEND_STRL("max_total_connections"), CURLMOPT_MAX_TOTAL_CONNECTIONS, IS_LONG);
#endif
	/* enable/disable HTTP pipelining */
	php_http_option_register(registry, ZEND_STRL("pipelining"), CURLMOPT_PIPELINING, _IS_BOOL);
	/* chunk length threshold for pipelining */
#if PHP_HTTP_CURL_VERSION(7,30,0)
	php_http_option_register(registry, ZEND_STRL("chunk_length_penalty_size"), CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE, IS_LONG);
#endif
	/* size threshold for pipelining penalty */
#if PHP_HTTP_CURL_VERSION(7,30,0)
	php_http_option_register(registry, ZEND_STRL("content_length_penalty_size"), CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE, IS_LONG);
#endif
	/* pipelining server blacklist */
#if PHP_HTTP_CURL_VERSION(7,30,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("pipelining_server_bl"), CURLMOPT_PIPELINING_SERVER_BL, IS_ARRAY))) {
		opt->setter = php_http_curlm_option_set_pipelining_bl;
	}
#endif
	/* pipelining host blacklist */
#if PHP_HTTP_CURL_VERSION(7,30,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("pipelining_site_bl"), CURLMOPT_PIPELINING_SITE_BL, IS_ARRAY))) {
		opt->setter = php_http_curlm_option_set_pipelining_bl;
	}
#endif
	/* events */
	if ((opt = php_http_option_register(registry, ZEND_STRL("use_eventloop"), 0, 0))) {
		opt->setter = php_http_curlm_option_set_use_eventloop;
	}
	/* share */
	if ((opt = php_http_option_register(registry, ZEND_STRL("share_cookies"), 0, _IS_BOOL))) {
		opt->setter = php_http_curlm_option_set_share_cookies;
		ZVAL_TRUE(&opt->defval);
	}
#if PHP_HTTP_CURL_VERSION(7,23,0)
	if ((opt = php_http_option_register(registry, ZEND_STRL("share_ssl"), 0, _IS_BOOL))) {
		opt->setter = php_http_curlm_option_set_share_ssl;
		ZVAL_TRUE(&opt->defval);
	}
#endif
}

static ZEND_RESULT_CODE php_http_curlm_set_option(php_http_option_t *opt, zval *val, void *userdata)
{
	php_http_client_t *client = userdata;
	php_http_client_curl_t *curl = client->ctx;
	CURLM *ch = curl->handle->multi;
	zval zopt, *orig = val;
	CURLMcode rc = CURLM_UNKNOWN_OPTION;
	ZEND_RESULT_CODE rv = SUCCESS;

	if (!val) {
		val = &opt->defval;
	} else if (opt->type && Z_TYPE_P(val) != opt->type && !(Z_TYPE_P(val) == IS_NULL && opt->type == IS_ARRAY)) {
		ZVAL_DUP(&zopt, val);
		convert_to_explicit_type(&zopt, opt->type);

		val = &zopt;
	}

	if (opt->setter) {
		rv = opt->setter(opt, val, client);
	} else {
		switch (opt->type) {
		case _IS_BOOL:
			if (CURLM_OK != (rc = curl_multi_setopt(ch, opt->option, (long) zend_is_true(val)))) {
				rv = FAILURE;
				php_error_docref(NULL, E_NOTICE, "Could not set option %s (%s)", opt->name->val, curl_multi_strerror(rc));
			}
			break;
		case IS_LONG:
			if (CURLM_OK != (rc = curl_multi_setopt(ch, opt->option, Z_LVAL_P(val)))) {
				rv = FAILURE;
				php_error_docref(NULL, E_NOTICE, "Could not set option %s (%s)", opt->name->val, curl_multi_strerror(rc));
			}
			break;
		default:
			rv = FAILURE;
			php_error_docref(NULL, E_NOTICE, "Could not set option %s", opt->name->val);
			break;
		}
	}

	if (val && val != orig && val != &opt->defval) {
		zval_ptr_dtor(val);
	}

	return rv;
}

/* client ops */

static ZEND_RESULT_CODE php_http_client_curl_handler_reset(php_http_client_curl_handler_t *curl)
{
	CURL *ch = curl->handle;
	php_http_curle_storage_t *st;

	if ((st = php_http_curle_get_storage(ch))) {
		if (st->url) {
			pefree(st->url, 1);
			st->url = NULL;
		}
		if (st->cookiestore) {
			pefree(st->cookiestore, 1);
			st->cookiestore = NULL;
		}
		st->errorbuffer[0] = '\0';
		st->errorcode = 0;
	}

	curl_easy_setopt(ch, CURLOPT_URL, NULL);
	curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(ch, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(ch, CURLOPT_NOBODY, 0L);
	/* libcurl < 7.19.6 does not clear auth info with USERPWD set to NULL */
#if PHP_HTTP_CURL_VERSION(7,19,1)
	curl_easy_setopt(ch, CURLOPT_PROXYUSERNAME, NULL);
	curl_easy_setopt(ch, CURLOPT_PROXYPASSWORD, NULL);
	curl_easy_setopt(ch, CURLOPT_USERNAME, NULL);
	curl_easy_setopt(ch, CURLOPT_PASSWORD, NULL);
#endif

#if PHP_HTTP_CURL_VERSION(7,21,3)
	if (curl->options.resolve) {
		curl_slist_free_all(curl->options.resolve);
		curl->options.resolve = NULL;
	}
#endif
	curl->options.retry.count = 0;
	curl->options.retry.delay = 0;
	curl->options.redirects = 0;
	curl->options.encode_cookies = 1;

	if (curl->options.headers) {
		curl_slist_free_all(curl->options.headers);
		curl->options.headers = NULL;
	}
	if (curl->options.proxyheaders) {
		curl_slist_free_all(curl->options.proxyheaders);
		curl->options.proxyheaders = NULL;
	}

	php_http_buffer_reset(&curl->options.cookies);
	php_http_buffer_reset(&curl->options.ranges);

	return SUCCESS;
}

static php_http_client_curl_handler_t *php_http_client_curl_handler_init(php_http_client_t *h, php_resource_factory_t *rf)
{
	void *handle;
	php_http_client_curl_t *curl = h->ctx;
	php_http_client_curl_handler_t *handler;

	if (!(handle = php_resource_factory_handle_ctor(rf, NULL))) {
		php_error_docref(NULL, E_WARNING, "Failed to initialize curl handle");
		return NULL;
	}

	handler = ecalloc(1, sizeof(*handler));
	handler->rf = rf;
	handler->client = h;
	handler->handle = handle;
	handler->response.body = php_http_message_body_init(NULL, NULL);
	php_http_buffer_init(&handler->response.headers);
	php_http_buffer_init(&handler->options.cookies);
	php_http_buffer_init(&handler->options.ranges);
	zend_hash_init(&handler->options.cache, 0, NULL, ZVAL_PTR_DTOR, 0);

#if ZTS
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
#endif
	curl_easy_setopt(handle, CURLOPT_HEADER, 0L);
	curl_easy_setopt(handle, CURLOPT_FILETIME, 1L);
	curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1L);
	curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, php_http_curle_header_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, php_http_curle_body_callback);
	curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, php_http_curle_raw_callback);
	curl_easy_setopt(handle, CURLOPT_READFUNCTION, php_http_curle_read_callback);
	curl_easy_setopt(handle, CURLOPT_SEEKFUNCTION, php_http_curle_seek_callback);
#if PHP_HTTP_CURL_VERSION(7,32,0)
	curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, php_http_curle_xferinfo_callback);
	curl_easy_setopt(handle, CURLOPT_XFERINFODATA, handler);
#else
	curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, php_http_curle_progress_callback);
	curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, handler);
#endif
	curl_easy_setopt(handle, CURLOPT_DEBUGDATA, handler);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, handler);
	curl_easy_setopt(handle, CURLOPT_HEADERDATA, handler);
	curl_easy_setopt(handle, CURLOPT_SHARE, curl->handle->share);

	php_http_client_curl_handler_reset(handler);

	return handler;
}


static ZEND_RESULT_CODE php_http_client_curl_handler_prepare(php_http_client_curl_handler_t *curl, php_http_client_enqueue_t *enqueue)
{
	size_t body_size;
	php_http_message_t *msg = enqueue->request;
	php_http_curle_storage_t *storage = php_http_curle_get_storage(curl->handle);

	/* request url */
	if (!PHP_HTTP_INFO(msg).request.url) {
		php_error_docref(NULL, E_WARNING, "Cannot request empty URL");
		return FAILURE;
	}
	storage->errorbuffer[0] = '\0';
	if (storage->url) {
		pefree(storage->url, 1);
	}
	php_http_url_to_string(PHP_HTTP_INFO(msg).request.url, &storage->url, NULL, 1);
	curl_easy_setopt(curl->handle, CURLOPT_URL, storage->url);

	/* apply options */
	php_http_options_apply(&php_http_curle_options, enqueue->options, curl);

	/* request headers */
	php_http_message_update_headers(msg);
	if (zend_hash_num_elements(&msg->hdrs)) {
		php_http_arrkey_t header_key;
		zval *header_val;
		zend_string *header_str;
		php_http_buffer_t header;
#if !PHP_HTTP_CURL_VERSION(7,23,0)
		zval *ct = zend_hash_str_find(&msg->hdrs, ZEND_STRL("Content-Length"));
#endif

		php_http_buffer_init(&header);
		ZEND_HASH_FOREACH_KEY_VAL(&msg->hdrs, header_key.h, header_key.key, header_val)
		{
			if (header_key.key) {
#if !PHP_HTTP_CURL_VERSION(7,23,0)
				/* avoid duplicate content-length header */
				if (ct && ct == header_val) {
					continue;
				}
#endif
				header_str = zval_get_string(header_val);
				php_http_buffer_appendf(&header, "%s: %s", header_key.key->val, header_str->val);
				php_http_buffer_fix(&header);
				curl->options.headers = curl_slist_append(curl->options.headers, header.data);
				php_http_buffer_reset(&header);
				zend_string_release(header_str);
			}
		}
		ZEND_HASH_FOREACH_END();
		php_http_buffer_dtor(&header);
	}
	curl_easy_setopt(curl->handle, CURLOPT_HTTPHEADER, curl->options.headers);

	/* attach request body */
	if ((body_size = php_http_message_body_size(msg->body))) {
		/* RFC2616, section 4.3 (para. 4) states that »a message-body MUST NOT be included in a request if the
		 * specification of the request method (section 5.1.1) does not allow sending an entity-body in request.«
		 * Following the clause in section 5.1.1 (para. 2) that request methods »MUST be implemented with the
		 * same semantics as those specified in section 9« reveal that not any single defined HTTP/1.1 method
		 * does not allow a request body.
		 */
		php_stream_rewind(php_http_message_body_stream(msg->body));
		curl_easy_setopt(curl->handle, CURLOPT_SEEKDATA, msg->body);
		curl_easy_setopt(curl->handle, CURLOPT_READDATA, msg->body);
		curl_easy_setopt(curl->handle, CURLOPT_INFILESIZE, body_size);
		curl_easy_setopt(curl->handle, CURLOPT_POSTFIELDSIZE, body_size);
		curl_easy_setopt(curl->handle, CURLOPT_POST, 1L);
	} else {
		curl_easy_setopt(curl->handle, CURLOPT_SEEKDATA, NULL);
		curl_easy_setopt(curl->handle, CURLOPT_READDATA, NULL);
		curl_easy_setopt(curl->handle, CURLOPT_INFILESIZE, 0L);
		curl_easy_setopt(curl->handle, CURLOPT_POSTFIELDSIZE, 0L);
	}

	/*
	 * Always use CUSTOMREQUEST, else curl won't send any request body for GET etc.
	 * See e.g. bug #69313.
	 *
	 * Here's what curl does:
	 * - CURLOPT_HTTPGET: ignore request body
	 * - CURLOPT_UPLOAD: set "Expect: 100-continue" header
	 * - CURLOPT_POST: set "Content-Type: application/x-www-form-urlencoded" header
	 * Now select the least bad.
	 *
	 * See also https://tools.ietf.org/html/rfc7231#section-5.1.1
	 */
	if (PHP_HTTP_INFO(msg).request.method) {
		switch(php_http_select_str(PHP_HTTP_INFO(msg).request.method, 2, "HEAD", "PUT")) {
		case 0:
			curl_easy_setopt(curl->handle, CURLOPT_NOBODY, 1L);
			break;
		case 1:
			curl_easy_setopt(curl->handle, CURLOPT_UPLOAD, 1L);
			break;
		default:
			curl_easy_setopt(curl->handle, CURLOPT_CUSTOMREQUEST, PHP_HTTP_INFO(msg).request.method);
		}
	} else {
		php_error_docref(NULL, E_WARNING, "Cannot use empty request method");
		return FAILURE;
	}

	return SUCCESS;
}

static void php_http_client_curl_handler_clear(php_http_client_curl_handler_t *handler)
{
	curl_easy_setopt(handler->handle, CURLOPT_NOPROGRESS, 1L);
#if PHP_HTTP_CURL_VERSION(7,32,0)
	curl_easy_setopt(handler->handle, CURLOPT_XFERINFOFUNCTION, NULL);
#else
	curl_easy_setopt(handler->handle, CURLOPT_PROGRESSFUNCTION, NULL);
#endif
	curl_easy_setopt(handler->handle, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(handler->handle, CURLOPT_DEBUGFUNCTION, NULL);
	curl_easy_setopt(handler->handle, CURLOPT_COOKIELIST, "FLUSH");
	curl_easy_setopt(handler->handle, CURLOPT_SHARE, NULL);
}

static void php_http_client_curl_handler_dtor(php_http_client_curl_handler_t *handler)
{
	php_http_client_curl_handler_clear(handler);

	php_resource_factory_handle_dtor(handler->rf, handler->handle);
	php_resource_factory_free(&handler->rf);

	php_http_message_body_free(&handler->response.body);
	php_http_buffer_dtor(&handler->response.headers);
	php_http_buffer_dtor(&handler->options.ranges);
	php_http_buffer_dtor(&handler->options.cookies);
	zend_hash_destroy(&handler->options.cache);

#if PHP_HTTP_CURL_VERSION(7,21,3)
	if (handler->options.resolve) {
		curl_slist_free_all(handler->options.resolve);
		handler->options.resolve = NULL;
	}
#endif

	if (handler->options.headers) {
		curl_slist_free_all(handler->options.headers);
		handler->options.headers = NULL;
	}

	if (handler->options.proxyheaders) {
		curl_slist_free_all(handler->options.proxyheaders);
		handler->options.proxyheaders = NULL;
	}

	efree(handler);
}

static php_http_client_t *php_http_client_curl_init(php_http_client_t *h, void *handle)
{
	php_http_client_curl_t *curl;

	if (!handle && !(handle = php_resource_factory_handle_ctor(h->rf, NULL))) {
		php_error_docref(NULL, E_WARNING, "Failed to initialize curl handle");
		return NULL;
	}

	curl = ecalloc(1, sizeof(*curl));
	curl->handle = handle;
	curl->unfinished = 0;
	h->ctx = curl;

	return h;
}

static void php_http_client_curl_dtor(php_http_client_t *h)
{
	php_http_client_curl_t *curl = h->ctx;

	if (curl->ev_ops) {
		curl->ev_ops->dtor(&curl->ev_ctx);
		curl->ev_ops = NULL;
	}
	curl->unfinished = 0;

	php_resource_factory_handle_dtor(h->rf, curl->handle);

	efree(curl);
	h->ctx = NULL;
}

static void queue_dtor(php_http_client_enqueue_t *e)
{
	php_http_client_curl_handler_t *handler = e->opaque;

	if (handler->queue.dtor) {
		e->opaque = handler->queue.opaque;
		handler->queue.dtor(e);
	}
	php_http_client_curl_handler_dtor(handler);
}

static php_resource_factory_t *create_rf(php_http_client_t *h, php_http_client_enqueue_t *enqueue)
{
	php_persistent_handle_factory_t *pf = NULL;
	php_resource_factory_t *rf = NULL;
	php_http_url_t *url = enqueue->request->http.info.request.url;

	if (!url || (!url->host && !url->path)) {
		php_error_docref(NULL, E_WARNING, "Cannot request empty URL");
		return NULL;
	}

	/* only if the client itself is setup for persistence */
	if (php_resource_factory_is_persistent(h->rf)) {
		zend_string *id;
		char *id_str = NULL;
		size_t id_len;
		int port = url->port ? url->port : 80;
		zval *zport;
		php_persistent_handle_factory_t *phf = h->rf->data;

		if ((zport = zend_hash_str_find(enqueue->options, ZEND_STRL("port")))) {
			zend_long lport = zval_get_long(zport);

			if (lport > 0) {
				port = lport;
			}
		}

		id_len = spprintf(&id_str, 0, "%.*s:%s:%d", (int) phf->ident->len, phf->ident->val, STR_PTR(url->host), port);
		id = php_http_cs2zs(id_str, id_len);
		pf = php_persistent_handle_concede(NULL, PHP_HTTP_G->client.curl.driver.request_name, id, NULL, NULL);
		zend_string_release(id);
	}

	if (pf) {
		rf = php_persistent_handle_resource_factory_init(NULL, pf);
	} else {
		rf = php_resource_factory_init(NULL, &php_http_curle_resource_factory_ops, NULL, NULL);
	}

	return rf;
}

static ZEND_RESULT_CODE php_http_client_curl_enqueue(php_http_client_t *h, php_http_client_enqueue_t *enqueue)
{
	CURLMcode rs;
	php_http_client_curl_t *curl = h->ctx;
	php_http_client_curl_handler_t *handler;
	php_http_client_progress_state_t *progress;
	php_resource_factory_t *rf;

	rf = create_rf(h, enqueue);
	if (!rf) {
		return FAILURE;
	}

	handler = php_http_client_curl_handler_init(h, rf);
	if (!handler) {
		return FAILURE;
	}

	if (SUCCESS != php_http_client_curl_handler_prepare(handler, enqueue)) {
		php_http_client_curl_handler_dtor(handler);
		return FAILURE;
	}

	handler->queue = *enqueue;
	enqueue->opaque = handler;
	enqueue->dtor = queue_dtor;

	if (CURLM_OK != (rs = curl_multi_add_handle(curl->handle->multi, handler->handle))) {
		php_http_client_curl_handler_dtor(handler);
		php_error_docref(NULL, E_WARNING, "Could not enqueue request: %s", curl_multi_strerror(rs));
		return FAILURE;
	}

	zend_llist_add_element(&h->requests, enqueue);
	++curl->unfinished;

	if (h->callback.progress.func && SUCCESS == php_http_client_getopt(h, PHP_HTTP_CLIENT_OPT_PROGRESS_INFO, enqueue->request, &progress)) {
		progress->info = "start";
		h->callback.progress.func(h->callback.progress.arg, h, &handler->queue, progress);
		progress->started = 1;
	}

	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_client_curl_dequeue(php_http_client_t *h, php_http_client_enqueue_t *enqueue)
{
	CURLMcode rs;
	php_http_client_curl_t *curl = h->ctx;
	php_http_client_curl_handler_t *handler = enqueue->opaque;

	if (h->callback.depth && !CG(unclean_shutdown)) {
		php_error_docref(NULL, E_WARNING, "Could not dequeue request while executing callbacks");
		return FAILURE;
	}

	php_http_client_curl_handler_clear(handler);
	if (CURLM_OK == (rs = curl_multi_remove_handle(curl->handle->multi, handler->handle))) {
		zend_llist_del_element(&h->requests, handler->handle, (int (*)(void *, void *)) compare_queue);
		return SUCCESS;
	} else {
		php_error_docref(NULL, E_WARNING, "Could not dequeue request: %s", curl_multi_strerror(rs));
	}

	return FAILURE;
}

static void php_http_client_curl_reset(php_http_client_t *h)
{
	zend_llist_element *next_el, *this_el;

	for (this_el = h->requests.head; this_el; this_el = next_el) {
		next_el = this_el->next;
		php_http_client_curl_dequeue(h, (void *) this_el->data);
	}
}

#if PHP_WIN32
#	define SELECT_ERROR SOCKET_ERROR
#else
#	define SELECT_ERROR -1
#endif

static ZEND_RESULT_CODE php_http_client_curl_wait(php_http_client_t *h, struct timeval *custom_timeout)
{
	int MAX;
	fd_set R, W, E;
	struct timeval timeout;
	php_http_client_curl_t *curl = h->ctx;

	if (curl->ev_ops) {
		return curl->ev_ops->wait(curl->ev_ctx, custom_timeout);
	}

	FD_ZERO(&R);
	FD_ZERO(&W);
	FD_ZERO(&E);

	if (CURLM_OK == curl_multi_fdset(curl->handle->multi, &R, &W, &E, &MAX)) {
		if (custom_timeout && timerisset(custom_timeout)) {
			timeout = *custom_timeout;
		} else {
			php_http_client_curl_get_timeout(curl, 1000, &timeout);
		}

		if (MAX == -1) {
			php_http_sleep((double) timeout.tv_sec + (double) (timeout.tv_usec / PHP_HTTP_MCROSEC));
			return SUCCESS;
		} else if (SELECT_ERROR != select(MAX + 1, &R, &W, &E, &timeout)) {
			return SUCCESS;
		}
	}
	return FAILURE;
}

static int php_http_client_curl_once(php_http_client_t *h)
{
	php_http_client_curl_t *curl = h->ctx;

	if (!h->callback.depth) {
		if (curl->ev_ops) {
			curl->ev_ops->once(curl->ev_ctx);
		} else {
			while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(curl->handle->multi, &curl->unfinished));
		}

		php_http_client_curl_responsehandler(h);
	}

	return curl->unfinished;
}

static ZEND_RESULT_CODE php_http_client_curl_exec(php_http_client_t *h)
{
	php_http_client_curl_t *curl = h->ctx;

	if (!h->callback.depth) {
		if (curl->ev_ops) {
			return curl->ev_ops->exec(curl->ev_ctx);
		}

		while (php_http_client_curl_once(h) && !EG(exception)) {
			if (SUCCESS != php_http_client_curl_wait(h, NULL)) {
#if PHP_WIN32
				/* see http://msdn.microsoft.com/library/en-us/winsock/winsock/windows_sockets_error_codes_2.asp */
				php_error_docref(NULL, E_WARNING, "WinSock error: %d", WSAGetLastError());
#else
				php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
#endif
				return FAILURE;
			}
		}
	}

	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_client_curl_setopt(php_http_client_t *h, php_http_client_setopt_opt_t opt, void *arg)
{
	php_http_client_curl_t *curl = h->ctx;

	switch (opt) {
		case PHP_HTTP_CLIENT_OPT_CONFIGURATION:
			return php_http_options_apply(&php_http_curlm_options, (HashTable *) arg,  h);
			break;

		case PHP_HTTP_CLIENT_OPT_ENABLE_PIPELINING:
			if (CURLM_OK != curl_multi_setopt(curl->handle->multi, CURLMOPT_PIPELINING, (long) *((zend_bool *) arg))) {
				return FAILURE;
			}
			break;

		case PHP_HTTP_CLIENT_OPT_USE_EVENTS:
#if PHP_HTTP_HAVE_LIBEVENT
			return php_http_curlm_use_eventloop(h, (*(zend_bool *) arg)
					? php_http_client_curl_event_ops_get()
					: NULL, NULL);
			break;
#endif

		default:
			return FAILURE;
	}
	return SUCCESS;
}

static int apply_available_options(zval *pDest, int num_args, va_list args, zend_hash_key *hash_key)
{
	php_http_option_t *opt = Z_PTR_P(pDest);
	HashTable *ht;
	zval entry;
	int c;

	ht = va_arg(args, HashTable*);

	if ((c = zend_hash_num_elements(&opt->suboptions.options))) {
		array_init_size(&entry, c);
		zend_hash_apply_with_arguments(&opt->suboptions.options, apply_available_options, 1, Z_ARRVAL(entry));
	} else {
		/* catch deliberate NULL options */
		if (Z_TYPE(opt->defval) == IS_STRING && !Z_STRVAL(opt->defval)) {
			ZVAL_NULL(&entry);
		} else {
			ZVAL_ZVAL(&entry, &opt->defval, 1, 0);
		}
	}

	if (hash_key->key) {
		zend_hash_update(ht, hash_key->key, &entry);
	} else {
		zend_hash_index_update(ht, hash_key->h, &entry);
	}

	return ZEND_HASH_APPLY_KEEP;
}

static ZEND_RESULT_CODE php_http_client_curl_getopt(php_http_client_t *h, php_http_client_getopt_opt_t opt, void *arg, void **res)
{
	php_http_client_enqueue_t *enqueue;

	switch (opt) {
	case PHP_HTTP_CLIENT_OPT_PROGRESS_INFO:
		if ((enqueue = php_http_client_enqueued(h, arg, NULL))) {
			php_http_client_curl_handler_t *handler = enqueue->opaque;

			*((php_http_client_progress_state_t **) res) = &handler->progress;
			return SUCCESS;
		}
		break;

	case PHP_HTTP_CLIENT_OPT_TRANSFER_INFO:
		if ((enqueue = php_http_client_enqueued(h, arg, NULL))) {
			php_http_client_curl_handler_t *handler = enqueue->opaque;

			php_http_curle_get_info(handler->handle, *(HashTable **) res);
			return SUCCESS;
		}
		break;

	case PHP_HTTP_CLIENT_OPT_AVAILABLE_OPTIONS:
		zend_hash_apply_with_arguments(&php_http_curle_options.options, apply_available_options, 1, *(HashTable **) res);
		break;

	case PHP_HTTP_CLIENT_OPT_AVAILABLE_CONFIGURATION:
		zend_hash_apply_with_arguments(&php_http_curlm_options.options, apply_available_options, 1, *(HashTable **) res);
		break;

	default:
		break;
	}

	return FAILURE;
}

static php_http_client_ops_t php_http_client_curl_ops = {
	&php_http_curlm_resource_factory_ops,
	php_http_client_curl_init,
	NULL /* copy */,
	php_http_client_curl_dtor,
	php_http_client_curl_reset,
	php_http_client_curl_exec,
	php_http_client_curl_wait,
	php_http_client_curl_once,
	php_http_client_curl_enqueue,
	php_http_client_curl_dequeue,
	php_http_client_curl_setopt,
	php_http_client_curl_getopt
};

php_http_client_ops_t *php_http_client_curl_get_ops(void)
{
	return &php_http_client_curl_ops;
}

#define REGISTER_NS_STRING_OR_NULL_CONSTANT(ns, name, str, flags)                              \
		do {                                                                           \
			if ((str) != NULL) {                                                   \
				REGISTER_NS_STRING_CONSTANT(ns, name, str, flags);             \
			} else {                                                               \
				REGISTER_NS_NULL_CONSTANT(ns, name, flags);                    \
			}                                                                      \
		} while (0)

PHP_MINIT_FUNCTION(http_client_curl)
{
	curl_version_info_data *info;
	php_http_options_t *options;

	PHP_HTTP_G->client.curl.driver.driver_name = zend_string_init(ZEND_STRL("curl"), 1);
	PHP_HTTP_G->client.curl.driver.client_name = zend_string_init(ZEND_STRL("http\\Client\\Curl"), 1);
	PHP_HTTP_G->client.curl.driver.request_name = zend_string_init(ZEND_STRL("http\\Client\\Curl\\Request"), 1);
	PHP_HTTP_G->client.curl.driver.client_ops = &php_http_client_curl_ops;

	if (SUCCESS != php_http_client_driver_add(&PHP_HTTP_G->client.curl.driver)) {
		return FAILURE;
	}

	if (SUCCESS != php_persistent_handle_provide(PHP_HTTP_G->client.curl.driver.client_name, &php_http_curlm_resource_factory_ops, NULL, NULL)) {
		return FAILURE;
	}
	if (SUCCESS != php_persistent_handle_provide(PHP_HTTP_G->client.curl.driver.request_name, &php_http_curle_resource_factory_ops, NULL, NULL)) {
		return FAILURE;
	}

	if ((options = php_http_options_init(&php_http_curle_options, 1))) {
		options->getter = php_http_curle_get_option;
		options->setter = php_http_curle_set_option;

		php_http_curle_options_init(options);
	}
	if ((options = php_http_options_init(&php_http_curlm_options, 1))) {
		options->getter = php_http_option_get;
		options->setter = php_http_curlm_set_option;

		php_http_curlm_options_init(options);
	}

	if ((info = curl_version_info(CURLVERSION_NOW))) {
		/*
		 * Feature constants
		 */
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "FEATURES", info->features, CONST_CS|CONST_PERSISTENT);

		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "IPV6", CURL_VERSION_IPV6, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "KERBEROS4", CURL_VERSION_KERBEROS4, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,40,0)
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "KERBEROS5", CURL_VERSION_KERBEROS5, CONST_CS|CONST_PERSISTENT);
#endif
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "SSL", CURL_VERSION_SSL, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "LIBZ", CURL_VERSION_LIBZ, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "NTLM", CURL_VERSION_NTLM, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "GSSNEGOTIATE", CURL_VERSION_GSSNEGOTIATE, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "ASYNCHDNS", CURL_VERSION_ASYNCHDNS, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "SPNEGO", CURL_VERSION_SPNEGO, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "LARGEFILE", CURL_VERSION_LARGEFILE, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "IDN", CURL_VERSION_IDN, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "SSPI", CURL_VERSION_SSPI, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,38,0)
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "GSSAPI", CURL_VERSION_GSSAPI, CONST_CS|CONST_PERSISTENT);
#endif
#if PHP_HTTP_CURL_VERSION(7,21,4)
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "TLSAUTH_SRP", CURL_VERSION_TLSAUTH_SRP, CONST_CS|CONST_PERSISTENT);
#endif
#if PHP_HTTP_CURL_VERSION(7,22,0)
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "NTLM_WB", CURL_VERSION_NTLM_WB, CONST_CS|CONST_PERSISTENT);
#endif
#if PHP_HTTP_CURL_VERSION(7,33,0)
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "HTTP2", CURL_VERSION_HTTP2, CONST_CS|CONST_PERSISTENT);
#endif
#if PHP_HTTP_CURL_VERSION(7,40,0)
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "UNIX_SOCKETS", CURL_VERSION_UNIX_SOCKETS, CONST_CS|CONST_PERSISTENT);
#endif
#if PHP_HTTP_CURL_VERSION(7,47,0)
		REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl\\Features", "PSL", CURL_VERSION_PSL, CONST_CS|CONST_PERSISTENT);
#endif

		/*
		 * Version constants
		 */
		REGISTER_NS_STRING_CONSTANT("http\\Client\\Curl", "VERSIONS", curl_version(), CONST_CS|CONST_PERSISTENT);
#if CURLVERSION_NOW >= 0
		REGISTER_NS_STRING_CONSTANT("http\\Client\\Curl\\Versions", "CURL", (char *) info->version, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_STRING_OR_NULL_CONSTANT("http\\Client\\Curl\\Versions", "SSL", (char *) info->ssl_version, CONST_CS|CONST_PERSISTENT);
		REGISTER_NS_STRING_OR_NULL_CONSTANT("http\\Client\\Curl\\Versions", "LIBZ", (char *) info->libz_version, CONST_CS|CONST_PERSISTENT);
# if CURLVERSION_NOW >= 1
		REGISTER_NS_STRING_OR_NULL_CONSTANT("http\\Client\\Curl\\Versions", "ARES", (char *) info->ares, CONST_CS|CONST_PERSISTENT);
#  if CURLVERSION_NOW >= 2
		REGISTER_NS_STRING_OR_NULL_CONSTANT("http\\Client\\Curl\\Versions", "IDN", (char *) info->libidn, CONST_CS|CONST_PERSISTENT);
#  endif
# endif
#endif
	}

	/*
	* HTTP Protocol Version Constants
	*/
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "HTTP_VERSION_1_0", CURL_HTTP_VERSION_1_0, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "HTTP_VERSION_1_1", CURL_HTTP_VERSION_1_1, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,33,0)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "HTTP_VERSION_2_0", CURL_HTTP_VERSION_2_0, CONST_CS|CONST_PERSISTENT);
#endif
#if PHP_HTTP_CURL_VERSION(7,47,0)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "HTTP_VERSION_2TLS", CURL_HTTP_VERSION_2TLS, CONST_CS|CONST_PERSISTENT);
#endif
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "HTTP_VERSION_ANY", CURL_HTTP_VERSION_NONE, CONST_CS|CONST_PERSISTENT);

	/*
	* SSL Version Constants
	*/
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "SSL_VERSION_TLSv1", CURL_SSLVERSION_TLSv1, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,34,0)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "SSL_VERSION_TLSv1_0", CURL_SSLVERSION_TLSv1_0, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "SSL_VERSION_TLSv1_1", CURL_SSLVERSION_TLSv1_1, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "SSL_VERSION_TLSv1_2", CURL_SSLVERSION_TLSv1_2, CONST_CS|CONST_PERSISTENT);
#endif
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "SSL_VERSION_SSLv2", CURL_SSLVERSION_SSLv2, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "SSL_VERSION_SSLv3", CURL_SSLVERSION_SSLv3, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "SSL_VERSION_ANY", CURL_SSLVERSION_DEFAULT, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,21,4) && PHP_HTTP_HAVE_LIBCURL_TLSAUTH_TYPE
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "TLSAUTH_SRP", CURL_TLSAUTH_SRP, CONST_CS|CONST_PERSISTENT);
#endif

	/*
	* DNS IPvX resolving
	*/
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "IPRESOLVE_V4", CURL_IPRESOLVE_V4, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "IPRESOLVE_V6", CURL_IPRESOLVE_V6, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "IPRESOLVE_ANY", CURL_IPRESOLVE_WHATEVER, CONST_CS|CONST_PERSISTENT);

	/*
	* Auth Constants
	*/
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "AUTH_BASIC", CURLAUTH_BASIC, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "AUTH_DIGEST", CURLAUTH_DIGEST, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,19,3)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "AUTH_DIGEST_IE", CURLAUTH_DIGEST_IE, CONST_CS|CONST_PERSISTENT);
#endif
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "AUTH_NTLM", CURLAUTH_NTLM, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "AUTH_GSSNEG", CURLAUTH_GSSNEGOTIATE, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,38,0)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "AUTH_SPNEGO", CURLAUTH_NEGOTIATE, CONST_CS|CONST_PERSISTENT);
#endif
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "AUTH_ANY", CURLAUTH_ANY, CONST_CS|CONST_PERSISTENT);

	/*
	* Proxy Type Constants
	*/
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "PROXY_SOCKS4", CURLPROXY_SOCKS4, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "PROXY_SOCKS4A", CURLPROXY_SOCKS5, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "PROXY_SOCKS5_HOSTNAME", CURLPROXY_SOCKS5, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "PROXY_SOCKS5", CURLPROXY_SOCKS5, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "PROXY_HTTP", CURLPROXY_HTTP, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,19,4)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "PROXY_HTTP_1_0", CURLPROXY_HTTP_1_0, CONST_CS|CONST_PERSISTENT);
#endif

	/*
	* Post Redirection Constants
	*/
#if PHP_HTTP_CURL_VERSION(7,19,1)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "POSTREDIR_301", CURL_REDIR_POST_301, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "POSTREDIR_302", CURL_REDIR_POST_302, CONST_CS|CONST_PERSISTENT);
#if PHP_HTTP_CURL_VERSION(7,26,0)
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "POSTREDIR_303", CURL_REDIR_POST_303, CONST_CS|CONST_PERSISTENT);
#endif
	REGISTER_NS_LONG_CONSTANT("http\\Client\\Curl", "POSTREDIR_ALL", CURL_REDIR_POST_ALL, CONST_CS|CONST_PERSISTENT);
#endif

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_client_curl)
{
	php_persistent_handle_cleanup(PHP_HTTP_G->client.curl.driver.client_name, NULL);
	php_persistent_handle_cleanup(PHP_HTTP_G->client.curl.driver.request_name, NULL);
	zend_string_release(PHP_HTTP_G->client.curl.driver.client_name);
	zend_string_release(PHP_HTTP_G->client.curl.driver.request_name);
	zend_string_release(PHP_HTTP_G->client.curl.driver.driver_name);

	php_http_options_dtor(&php_http_curle_options);
	php_http_options_dtor(&php_http_curlm_options);

	return SUCCESS;
}

#endif /* PHP_HTTP_HAVE_LIBCURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
