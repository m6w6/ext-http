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

#include "php_http_api.h"

#if PHP_HTTP_HAVE_CURL

#include <curl/curl.h>
#define PHP_HTTP_CURL_VERSION(x, y, z) (LIBCURL_VERSION_NUM >= (((x)<<16) + ((y)<<8) + (z)))

#if PHP_HTTP_HAVE_EVENT
#	include <event.h>
#endif

typedef struct php_http_curl_request {
	CURL *handle;

	struct {
		HashTable cache;

		struct curl_slist *headers;
		struct curl_slist *resolve;
		php_http_buffer_t cookies;

		long redirects;

		struct {
			uint count;
			double delay;
		} retry;

	} options;

	php_http_request_progress_t progress;

} php_http_curl_request_t;

typedef struct php_http_curl_request_storage {
	char *url;
	char *cookiestore;
	char errorbuffer[0x100];
} php_http_curl_request_storage_t;

typedef struct php_http_curl_request_pool {
	CURLM *handle;

	int unfinished;  /* int because of curl_multi_perform() */

#if PHP_HTTP_HAVE_EVENT
	struct event *timeout;
	unsigned useevents:1;
	unsigned runsocket:1;
#endif
} php_http_curl_request_pool_t;

typedef struct php_http_curl_request_datashare {
	CURLSH *handle;
} php_http_curl_request_datashare_t;

#define PHP_HTTP_CURL_OPT_STRING(OPTION, ldiff, obdc) \
	{ \
		char *K = #OPTION; \
		PHP_HTTP_CURL_OPT_STRING_EX(K+lenof("CURLOPT_KEY")+ldiff, OPTION, obdc); \
	}
#define PHP_HTTP_CURL_OPT_STRING_EX(keyname, optname, obdc) \
	if (!strcasecmp(key.str, keyname)) { \
		zval *copy = cache_option(&curl->options.cache, keyname, strlen(keyname)+1, 0, php_http_ztyp(IS_STRING, *param)); \
		if (obdc) { \
			if (SUCCESS != php_check_open_basedir(Z_STRVAL_P(copy) TSRMLS_CC)) { \
				return FAILURE; \
			} \
		} \
		curl_easy_setopt(ch, optname, Z_STRVAL_P(copy)); \
		zval_ptr_dtor(&copy); \
		continue; \
	}
#define PHP_HTTP_CURL_OPT_LONG(OPTION, ldiff) \
	{ \
		char *K = #OPTION; \
		PHP_HTTP_CURL_OPT_LONG_EX(K+lenof("CURLOPT_KEY")+ldiff, OPTION); \
	}
#define PHP_HTTP_CURL_OPT_LONG_EX(keyname, optname) \
	if (!strcasecmp(key.str, keyname)) { \
		zval *copy = php_http_ztyp(IS_LONG, *param); \
		curl_easy_setopt(ch, optname, Z_LVAL_P(copy)); \
		zval_ptr_dtor(&copy); \
		continue; \
	}

static inline php_http_curl_request_storage_t *get_storage(CURL *ch) {
	php_http_curl_request_storage_t *st = NULL;

	curl_easy_getinfo(ch, CURLINFO_PRIVATE, &st);

	if (!st) {
		st = pecalloc(1, sizeof(*st), 1);
		curl_easy_setopt(ch, CURLOPT_PRIVATE, st);
		curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, st->errorbuffer);
	}

	return st;
}

/* resource_factory ops */

static void *php_http_curl_ctor(void *opaque TSRMLS_DC)
{
	void *ch;

	if ((ch = curl_easy_init())) {
		get_storage(ch);
		return ch;
	}
	return NULL;
}

static void *php_http_curl_copy(void *opaque, void *handle TSRMLS_DC)
{
	void *ch;

	if ((ch = curl_easy_duphandle(handle))) {
		curl_easy_reset(ch);
		get_storage(ch);
		return ch;
	}
	return NULL;
}

static void php_http_curl_dtor(void *opaque, void *handle TSRMLS_DC)
{
	php_http_curl_request_storage_t *st = get_storage(handle);

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

static void *php_http_curlm_ctor(void *opaque TSRMLS_DC)
{
	return curl_multi_init();
}

static void php_http_curlm_dtor(void *opaque, void *handle TSRMLS_DC)
{
	curl_multi_cleanup(handle);
}

static void *php_http_curlsh_ctor(void *opaque TSRMLS_DC)
{
	return curl_share_init();
}

static void php_http_curlsh_dtor(void *opaque, void *handle TSRMLS_DC)
{
	curl_share_cleanup(handle);
}

/* callbacks */

static size_t php_http_curl_read_callback(void *data, size_t len, size_t n, void *ctx)
{
	php_http_message_body_t *body = ctx;

	if (body) {
		TSRMLS_FETCH_FROM_CTX(body->ts);
		return php_stream_read(php_http_message_body_stream(body), data, len * n);
	}
	return 0;
}

static int php_http_curl_progress_callback(void *ctx, double dltotal, double dlnow, double ultotal, double ulnow)
{
	php_http_request_t *h = ctx;
	php_http_curl_request_t *curl = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	curl->progress.state.dl.total = dltotal;
	curl->progress.state.dl.now = dlnow;
	curl->progress.state.ul.total = ultotal;
	curl->progress.state.ul.now = ulnow;

	php_http_request_progress_notify(&curl->progress TSRMLS_CC);

	return 0;
}

static curlioerr php_http_curl_ioctl_callback(CURL *ch, curliocmd cmd, void *ctx)
{
	php_http_message_body_t *body = ctx;

	if (cmd != CURLIOCMD_RESTARTREAD) {
		return CURLIOE_UNKNOWNCMD;
	}

	if (body) {
		TSRMLS_FETCH_FROM_CTX(body->ts);

		if (SUCCESS == php_stream_rewind(php_http_message_body_stream(body))) {
			return CURLIOE_OK;
		}
	}

	return CURLIOE_FAILRESTART;
}

static int php_http_curl_raw_callback(CURL *ch, curl_infotype type, char *data, size_t length, void *ctx)
{
	php_http_request_t *h = ctx;
	php_http_curl_request_t *curl = h->ctx;
	unsigned flags = 0;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	/* catch progress */
	switch (type) {
		case CURLINFO_TEXT:
			if (php_memnstr(data, ZEND_STRL("About to connect"), data + length)) {
				curl->progress.state.info = "resolve";
			} else if (php_memnstr(data, ZEND_STRL("Trying"), data + length)) {
				curl->progress.state.info = "connect";
			} else if (php_memnstr(data, ZEND_STRL("Connected"), data + length)) {
				curl->progress.state.info = "connected";
			} else if (php_memnstr(data, ZEND_STRL("left intact"), data + length)) {
				curl->progress.state.info = "not disconnected";
			} else if (php_memnstr(data, ZEND_STRL("closed"), data + length)) {
				curl->progress.state.info = "disconnected";
			} else if (php_memnstr(data, ZEND_STRL("Issue another request"), data + length)) {
				curl->progress.state.info = "redirect";
			}
			php_http_request_progress_notify(&curl->progress TSRMLS_CC);
			break;
		case CURLINFO_HEADER_OUT:
		case CURLINFO_DATA_OUT:
		case CURLINFO_SSL_DATA_OUT:
			curl->progress.state.info = "send";
			break;
		case CURLINFO_HEADER_IN:
		case CURLINFO_DATA_IN:
		case CURLINFO_SSL_DATA_IN:
			curl->progress.state.info = "receive";
			break;
		default:
			break;
	}
	/* process data */
	switch (type) {
		case CURLINFO_HEADER_IN:
		case CURLINFO_DATA_IN:
		case CURLINFO_HEADER_OUT:
		case CURLINFO_DATA_OUT:
			php_http_buffer_append(h->buffer, data, length);

			if (curl->options.redirects) {
				flags |= PHP_HTTP_MESSAGE_PARSER_EMPTY_REDIRECTS;
			}

			if (PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE == php_http_message_parser_parse(h->parser, h->buffer, flags, &h->message)) {
				return -1;
			}
			break;
		default:
			break;
	}

#if 0
	/* debug */
	_dpf(type, data, length);
#endif

	return 0;
}

static int php_http_curl_dummy_callback(char *data, size_t n, size_t l, void *s)
{
	return n*l;
}

static STATUS php_http_curl_request_prepare(php_http_request_t *h, const char *meth, const char *url, php_http_message_body_t *body)
{
	php_http_curl_request_t *curl = h->ctx;
	php_http_curl_request_storage_t *storage = get_storage(curl->handle);
	TSRMLS_FETCH_FROM_CTX(h->ts);

	storage->errorbuffer[0] = '\0';
	if (storage->url) {
		pefree(storage->url, 1);
	}
	storage->url = pestrdup(url, 1);
	curl_easy_setopt(curl->handle, CURLOPT_URL, storage->url);

	/* request method */
	switch (php_http_select_str(meth, 4, "GET", "HEAD", "POST", "PUT")) {
		case 0:
			curl_easy_setopt(curl->handle, CURLOPT_HTTPGET, 1L);
			break;

		case 1:
			curl_easy_setopt(curl->handle, CURLOPT_NOBODY, 1L);
			break;

		case 2:
			curl_easy_setopt(curl->handle, CURLOPT_POST, 1L);
			break;

		case 3:
			curl_easy_setopt(curl->handle, CURLOPT_UPLOAD, 1L);
			break;

		default: {
			if (meth) {
				curl_easy_setopt(curl->handle, CURLOPT_CUSTOMREQUEST, meth);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_METHOD, "Unsupported request method: '%s' (%s)", meth, url);
				return FAILURE;
			}
			break;
		}
	}

	/* attach request body */
	if (body) {
		/* RFC2616, section 4.3 (para. 4) states that »a message-body MUST NOT be included in a request if the
		 * specification of the request method (section 5.1.1) does not allow sending an entity-body in request.«
		 * Following the clause in section 5.1.1 (para. 2) that request methods »MUST be implemented with the
		 * same semantics as those specified in section 9« reveal that not any single defined HTTP/1.1 method
		 * does not allow a request body.
		 */
		size_t body_size = php_http_message_body_size(body);

		curl_easy_setopt(curl->handle, CURLOPT_IOCTLDATA, body);
		curl_easy_setopt(curl->handle, CURLOPT_READDATA, body);
		curl_easy_setopt(curl->handle, CURLOPT_INFILESIZE, body_size);
		curl_easy_setopt(curl->handle, CURLOPT_POSTFIELDSIZE, body_size);
	}

	return SUCCESS;
}

static void php_http_curl_request_pool_responsehandler(php_http_request_pool_t *pool)
{
	int remaining = 0;
	zval **requests;
	php_http_curl_request_pool_t *curl = pool->ctx;
	TSRMLS_FETCH_FROM_CTX(pool->ts);

	do {
		CURLMsg *msg = curl_multi_info_read(curl->handle, &remaining);

		if (msg && CURLMSG_DONE == msg->msg) {
			zval **request;

			if (CURLE_OK != msg->data.result) {
				php_http_curl_request_storage_t *st = get_storage(msg->easy_handle);
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(msg->data.result), STR_PTR(st->errorbuffer), STR_PTR(st->url));
			}

			php_http_request_pool_requests(pool, &requests, NULL);
			for (request = requests; *request; ++request) {
				php_http_request_object_t *obj = zend_object_store_get_object(*request TSRMLS_CC);

				if (msg->easy_handle == ((php_http_curl_request_t *) (obj->request->ctx))->handle) {
					Z_ADDREF_PP(request);
					zend_llist_add_element(&pool->requests.finished, request);
					php_http_request_object_responsehandler(obj, *request TSRMLS_CC);
				}

				zval_ptr_dtor(request);
			}
			efree(requests);
		}
	} while (remaining);
}


#if PHP_HTTP_HAVE_EVENT

typedef struct php_http_request_pool_event {
	struct event evnt;
	php_http_request_pool_t *pool;
} php_http_request_pool_event_t;

static inline int etoca(short action) {
	switch (action & (EV_READ|EV_WRITE)) {
		case EV_READ:
			return CURL_CSELECT_IN;
			break;
		case EV_WRITE:
			return CURL_CSELECT_OUT;
			break;
		case EV_READ|EV_WRITE:
			return CURL_CSELECT_IN|CURL_CSELECT_OUT;
			break;
		default:
			return 0;
	}
}

static void php_http_curl_request_pool_timeout_callback(int socket, short action, void *event_data)
{
	php_http_request_pool_t *pool = event_data;
	php_http_curl_request_pool_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "T");
#endif
	if (curl->useevents) {
		CURLMcode rc;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

		while (CURLM_CALL_MULTI_PERFORM == (rc = curl_multi_socket_action(curl->handle, socket, etoca(action), &curl->unfinished)));

		if (CURLM_OK != rc) {
			php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "%s",  curl_multi_strerror(rc));
		}

		php_http_curl_request_pool_responsehandler(pool);
	}
}

static void php_http_curl_request_pool_event_callback(int socket, short action, void *event_data)
{
	php_http_request_pool_t *pool = event_data;
	php_http_curl_request_pool_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "E");
#endif
	if (curl->useevents) {
		CURLMcode rc = CURLE_OK;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

		while (CURLM_CALL_MULTI_PERFORM == (rc = curl_multi_socket_action(curl->handle, socket, etoca(action), &curl->unfinished)));

		if (CURLM_OK != rc) {
			php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "%s", curl_multi_strerror(rc));
		}

		php_http_curl_request_pool_responsehandler(pool);

		/* remove timeout if there are no transfers left */
		if (!curl->unfinished && event_initialized(curl->timeout) && event_pending(curl->timeout, EV_TIMEOUT, NULL)) {
			event_del(curl->timeout);
		}
	}
}

static int php_http_curl_request_pool_socket_callback(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	php_http_request_pool_t *pool = socket_data;
	php_http_curl_request_pool_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "S");
#endif
	if (curl->useevents) {
		int events = EV_PERSIST;
		php_http_request_pool_event_t *ev = assign_data;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

		if (!ev) {
			ev = ecalloc(1, sizeof(php_http_request_pool_event_t));
			ev->pool = pool;
			curl_multi_assign(curl->handle, sock, ev);
			event_base_set(PHP_HTTP_G->curl.event_base, &ev->evnt);
		} else {
			event_del(&ev->evnt);
		}

		switch (action) {
			case CURL_POLL_IN:
				events |= EV_READ;
				break;
			case CURL_POLL_OUT:
				events |= EV_WRITE;
				break;
			case CURL_POLL_INOUT:
				events |= EV_READ|EV_WRITE;
				break;

			case CURL_POLL_REMOVE:
				efree(ev);
				/* no break */
			case CURL_POLL_NONE:
				return 0;

			default:
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "Unknown socket action %d", action);
				return -1;
		}

		event_set(&ev->evnt, sock, events, php_http_curl_request_pool_event_callback, pool);
		event_add(&ev->evnt, NULL);
	}

	return 0;
}

static void php_http_curl_request_pool_timer_callback(CURLM *multi, long timeout_ms, void *timer_data)
{
	php_http_request_pool_t *pool = timer_data;
	php_http_curl_request_pool_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "%ld", timeout_ms);
#endif
	if (curl->useevents) {

		if (timeout_ms < 0) {
			php_http_curl_request_pool_timeout_callback(CURL_SOCKET_TIMEOUT, CURL_CSELECT_IN|CURL_CSELECT_OUT, pool);
		} else if (timeout_ms > 0 || !event_initialized(curl->timeout) || !event_pending(curl->timeout, EV_TIMEOUT, NULL)) {
			struct timeval timeout;
			TSRMLS_FETCH_FROM_CTX(pool->ts);

			if (!event_initialized(curl->timeout)) {
				event_set(curl->timeout, -1, 0, php_http_curl_request_pool_timeout_callback, pool);
				event_base_set(PHP_HTTP_G->curl.event_base, curl->timeout);
			} else if (event_pending(curl->timeout, EV_TIMEOUT, NULL)) {
				event_del(curl->timeout);
			}

			timeout.tv_sec = timeout_ms / 1000;
			timeout.tv_usec = (timeout_ms % 1000) * 1000;

			event_add(curl->timeout, &timeout);
		}
	}
}

#endif /* HAVE_EVENT */


static inline zval *cache_option(HashTable *cache, char *key, size_t keylen, ulong h, zval *opt)
{
	Z_ADDREF_P(opt);

	if (h) {
		zend_hash_quick_update(cache, key, keylen, h, &opt, sizeof(zval *), NULL);
	} else {
		zend_hash_update(cache, key, keylen, &opt, sizeof(zval *), NULL);
	}

	return opt;
}

static inline zval *get_option(HashTable *cache, HashTable *options, char *key, size_t keylen, int type)
{
	if (options) {
		zval **zoption;
		ulong h = zend_hash_func(key, keylen);

		if (SUCCESS == zend_hash_quick_find(options, key, keylen, h, (void *) &zoption)) {
			zval *option = php_http_ztyp(type, *zoption);

			if (cache) {
				zval *cached = cache_option(cache, key, keylen, h, option);

				zval_ptr_dtor(&option);
				return cached;
			}
			return option;
		}
	}

	return NULL;
}

static STATUS set_options(php_http_request_t *h, HashTable *options)
{
	zval *zoption;
	int range_req = 0;
	php_http_curl_request_t *curl = h->ctx;
	CURL *ch = curl->handle;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	/* proxy */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("proxyhost"), IS_STRING))) {
		curl_easy_setopt(ch, CURLOPT_PROXY, Z_STRVAL_P(zoption));
		/* type */
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("proxytype"), IS_LONG))) {
			curl_easy_setopt(ch, CURLOPT_PROXYTYPE, Z_LVAL_P(zoption));
		}
		/* port */
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("proxyport"), IS_LONG))) {
			curl_easy_setopt(ch, CURLOPT_PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("proxyauth"), IS_STRING)) && Z_STRLEN_P(zoption)) {
			curl_easy_setopt(ch, CURLOPT_PROXYUSERPWD, Z_STRVAL_P(zoption));
		}
		/* auth method */
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("proxyauthtype"), IS_LONG))) {
			curl_easy_setopt(ch, CURLOPT_PROXYAUTH, Z_LVAL_P(zoption));
		}
		/* tunnel */
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("proxytunnel"), IS_BOOL)) && Z_BVAL_P(zoption)) {
			curl_easy_setopt(ch, CURLOPT_HTTPPROXYTUNNEL, 1L);
		}
	}
#if PHP_HTTP_CURL_VERSION(7,19,4)
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("noproxy"), IS_STRING))) {
		curl_easy_setopt(ch, CURLOPT_NOPROXY, Z_STRVAL_P(zoption));
	}
#endif

	/* dns */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("dns_cache_timeout"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_DNS_CACHE_TIMEOUT, Z_LVAL_P(zoption));
	}
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("ipresolve"), IS_LONG)) && Z_LVAL_P(zoption)) {
		curl_easy_setopt(ch, CURLOPT_IPRESOLVE, Z_LVAL_P(zoption));
	}
#if PHP_HTTP_CURL_VERSION(7,21,3)
	if (curl->options.resolve) {
		curl_slist_free_all(curl->options.resolve);
		curl->options.resolve = NULL;
	}
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("resolve"), IS_ARRAY))) {
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		HashPosition pos;
		zval **data;

		FOREACH_KEYVAL(pos, zoption, key, data) {
			zval *cpy = php_http_ztyp(IS_STRING, *data);

			curl->options.resolve = curl_slist_append(curl->options.resolve, Z_STRVAL_P(cpy));

			zval_ptr_dtor(&cpy);
		}
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,24,0)
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("dns_servers"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		curl_easy_setopt(ch, CURLOPT_DNS_SERVERS, Z_STRVAL_P(zoption));
	}
#endif

	/* limits */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("low_speed_limit"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_LOW_SPEED_LIMIT, Z_LVAL_P(zoption));
	}
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("low_speed_time"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_LOW_SPEED_TIME, Z_LVAL_P(zoption));
	}
	/* LSF weirdance
	if ((zoption = get_option(&curl->cache.options, options, ZEND_STRS("max_send_speed"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t) Z_LVAL_P(zoption));
	}
	if ((zoption = get_option(&curl->cache.options, options, ZEND_STRS("max_recv_speed"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t) Z_LVAL_P(zoption));
	}
	*/
	/* crashes
	if ((zoption = get_option(&curl->cache.options, options, ZEND_STRS("maxconnects"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_MAXCONNECTS, Z_LVAL_P(zoption));
	} */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("fresh_connect"), IS_BOOL)) && Z_BVAL_P(zoption)) {
		curl_easy_setopt(ch, CURLOPT_FRESH_CONNECT, 1L);
	}
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("forbid_reuse"), IS_BOOL)) && Z_BVAL_P(zoption)) {
		curl_easy_setopt(ch, CURLOPT_FORBID_REUSE, 1L);
	}

	/* outgoing interface */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("interface"), IS_STRING))) {
		curl_easy_setopt(ch, CURLOPT_INTERFACE, Z_STRVAL_P(zoption));

		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("portrange"), IS_ARRAY))) {
			zval **prs, **pre;

			zend_hash_internal_pointer_reset(Z_ARRVAL_P(zoption));
			if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void *) &prs)) {
				zend_hash_move_forward(Z_ARRVAL_P(zoption));
				if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void *) &pre)) {
					zval *prs_cpy = php_http_ztyp(IS_LONG, *prs);
					zval *pre_cpy = php_http_ztyp(IS_LONG, *pre);

					if (Z_LVAL_P(prs_cpy) && Z_LVAL_P(pre_cpy)) {
						curl_easy_setopt(ch, CURLOPT_LOCALPORT, MIN(Z_LVAL_P(prs_cpy), Z_LVAL_P(pre_cpy)));
						curl_easy_setopt(ch, CURLOPT_LOCALPORTRANGE, labs(Z_LVAL_P(prs_cpy)-Z_LVAL_P(pre_cpy))+1L);
					}
					zval_ptr_dtor(&prs_cpy);
					zval_ptr_dtor(&pre_cpy);
				}
			}
		}
	}

	/* another port */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("port"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_PORT, Z_LVAL_P(zoption));
	}

	/* RFC4007 zone_id */
#if PHP_HTTP_CURL_VERSION(7,19,0)
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("address_scope"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_ADDRESS_SCOPE, Z_LVAL_P(zoption));
	}
#endif

	/* auth */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("httpauth"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		curl_easy_setopt(ch, CURLOPT_USERPWD, Z_STRVAL_P(zoption));
	}
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("httpauthtype"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_HTTPAUTH, Z_LVAL_P(zoption));
	}

	/* redirects, defaults to 0 */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("redirect"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1L : 0L);
		curl_easy_setopt(ch, CURLOPT_MAXREDIRS, curl->options.redirects = Z_LVAL_P(zoption));
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("unrestrictedauth"), IS_BOOL))) {
			curl_easy_setopt(ch, CURLOPT_UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("postredir"), IS_BOOL))) {
#if PHP_HTTP_CURL_VERSION(7,19,1)
			curl_easy_setopt(ch, CURLOPT_POSTREDIR, Z_BVAL_P(zoption) ? 1L : 0L);
#else
			curl_easy_setopt(ch, CURLOPT_POST301, Z_BVAL_P(zoption) ? 1L : 0L);
#endif
		}
	} else {
		curl->options.redirects = 0;
	}

	/* retries, defaults to 0 */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("retrycount"), IS_LONG))) {
		curl->options.retry.count = Z_LVAL_P(zoption);
		if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("retrydelay"), IS_DOUBLE))) {
			curl->options.retry.delay = Z_DVAL_P(zoption);
		} else {
			curl->options.retry.delay = 0;
		}
	} else {
		curl->options.retry.count = 0;
	}

	/* referer */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("referer"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		curl_easy_setopt(ch, CURLOPT_REFERER, Z_STRVAL_P(zoption));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("useragent"), IS_STRING))) {
		/* allow to send no user agent, not even default one */
		if (Z_STRLEN_P(zoption)) {
			curl_easy_setopt(ch, CURLOPT_USERAGENT, Z_STRVAL_P(zoption));
		} else {
			curl_easy_setopt(ch, CURLOPT_USERAGENT, NULL);
		}
	}

	/* resume */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("resume"), IS_LONG)) && (Z_LVAL_P(zoption) > 0)) {
		range_req = 1;
		curl_easy_setopt(ch, CURLOPT_RESUME_FROM, Z_LVAL_P(zoption));
	}
	/* or range of kind array(array(0,499), array(100,1499)) */
	else if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("range"), IS_ARRAY)) && zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
		HashPosition pos1, pos2;
		zval **rr, **rb, **re;
		php_http_buffer_t rs;

		php_http_buffer_init(&rs);
		FOREACH_VAL(pos1, zoption, rr) {
			if (Z_TYPE_PP(rr) == IS_ARRAY) {
				zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(rr), &pos2);
				if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(rr), (void *) &rb, &pos2)) {
					zend_hash_move_forward_ex(Z_ARRVAL_PP(rr), &pos2);
					if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(rr), (void *) &re, &pos2)) {
						if (	((Z_TYPE_PP(rb) == IS_LONG) || ((Z_TYPE_PP(rb) == IS_STRING) && is_numeric_string(Z_STRVAL_PP(rb), Z_STRLEN_PP(rb), NULL, NULL, 1))) &&
								((Z_TYPE_PP(re) == IS_LONG) || ((Z_TYPE_PP(re) == IS_STRING) && is_numeric_string(Z_STRVAL_PP(re), Z_STRLEN_PP(re), NULL, NULL, 1)))) {
							zval *rbl = php_http_ztyp(IS_LONG, *rb);
							zval *rel = php_http_ztyp(IS_LONG, *re);

							if ((Z_LVAL_P(rbl) >= 0) && (Z_LVAL_P(rel) >= 0)) {
								php_http_buffer_appendf(&rs, "%ld-%ld,", Z_LVAL_P(rbl), Z_LVAL_P(rel));
							}
							zval_ptr_dtor(&rbl);
							zval_ptr_dtor(&rel);
						}
					}
				}
			}
		}

		if (PHP_HTTP_BUFFER_LEN(&rs)) {
			zval *cached_range;

			/* ditch last comma */
			PHP_HTTP_BUFFER_VAL(&rs)[PHP_HTTP_BUFFER_LEN(&rs)-- -1] = '\0';
			/* cache string */
			MAKE_STD_ZVAL(cached_range);
			ZVAL_STRINGL(cached_range, PHP_HTTP_BUFFER_VAL(&rs), PHP_HTTP_BUFFER_LEN(&rs), 0);
			curl_easy_setopt(ch, CURLOPT_RANGE, Z_STRVAL_P(cache_option(&curl->options.cache, ZEND_STRS("range"), 0, cached_range)));
			zval_ptr_dtor(&cached_range);
		}
	}

	/* additional headers, array('name' => 'value') */
	if (curl->options.headers) {
		curl_slist_free_all(curl->options.headers);
		curl->options.headers = NULL;
	}
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("headers"), IS_ARRAY))) {
		php_http_array_hashkey_t header_key = php_http_array_hashkey_init(0);
		zval **header_val;
		HashPosition pos;
		php_http_buffer_t header;

		php_http_buffer_init(&header);
		FOREACH_KEYVAL(pos, zoption, header_key, header_val) {
			if (header_key.type == HASH_KEY_IS_STRING) {
				zval *header_cpy = php_http_ztyp(IS_STRING, *header_val);

				if (!strcasecmp(header_key.str, "range")) {
					range_req = 1;
				}

				php_http_buffer_appendf(&header, "%s: %s", header_key.str, Z_STRVAL_P(header_cpy));
				php_http_buffer_fix(&header);
				curl->options.headers = curl_slist_append(curl->options.headers, PHP_HTTP_BUFFER_VAL(&header));
				php_http_buffer_reset(&header);

				zval_ptr_dtor(&header_cpy);
			}
		}
		php_http_buffer_dtor(&header);
	}
	/* etag */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("etag"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		zend_bool is_quoted = !((Z_STRVAL_P(zoption)[0] != '"') || (Z_STRVAL_P(zoption)[Z_STRLEN_P(zoption)-1] != '"'));
		php_http_buffer_t header;

		php_http_buffer_init(&header);
		php_http_buffer_appendf(&header, is_quoted?"%s: %s":"%s: \"%s\"", range_req?"If-Match":"If-None-Match", Z_STRVAL_P(zoption));
		php_http_buffer_fix(&header);
		curl->options.headers = curl_slist_append(curl->options.headers, PHP_HTTP_BUFFER_VAL(&header));
		php_http_buffer_dtor(&header);
	}
	/* compression */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("compress"), IS_BOOL)) && Z_LVAL_P(zoption)) {
		curl->options.headers = curl_slist_append(curl->options.headers, "Accept-Encoding: gzip;q=1.0,deflate;q=0.5");
	}
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, curl->options.headers);

	/* lastmodified */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("lastmodified"), IS_LONG))) {
		if (Z_LVAL_P(zoption)) {
			if (Z_LVAL_P(zoption) > 0) {
				curl_easy_setopt(ch, CURLOPT_TIMEVALUE, Z_LVAL_P(zoption));
			} else {
				curl_easy_setopt(ch, CURLOPT_TIMEVALUE, (long) PHP_HTTP_G->env.request.time + Z_LVAL_P(zoption));
			}
			curl_easy_setopt(ch, CURLOPT_TIMECONDITION, (long) (range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE));
		} else {
			curl_easy_setopt(ch, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
		}
	}

	/* cookies, array('name' => 'value') */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("cookies"), IS_ARRAY))) {
		php_http_buffer_dtor(&curl->options.cookies);
		if (zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
			zval *urlenc_cookies = NULL;
			/* check whether cookies should not be urlencoded; default is to urlencode them */
			if ((!(urlenc_cookies = get_option(&curl->options.cache, options, ZEND_STRS("encodecookies"), IS_BOOL))) || Z_BVAL_P(urlenc_cookies)) {
				if (SUCCESS == php_http_url_encode_hash_ex(HASH_OF(zoption), &curl->options.cookies, ZEND_STRS(";"), ZEND_STRS("="), NULL, 0 TSRMLS_CC)) {
					php_http_buffer_fix(&curl->options.cookies);
					curl_easy_setopt(ch, CURLOPT_COOKIE, curl->options.cookies.data);
				}
			} else {
				HashPosition pos;
				php_http_array_hashkey_t cookie_key = php_http_array_hashkey_init(0);
				zval **cookie_val;

				FOREACH_KEYVAL(pos, zoption, cookie_key, cookie_val) {
					if (cookie_key.type == HASH_KEY_IS_STRING) {
						zval *val = php_http_ztyp(IS_STRING, *cookie_val);
						php_http_buffer_appendf(&curl->options.cookies, "%s=%s; ", cookie_key.str, Z_STRVAL_P(val));
						zval_ptr_dtor(&val);
					}
				}

				php_http_buffer_fix(&curl->options.cookies);
				if (PHP_HTTP_BUFFER_LEN(&curl->options.cookies)) {
					curl_easy_setopt(ch, CURLOPT_COOKIE, PHP_HTTP_BUFFER_VAL(&curl->options.cookies));
				}
			}
		}
	}

	/* don't load session cookies from cookiestore */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("cookiesession"), IS_BOOL)) && Z_BVAL_P(zoption)) {
		curl_easy_setopt(ch, CURLOPT_COOKIESESSION, 1L);
	}

	/* cookiestore, read initial cookies from that file and store cookies back into that file */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("cookiestore"), IS_STRING))) {
		php_http_curl_request_storage_t *storage = get_storage(curl->handle);

		if (Z_STRLEN_P(zoption)) {
			if (SUCCESS != php_check_open_basedir(Z_STRVAL_P(zoption) TSRMLS_CC)) {
				return FAILURE;
			}
		}
		if (storage->cookiestore) {
			pefree(storage->cookiestore, 1);
		}
		storage->cookiestore = pestrndup(Z_STRVAL_P(zoption), Z_STRLEN_P(zoption), 1);
		curl_easy_setopt(ch, CURLOPT_COOKIEFILE, storage->cookiestore);
		curl_easy_setopt(ch, CURLOPT_COOKIEJAR, storage->cookiestore);
	}

	/* maxfilesize */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("maxfilesize"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* http protocol */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("protocol"), IS_LONG))) {
		curl_easy_setopt(ch, CURLOPT_HTTP_VERSION, Z_LVAL_P(zoption));
	}

	/* timeout, defaults to 0 */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("timeout"), IS_DOUBLE))) {
		curl_easy_setopt(ch, CURLOPT_TIMEOUT_MS, (long)(Z_DVAL_P(zoption)*1000));
	}
	/* connecttimeout, defaults to 0 */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("connecttimeout"), IS_DOUBLE))) {
		curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT_MS, (long)(Z_DVAL_P(zoption)*1000));
	}

	/* ssl */
	if ((zoption = get_option(&curl->options.cache, options, ZEND_STRS("ssl"), IS_ARRAY))) {
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		zval **param;
		HashPosition pos;

		FOREACH_KEYVAL(pos, zoption, key, param) {
			if (key.type == HASH_KEY_IS_STRING) {
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLCERT, 0, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTTYPE, 0, 0);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTPASSWD, 0, 0);

				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLKEY, 0, 0);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYTYPE, 0, 0);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYPASSWD, 0, 0);

				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLENGINE, 0, 0);
				PHP_HTTP_CURL_OPT_LONG(CURLOPT_SSLVERSION, 0);

				PHP_HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYPEER, 1);
				PHP_HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYHOST, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSL_CIPHER_LIST, 1, 0);

				PHP_HTTP_CURL_OPT_STRING(CURLOPT_CAINFO, -3, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_CAPATH, -3, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_RANDOM_FILE, -3, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_EGDSOCKET, -3, 1);
#if PHP_HTTP_CURL_VERSION(7,19,0)
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_ISSUERCERT, -3, 1);
	#if defined(PHP_HTTP_HAVE_OPENSSL)
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_CRLFILE, -3, 1);
	#endif
#endif
#if PHP_HTTP_CURL_VERSION(7,19,1) && defined(PHP_HTTP_HAVE_OPENSSL)
				PHP_HTTP_CURL_OPT_LONG(CURLOPT_CERTINFO, -3);
#endif
			}
		}
	}
	return SUCCESS;
}

static STATUS get_info(CURL *ch, HashTable *info)
{
	char *c;
	long l;
	double d;
	struct curl_slist *s, *p;
	zval *subarray, array;
	INIT_PZVAL_ARRAY(&array, info);

	/* BEGIN::CURLINFO */
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_EFFECTIVE_URL, &c)) {
		add_assoc_string_ex(&array, "effective_url", sizeof("effective_url"), c ? c : "", 1);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &l)) {
		add_assoc_long_ex(&array, "response_code", sizeof("response_code"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_TOTAL_TIME, &d)) {
		add_assoc_double_ex(&array, "total_time", sizeof("total_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_NAMELOOKUP_TIME, &d)) {
		add_assoc_double_ex(&array, "namelookup_time", sizeof("namelookup_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONNECT_TIME, &d)) {
		add_assoc_double_ex(&array, "connect_time", sizeof("connect_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PRETRANSFER_TIME, &d)) {
		add_assoc_double_ex(&array, "pretransfer_time", sizeof("pretransfer_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SIZE_UPLOAD, &d)) {
		add_assoc_double_ex(&array, "size_upload", sizeof("size_upload"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SIZE_DOWNLOAD, &d)) {
		add_assoc_double_ex(&array, "size_download", sizeof("size_download"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SPEED_DOWNLOAD, &d)) {
		add_assoc_double_ex(&array, "speed_download", sizeof("speed_download"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SPEED_UPLOAD, &d)) {
		add_assoc_double_ex(&array, "speed_upload", sizeof("speed_upload"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_HEADER_SIZE, &l)) {
		add_assoc_long_ex(&array, "header_size", sizeof("header_size"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REQUEST_SIZE, &l)) {
		add_assoc_long_ex(&array, "request_size", sizeof("request_size"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SSL_VERIFYRESULT, &l)) {
		add_assoc_long_ex(&array, "ssl_verifyresult", sizeof("ssl_verifyresult"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_FILETIME, &l)) {
		add_assoc_long_ex(&array, "filetime", sizeof("filetime"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &d)) {
		add_assoc_double_ex(&array, "content_length_download", sizeof("content_length_download"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONTENT_LENGTH_UPLOAD, &d)) {
		add_assoc_double_ex(&array, "content_length_upload", sizeof("content_length_upload"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_STARTTRANSFER_TIME, &d)) {
		add_assoc_double_ex(&array, "starttransfer_time", sizeof("starttransfer_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONTENT_TYPE, &c)) {
		add_assoc_string_ex(&array, "content_type", sizeof("content_type"), c ? c : "", 1);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REDIRECT_TIME, &d)) {
		add_assoc_double_ex(&array, "redirect_time", sizeof("redirect_time"), d);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REDIRECT_COUNT, &l)) {
		add_assoc_long_ex(&array, "redirect_count", sizeof("redirect_count"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_HTTP_CONNECTCODE, &l)) {
		add_assoc_long_ex(&array, "connect_code", sizeof("connect_code"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_HTTPAUTH_AVAIL, &l)) {
		add_assoc_long_ex(&array, "httpauth_avail", sizeof("httpauth_avail"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PROXYAUTH_AVAIL, &l)) {
		add_assoc_long_ex(&array, "proxyauth_avail", sizeof("proxyauth_avail"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_OS_ERRNO, &l)) {
		add_assoc_long_ex(&array, "os_errno", sizeof("os_errno"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_NUM_CONNECTS, &l)) {
		add_assoc_long_ex(&array, "num_connects", sizeof("num_connects"), l);
	}
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_SSL_ENGINES, &s)) {
		MAKE_STD_ZVAL(subarray);
		array_init(subarray);
		for (p = s; p; p = p->next) {
			if (p->data) {
				add_next_index_string(subarray, p->data, 1);
			}
		}
		add_assoc_zval_ex(&array, "ssl_engines", sizeof("ssl_engines"), subarray);
		curl_slist_free_all(s);
	}
#if PHP_HTTP_CURL_VERSION(7,14,1)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_COOKIELIST, &s)) {
		MAKE_STD_ZVAL(subarray);
		array_init(subarray);
		for (p = s; p; p = p->next) {
			if (p->data) {
				add_next_index_string(subarray, p->data, 1);
			}
		}
		add_assoc_zval_ex(&array, "cookies", sizeof("cookies"), subarray);
		curl_slist_free_all(s);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,18,2)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_REDIRECT_URL, &c)) {
		add_assoc_string_ex(&array, "redirect_url", sizeof("redirect_url"), c ? c : "", 1);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,19,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_PRIMARY_IP, &c)) {
		add_assoc_string_ex(&array, "primary_ip", sizeof("primary_ip"), c ? c : "", 1);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,19,0)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_APPCONNECT_TIME, &d)) {
		add_assoc_double_ex(&array, "appconnect_time", sizeof("appconnect_time"), d);
	}
#endif
#if PHP_HTTP_CURL_VERSION(7,19,4)
	if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CONDITION_UNMET, &l)) {
		add_assoc_long_ex(&array, "condition_unmet", sizeof("condition_unmet"), l);
	}
#endif
	/* END::CURLINFO */
#if PHP_HTTP_CURL_VERSION(7,19,1) && defined(PHP_HTTP_HAVE_OPENSSL)
	{
		int i;
		zval *ci_array;
		struct curl_certinfo *ci;
		char *colon, *keyname;

		if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_CERTINFO, &ci)) {
			MAKE_STD_ZVAL(ci_array);
			array_init(ci_array);

			for (i = 0; i < ci->num_of_certs; ++i) {
				s = ci->certinfo[i];

				MAKE_STD_ZVAL(subarray);
				array_init(subarray);
				for (p = s; p; p = p->next) {
					if (p->data) {
						if ((colon = strchr(p->data, ':'))) {
							keyname = estrndup(p->data, colon - p->data);
							add_assoc_string_ex(subarray, keyname, colon - p->data + 1, colon + 1, 1);
							efree(keyname);
						} else {
							add_next_index_string(subarray, p->data, 1);
						}
					}
				}
				add_next_index_zval(ci_array, subarray);
			}
			add_assoc_zval_ex(&array, "certinfo", sizeof("certinfo"), ci_array);
		}
	}
#endif
	add_assoc_string_ex(&array, "error", sizeof("error"), get_storage(ch)->errorbuffer, 1);

	return SUCCESS;
}


/* request datashare handler ops */

static php_http_request_datashare_t *php_http_curl_request_datashare_init(php_http_request_datashare_t *h, void *handle)
{
	php_http_curl_request_datashare_t *curl;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (!handle && !(handle = php_http_resource_factory_handle_ctor(h->rf TSRMLS_CC))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_DATASHARE, "could not initialize curl share handle");
		return NULL;
	}

	curl = ecalloc(1, sizeof(*curl));
	curl->handle = handle;
	h->ctx = curl;

	return h;
}

static void php_http_curl_request_datashare_dtor(php_http_request_datashare_t *h)
{
	php_http_curl_request_datashare_t *curl = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	php_http_resource_factory_handle_dtor(h->rf, curl->handle TSRMLS_CC);

	efree(curl);
	h->ctx = NULL;
}

static STATUS php_http_curl_request_datashare_attach(php_http_request_datashare_t *h, php_http_request_t *r)
{
	CURLcode rc;
	php_http_curl_request_datashare_t *curl = h->ctx;
	php_http_curl_request_t *recurl = r->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (CURLE_OK != (rc = curl_easy_setopt(recurl->handle, CURLOPT_SHARE, curl->handle))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_DATASHARE, "Could not attach request to the datashare: %s", curl_easy_strerror(rc));
		return FAILURE;
	}
	return SUCCESS;
}

static STATUS php_http_curl_request_datashare_detach(php_http_request_datashare_t *h, php_http_request_t *r)
{
	CURLcode rc;
	php_http_curl_request_t *recurl = r->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);


	if (CURLE_OK != (rc = curl_easy_setopt(recurl->handle, CURLOPT_SHARE, NULL))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_DATASHARE, "Could not detach request from the datashare: %s", curl_share_strerror(rc));
		return FAILURE;
	}
	return SUCCESS;
}

static STATUS php_http_curl_request_datashare_setopt(php_http_request_datashare_t *h, php_http_request_datashare_setopt_opt_t opt, void *arg)
{
	CURLSHcode rc;
	php_http_curl_request_datashare_t *curl = h->ctx;

	switch (opt) {
		case PHP_HTTP_REQUEST_DATASHARE_OPT_COOKIES:
			if (CURLSHE_OK != (rc = curl_share_setopt(curl->handle, *((zend_bool *) arg) ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, CURL_LOCK_DATA_COOKIE))) {
				TSRMLS_FETCH_FROM_CTX(h->ts);

				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_DATASHARE, "Could not %s sharing of cookie data: %s",  *((zend_bool *) arg) ? "enable" : "disable", curl_share_strerror(rc));
				return FAILURE;
			}
			break;

		case PHP_HTTP_REQUEST_DATASHARE_OPT_RESOLVER:
			if (CURLSHE_OK != (rc = curl_share_setopt(curl->handle, *((zend_bool *) arg) ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, CURL_LOCK_DATA_DNS))) {
				TSRMLS_FETCH_FROM_CTX(h->ts);

				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_DATASHARE, "Could not %s sharing of resolver data: %s",  *((zend_bool *) arg) ? "enable" : "disable", curl_share_strerror(rc));
				return FAILURE;
			}
			break;

#if PHP_HTTP_CURL_VERSION(7,23,0)
		case PHP_HTTP_REQUEST_DATASHARE_OPT_SSLSESSIONS:
			if (CURLSHE_OK != (rc = curl_share_setopt(curl->handle, *((zend_bool *) arg) ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, CURL_LOCK_DATA_SSL_SESSION))) {
				TSRMLS_FETCH_FROM_CTX(h->ts);

				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_DATASHARE, "Could not %s sharing of SSL session data: %s",  *((zend_bool *) arg) ? "enable" : "disable", curl_share_strerror(rc));
				return FAILURE;
			}
			break;
#endif

		default:
			return FAILURE;
	}

	return SUCCESS;
}

static php_http_resource_factory_ops_t php_http_curlsh_resource_factory_ops = {
	php_http_curlsh_ctor,
	NULL,
	php_http_curlsh_dtor
};

static php_http_request_datashare_ops_t php_http_curl_request_datashare_ops = {
		&php_http_curlsh_resource_factory_ops,
		php_http_curl_request_datashare_init,
		NULL /* copy */,
		php_http_curl_request_datashare_dtor,
		NULL /*reset */,
		php_http_curl_request_datashare_attach,
		php_http_curl_request_datashare_detach,
		php_http_curl_request_datashare_setopt,
};

PHP_HTTP_API php_http_request_datashare_ops_t *php_http_curl_get_request_datashare_ops(void)
{
	return &php_http_curl_request_datashare_ops;
}


/* request pool handler ops */

static php_http_request_pool_t *php_http_curl_request_pool_init(php_http_request_pool_t *h, void *handle)
{
	php_http_curl_request_pool_t *curl;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (!handle && !(handle = php_http_resource_factory_handle_ctor(h->rf TSRMLS_CC))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_POOL, "could not initialize curl pool handle");
		return NULL;
	}

	curl = ecalloc(1, sizeof(*curl));
	curl->handle = handle;
	curl->unfinished = 0;
	h->ctx = curl;

	return h;
}

static void php_http_curl_request_pool_dtor(php_http_request_pool_t *h)
{
	php_http_curl_request_pool_t *curl = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

#if PHP_HTTP_HAVE_EVENT
	if (curl->timeout) {
		efree(curl->timeout);
		curl->timeout = NULL;
	}
#endif
	curl->unfinished = 0;
	php_http_request_pool_reset(h);

	php_http_resource_factory_handle_dtor(h->rf, curl->handle TSRMLS_CC);

	efree(curl);
	h->ctx = NULL;
}

static STATUS php_http_curl_request_pool_attach(php_http_request_pool_t *h, php_http_request_t *r, const char *m, const char *url, php_http_message_body_t *body)
{
	php_http_curl_request_pool_t *curl = h->ctx;
	php_http_curl_request_t *recurl = r->ctx;
	CURLMcode rs;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (SUCCESS != php_http_curl_request_prepare(r, m, url, body)) {
		return FAILURE;
	}

	if (CURLM_OK == (rs = curl_multi_add_handle(curl->handle, recurl->handle))) {
		++curl->unfinished;
		return SUCCESS;
	} else {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_POOL, "Could not attach request to pool: %s", curl_multi_strerror(rs));
		return FAILURE;
	}
}

static STATUS php_http_curl_request_pool_detach(php_http_request_pool_t *h, php_http_request_t *r)
{
	php_http_curl_request_pool_t *curl = h->ctx;
	php_http_curl_request_t *recurl = r->ctx;
	CURLMcode rs = curl_multi_remove_handle(curl->handle, recurl->handle);
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (CURLM_OK == rs) {
		return SUCCESS;
	} else {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_POOL, "Could not detach request from pool: %s", curl_multi_strerror(rs));
		return FAILURE;
	}
}

#ifdef PHP_WIN32
#	define SELECT_ERROR SOCKET_ERROR
#else
#	define SELECT_ERROR -1
#endif

static STATUS php_http_curl_request_pool_wait(php_http_request_pool_t *h, struct timeval *custom_timeout)
{
	int MAX;
	fd_set R, W, E;
	struct timeval timeout;
	php_http_curl_request_pool_t *curl = h->ctx;

#if PHP_HTTP_HAVE_EVENT
	if (curl->useevents) {
		TSRMLS_FETCH_FROM_CTX(h->ts);

		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "not implemented");
		return FAILURE;
	}
#endif

	if (custom_timeout && timerisset(custom_timeout)) {
		timeout = *custom_timeout;
	} else {
		long max_tout = 1000;

		if ((CURLM_OK == curl_multi_timeout(curl->handle, &max_tout)) && (max_tout > 0)) {
			timeout.tv_sec = max_tout / 1000;
			timeout.tv_usec = (max_tout % 1000) * 1000;
		} else {
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;
		}
	}

	FD_ZERO(&R);
	FD_ZERO(&W);
	FD_ZERO(&E);

	if (CURLM_OK == curl_multi_fdset(curl->handle, &R, &W, &E, &MAX)) {
		if (MAX == -1) {
			php_http_sleep((double) timeout.tv_sec + (double) (timeout.tv_usec / PHP_HTTP_MCROSEC));
			return SUCCESS;
		} else if (SELECT_ERROR != select(MAX + 1, &R, &W, &E, &timeout)) {
			return SUCCESS;
		}
	}
	return FAILURE;
}

static int php_http_curl_request_pool_once(php_http_request_pool_t *h)
{
	php_http_curl_request_pool_t *curl = h->ctx;

#if PHP_HTTP_HAVE_EVENT
	if (curl->useevents) {
		TSRMLS_FETCH_FROM_CTX(h->ts);
		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "not implemented");
		return FAILURE;
	}
#endif

	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(curl->handle, &curl->unfinished));

	php_http_curl_request_pool_responsehandler(h);

	return curl->unfinished;

}
#if PHP_HTTP_HAVE_EVENT
static void dolog(int i, const char *m) {
	fprintf(stderr, "%d: %s\n", i, m);
}
#endif
static STATUS php_http_curl_request_pool_exec(php_http_request_pool_t *h)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

#if PHP_HTTP_HAVE_EVENT
	php_http_curl_request_pool_t *curl = h->ctx;

	if (curl->useevents) {
		event_set_log_callback(dolog);
		do {
#if DBG_EVENTS
			fprintf(stderr, "X");
#endif
			event_base_dispatch(PHP_HTTP_G->curl.event_base);
		} while (curl->unfinished);
	} else
#endif
	{
		while (php_http_curl_request_pool_once(h)) {
			if (SUCCESS != php_http_curl_request_pool_wait(h, NULL)) {
#ifdef PHP_WIN32
				/* see http://msdn.microsoft.com/library/en-us/winsock/winsock/windows_sockets_error_codes_2.asp */
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "WinSock error: %d", WSAGetLastError());
#else
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, strerror(errno));
#endif
				return FAILURE;
			}
		}
	}

	return SUCCESS;
}

static STATUS php_http_curl_request_pool_setopt(php_http_request_pool_t *h, php_http_request_pool_setopt_opt_t opt, void *arg)
{
	php_http_curl_request_pool_t *curl = h->ctx;

	switch (opt) {
		case PHP_HTTP_REQUEST_POOL_OPT_ENABLE_PIPELINING:
			if (CURLM_OK != curl_multi_setopt(curl->handle, CURLMOPT_PIPELINING, (long) *((zend_bool *) arg))) {
				return FAILURE;
			}
			break;

		case PHP_HTTP_REQUEST_POOL_OPT_USE_EVENTS:
#if PHP_HTTP_HAVE_EVENT
			if ((curl->useevents = *((zend_bool *) arg))) {
				if (!curl->timeout) {
					curl->timeout = ecalloc(1, sizeof(struct event));
				}
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETDATA, h);
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETFUNCTION, php_http_curl_request_pool_socket_callback);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERDATA, h);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERFUNCTION, php_http_curl_request_pool_timer_callback);
			} else {
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETDATA, NULL);
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETFUNCTION, NULL);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERDATA, NULL);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERFUNCTION, NULL);
			}
			break;
#endif

		default:
			return FAILURE;
	}
	return SUCCESS;
}

static php_http_resource_factory_ops_t php_http_curlm_resource_factory_ops = {
	php_http_curlm_ctor,
	NULL,
	php_http_curlm_dtor
};

static php_http_request_pool_ops_t php_http_curl_request_pool_ops = {
		&php_http_curlm_resource_factory_ops,
		php_http_curl_request_pool_init,
		NULL /* copy */,
		php_http_curl_request_pool_dtor,
		NULL /*reset */,
		php_http_curl_request_pool_exec,
		php_http_curl_request_pool_wait,
		php_http_curl_request_pool_once,
		php_http_curl_request_pool_attach,
		php_http_curl_request_pool_detach,
		php_http_curl_request_pool_setopt,
};

PHP_HTTP_API php_http_request_pool_ops_t *php_http_curl_get_request_pool_ops(void)
{
	return &php_http_curl_request_pool_ops;
}

/* request handler ops */

static STATUS php_http_curl_request_reset(php_http_request_t *h);

static php_http_request_t *php_http_curl_request_init(php_http_request_t *h, void *handle)
{
	php_http_curl_request_t *ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (!handle && !(handle = php_http_resource_factory_handle_ctor(h->rf TSRMLS_CC))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "could not initialize curl handle");
		return NULL;
	}

	ctx = ecalloc(1, sizeof(*ctx));
	ctx->handle = handle;
	php_http_buffer_init(&ctx->options.cookies);
	zend_hash_init(&ctx->options.cache, 0, NULL, ZVAL_PTR_DTOR, 0);
	h->ctx = ctx;

#if defined(ZTS)
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
#endif
	curl_easy_setopt(handle, CURLOPT_HEADER, 0L);
	curl_easy_setopt(handle, CURLOPT_FILETIME, 1L);
	curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1L);
	curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, NULL);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, php_http_curl_dummy_callback);
	curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, php_http_curl_raw_callback);
	curl_easy_setopt(handle, CURLOPT_READFUNCTION, php_http_curl_read_callback);
	curl_easy_setopt(handle, CURLOPT_IOCTLFUNCTION, php_http_curl_ioctl_callback);
	curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, php_http_curl_progress_callback);
	curl_easy_setopt(handle, CURLOPT_DEBUGDATA, h);
	curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, h);

	php_http_curl_request_reset(h);

	return h;
}

static php_http_request_t *php_http_curl_request_copy(php_http_request_t *from, php_http_request_t *to)
{
	php_http_curl_request_t *ctx = from->ctx;
	void *copy;
	TSRMLS_FETCH_FROM_CTX(from->ts);

	if (!(copy = php_http_resource_factory_handle_copy(from->rf, ctx->handle TSRMLS_CC))) {
		return NULL;
	}

	if (to) {
		return php_http_curl_request_init(to, copy);
	} else {
		return php_http_request_init(NULL, from->ops, from->rf, copy TSRMLS_CC);
	}
}

static void php_http_curl_request_dtor(php_http_request_t *h)
{
	php_http_curl_request_t *ctx = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	curl_easy_setopt(ctx->handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(ctx->handle, CURLOPT_PROGRESSFUNCTION, NULL);
	curl_easy_setopt(ctx->handle, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(ctx->handle, CURLOPT_DEBUGFUNCTION, NULL);

	php_http_resource_factory_handle_dtor(h->rf, ctx->handle TSRMLS_CC);

	php_http_buffer_dtor(&ctx->options.cookies);
	zend_hash_destroy(&ctx->options.cache);

	if (ctx->options.headers) {
		curl_slist_free_all(ctx->options.headers);
		ctx->options.headers = NULL;
	}
	php_http_request_progress_dtor(&ctx->progress TSRMLS_CC);

	efree(ctx);
	h->ctx = NULL;
}
static STATUS php_http_curl_request_reset(php_http_request_t *h)
{
	CURL *ch = ((php_http_curl_request_t *) h->ctx)->handle;
	php_http_curl_request_storage_t *st;

	if ((st = get_storage(ch))) {
		if (st->url) {
			pefree(st->url, 1);
			st->url = NULL;
		}
		if (st->cookiestore) {
			pefree(st->cookiestore, 1);
			st->cookiestore = NULL;
		}
		st->errorbuffer[0] = '\0';
	}

	curl_easy_setopt(ch, CURLOPT_URL, NULL);
#if PHP_HTTP_CURL_VERSION(7,19,4)
	curl_easy_setopt(ch, CURLOPT_NOPROXY, NULL);
#endif
	curl_easy_setopt(ch, CURLOPT_PROXY, NULL);
	curl_easy_setopt(ch, CURLOPT_PROXYPORT, 0L);
	curl_easy_setopt(ch, CURLOPT_PROXYTYPE, 0L);
	/* libcurl < 7.19.6 does not clear auth info with USERPWD set to NULL */
#if PHP_HTTP_CURL_VERSION(7,19,1)
	curl_easy_setopt(ch, CURLOPT_PROXYUSERNAME, NULL);
	curl_easy_setopt(ch, CURLOPT_PROXYPASSWORD, NULL);
#endif
	curl_easy_setopt(ch, CURLOPT_PROXYAUTH, 0L);
	curl_easy_setopt(ch, CURLOPT_HTTPPROXYTUNNEL, 0L);
	curl_easy_setopt(ch, CURLOPT_DNS_CACHE_TIMEOUT, 60L);
	curl_easy_setopt(ch, CURLOPT_IPRESOLVE, 0);
#if PHP_HTTP_CURL_VERSION(7,21,3)
	curl_easy_setopt(ch, CURLOPT_RESOLVE, NULL);
#endif
#if PHP_HTTP_CURL_VERSION(7,24,0)
	curl_easy_setopt(ch, CURLOPT_DNS_SERVERS, NULL);
#endif
	curl_easy_setopt(ch, CURLOPT_LOW_SPEED_LIMIT, 0L);
	curl_easy_setopt(ch, CURLOPT_LOW_SPEED_TIME, 0L);
	/* LFS weirdance
	curl_easy_setopt(ch, CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t) 0);
	curl_easy_setopt(ch, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t) 0);
	*/
	/* crashes
	curl_easy_setopt(ch, CURLOPT_MAXCONNECTS, 5L); */
	curl_easy_setopt(ch, CURLOPT_FRESH_CONNECT, 0L);
	curl_easy_setopt(ch, CURLOPT_FORBID_REUSE, 0L);
	curl_easy_setopt(ch, CURLOPT_INTERFACE, NULL);
	curl_easy_setopt(ch, CURLOPT_PORT, 0L);
#if PHP_HTTP_CURL_VERSION(7,19,0)
	curl_easy_setopt(ch, CURLOPT_ADDRESS_SCOPE, 0L);
#endif
	curl_easy_setopt(ch, CURLOPT_LOCALPORT, 0L);
	curl_easy_setopt(ch, CURLOPT_LOCALPORTRANGE, 0L);
	/* libcurl < 7.19.6 does not clear auth info with USERPWD set to NULL */
#if PHP_HTTP_CURL_VERSION(7,19,1)
	curl_easy_setopt(ch, CURLOPT_USERNAME, NULL);
	curl_easy_setopt(ch, CURLOPT_PASSWORD, NULL);
#endif
	curl_easy_setopt(ch, CURLOPT_HTTPAUTH, 0L);
	curl_easy_setopt(ch, CURLOPT_ENCODING, NULL);
	/* we do this ourself anyway */
	curl_easy_setopt(ch, CURLOPT_HTTP_CONTENT_DECODING, 0L);
	curl_easy_setopt(ch, CURLOPT_HTTP_TRANSFER_DECODING, 0L);
	curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 0L);
#if PHP_HTTP_CURL_VERSION(7,19,1)
	curl_easy_setopt(ch, CURLOPT_POSTREDIR, 0L);
#else
	curl_easy_setopt(ch, CURLOPT_POST301, 0L);
#endif
	curl_easy_setopt(ch, CURLOPT_UNRESTRICTED_AUTH, 0L);
	curl_easy_setopt(ch, CURLOPT_REFERER, NULL);
	curl_easy_setopt(ch, CURLOPT_USERAGENT, "PECL::HTTP/" PHP_HTTP_EXT_VERSION " (PHP/" PHP_VERSION ")");
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(ch, CURLOPT_COOKIE, NULL);
	curl_easy_setopt(ch, CURLOPT_COOKIESESSION, 0L);
	/* these options would enable curl's cookie engine by default which we don't want
	curl_easy_setopt(ch, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(ch, CURLOPT_COOKIEJAR, NULL); */
	curl_easy_setopt(ch, CURLOPT_COOKIELIST, NULL);
	curl_easy_setopt(ch, CURLOPT_RANGE, NULL);
	curl_easy_setopt(ch, CURLOPT_RESUME_FROM, 0L);
	curl_easy_setopt(ch, CURLOPT_MAXFILESIZE, 0L);
	curl_easy_setopt(ch, CURLOPT_TIMECONDITION, 0L);
	curl_easy_setopt(ch, CURLOPT_TIMEVALUE, 0L);
	curl_easy_setopt(ch, CURLOPT_TIMEOUT, 0L);
	curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, 3);
	curl_easy_setopt(ch, CURLOPT_SSLCERT, NULL);
	curl_easy_setopt(ch, CURLOPT_SSLCERTTYPE, NULL);
	curl_easy_setopt(ch, CURLOPT_SSLCERTPASSWD, NULL);
	curl_easy_setopt(ch, CURLOPT_SSLKEY, NULL);
	curl_easy_setopt(ch, CURLOPT_SSLKEYTYPE, NULL);
	curl_easy_setopt(ch, CURLOPT_SSLKEYPASSWD, NULL);
	curl_easy_setopt(ch, CURLOPT_SSLENGINE, NULL);
	curl_easy_setopt(ch, CURLOPT_SSLVERSION, 0L);
	curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(ch, CURLOPT_SSL_CIPHER_LIST, NULL);
#if PHP_HTTP_CURL_VERSION(7,19,0)
	curl_easy_setopt(ch, CURLOPT_ISSUERCERT, NULL);
#if defined(PHP_HTTP_HAVE_OPENSSL)
	curl_easy_setopt(ch, CURLOPT_CRLFILE, NULL);
#endif
#endif
#if PHP_HTTP_CURL_VERSION(7,19,1) && defined(PHP_HTTP_HAVE_OPENSSL)
	curl_easy_setopt(ch, CURLOPT_CERTINFO, NULL);
#endif
#ifdef PHP_HTTP_CURL_CAINFO
	curl_easy_setopt(ch, CURLOPT_CAINFO, PHP_HTTP_CURL_CAINFO);
#else
	curl_easy_setopt(ch, CURLOPT_CAINFO, NULL);
#endif
	curl_easy_setopt(ch, CURLOPT_CAPATH, NULL);
	curl_easy_setopt(ch, CURLOPT_RANDOM_FILE, NULL);
	curl_easy_setopt(ch, CURLOPT_EGDSOCKET, NULL);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, NULL);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, 0L);
	curl_easy_setopt(ch, CURLOPT_HTTPPOST, NULL);
	curl_easy_setopt(ch, CURLOPT_IOCTLDATA, NULL);
	curl_easy_setopt(ch, CURLOPT_READDATA, NULL);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE, 0L);
	curl_easy_setopt(ch, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
	curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(ch, CURLOPT_NOBODY, 0L);
	curl_easy_setopt(ch, CURLOPT_POST, 0L);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(ch, CURLOPT_HTTPGET, 1L);

	return SUCCESS;
}

static STATUS php_http_curl_request_exec(php_http_request_t *h, const char *meth, const char *url, php_http_message_body_t *body)
{
	uint tries = 0;
	CURLcode result;
	php_http_curl_request_t *curl = h->ctx;
	php_http_curl_request_storage_t *storage = get_storage(curl->handle);
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (SUCCESS != php_http_curl_request_prepare(h, meth, url, body)) {
		return FAILURE;
	}

retry:
	if (CURLE_OK != (result = curl_easy_perform(curl->handle))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(result), storage->errorbuffer, storage->url);

		if (EG(exception)) {
			add_property_long(EG(exception), "curlCode", result);
		}

		if (curl->options.retry.count > tries++) {
			switch (result) {
				case CURLE_COULDNT_RESOLVE_PROXY:
				case CURLE_COULDNT_RESOLVE_HOST:
				case CURLE_COULDNT_CONNECT:
				case CURLE_WRITE_ERROR:
				case CURLE_READ_ERROR:
				case CURLE_OPERATION_TIMEDOUT:
				case CURLE_SSL_CONNECT_ERROR:
				case CURLE_GOT_NOTHING:
				case CURLE_SSL_ENGINE_SETFAILED:
				case CURLE_SEND_ERROR:
				case CURLE_RECV_ERROR:
				case CURLE_SSL_ENGINE_INITFAILED:
				case CURLE_LOGIN_DENIED:
					if (curl->options.retry.delay >= PHP_HTTP_DIFFSEC) {
						php_http_sleep(curl->options.retry.delay);
					}
					goto retry;
				default:
					break;
			}
		} else {
			return FAILURE;
		}
	}

	return SUCCESS;
}

static STATUS php_http_curl_request_setopt(php_http_request_t *h, php_http_request_setopt_opt_t opt, void *arg)
{
	php_http_curl_request_t *curl = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	switch (opt) {
		case PHP_HTTP_REQUEST_OPT_SETTINGS:
			return set_options(h, arg);
			break;

		case PHP_HTTP_REQUEST_OPT_PROGRESS_CALLBACK:
			if (curl->progress.in_cb) {
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Cannot change progress callback while executing it");
				return FAILURE;
			}
			if (curl->progress.callback) {
				php_http_request_progress_dtor(&curl->progress TSRMLS_CC);
			}
			curl->progress.callback = arg;
			break;

		case PHP_HTTP_REQUEST_OPT_COOKIES_ENABLE:
			/* are cookies already enabled anyway? */
			if (!get_storage(curl->handle)->cookiestore) {
				if (CURLE_OK != curl_easy_setopt(curl->handle, CURLOPT_COOKIEFILE, "")) {
					return FAILURE;
				}
			}
			break;

		case PHP_HTTP_REQUEST_OPT_COOKIES_RESET:
			if (CURLE_OK != curl_easy_setopt(curl->handle, CURLOPT_COOKIELIST, "ALL")) {
				return FAILURE;
			}
			break;

		case PHP_HTTP_REQUEST_OPT_COOKIES_RESET_SESSION:
			if (CURLE_OK != curl_easy_setopt(curl->handle, CURLOPT_COOKIELIST, "SESS")) {
				return FAILURE;
			}
			break;

		case PHP_HTTP_REQUEST_OPT_COOKIES_FLUSH:
			if (CURLE_OK != curl_easy_setopt(curl->handle, CURLOPT_COOKIELIST, "FLUSH")) {
				return FAILURE;
			}
			break;

		default:
			return FAILURE;
	}

	return SUCCESS;
}

static STATUS php_http_curl_request_getopt(php_http_request_t *h, php_http_request_getopt_opt_t opt, void *arg)
{
	php_http_curl_request_t *curl = h->ctx;

	switch (opt) {
		case PHP_HTTP_REQUEST_OPT_PROGRESS_INFO:
			*((php_http_request_progress_t **) arg) = &curl->progress;
			break;

		case PHP_HTTP_REQUEST_OPT_TRANSFER_INFO:
			get_info(curl->handle, arg);
			break;

		default:
			return FAILURE;
	}

	return SUCCESS;
}

static php_http_resource_factory_ops_t php_http_curl_resource_factory_ops = {
	php_http_curl_ctor,
	php_http_curl_copy,
	php_http_curl_dtor
};

static php_http_request_ops_t php_http_curl_request_ops = {
	&php_http_curl_resource_factory_ops,
	php_http_curl_request_init,
	php_http_curl_request_copy,
	php_http_curl_request_dtor,
	php_http_curl_request_reset,
	php_http_curl_request_exec,
	php_http_curl_request_setopt,
	php_http_curl_request_getopt
};

PHP_HTTP_API php_http_request_ops_t *php_http_curl_get_request_ops(void)
{
	return &php_http_curl_request_ops;
}

#if defined(ZTS) && defined(PHP_HTTP_HAVE_SSL)
#	ifdef PHP_WIN32
#		define PHP_HTTP_NEED_OPENSSL_TSL
#		include <openssl/crypto.h>
#	else /* !PHP_WIN32 */
#		if defined(PHP_HTTP_HAVE_OPENSSL)
#			define PHP_HTTP_NEED_OPENSSL_TSL
#			include <openssl/crypto.h>
#		elif defined(PHP_HTTP_HAVE_GNUTLS)
#			define PHP_HTTP_NEED_GNUTLS_TSL
#			include <gcrypt.h>
#		else
#			warning \
				"libcurl was compiled with SSL support, but configure could not determine which" \
				"library was used; thus no SSL crypto locking callbacks will be set, which may " \
				"cause random crashes on SSL requests"
#		endif /* PHP_HTTP_HAVE_OPENSSL || PHP_HTTP_HAVE_GNUTLS */
#	endif /* PHP_WIN32 */
#endif /* ZTS && PHP_HTTP_HAVE_SSL */


#ifdef PHP_HTTP_NEED_OPENSSL_TSL
static MUTEX_T *php_http_openssl_tsl = NULL;

static void php_http_openssl_thread_lock(int mode, int n, const char * file, int line)
{
	if (mode & CRYPTO_LOCK) {
		tsrm_mutex_lock(php_http_openssl_tsl[n]);
	} else {
		tsrm_mutex_unlock(php_http_openssl_tsl[n]);
	}
}

static ulong php_http_openssl_thread_id(void)
{
	return (ulong) tsrm_thread_id();
}
#endif
#ifdef PHP_HTTP_NEED_GNUTLS_TSL
static int php_http_gnutls_mutex_create(void **m)
{
	if (*((MUTEX_T *) m) = tsrm_mutex_alloc()) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

static int php_http_gnutls_mutex_destroy(void **m)
{
	tsrm_mutex_free(*((MUTEX_T *) m));
	return SUCCESS;
}

static int php_http_gnutls_mutex_lock(void **m)
{
	return tsrm_mutex_lock(*((MUTEX_T *) m));
}

static int php_http_gnutls_mutex_unlock(void **m)
{
	return tsrm_mutex_unlock(*((MUTEX_T *) m));
}

static struct gcry_thread_cbs php_http_gnutls_tsl = {
	GCRY_THREAD_OPTION_USER,
	NULL,
	php_http_gnutls_mutex_create,
	php_http_gnutls_mutex_destroy,
	php_http_gnutls_mutex_lock,
	php_http_gnutls_mutex_unlock
};
#endif

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpCURL, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpCURL, method, 0)
#define PHP_HTTP_CURL_ME(method, visibility)	PHP_ME(HttpCURL, method, PHP_HTTP_ARGS(HttpCURL, method), visibility)
#define PHP_HTTP_CURL_ALIAS(method, func)	PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpCURL, method))
#define PHP_HTTP_CURL_MALIAS(me, al, vis)	ZEND_FENTRY(me, ZEND_MN(HttpCURL_##al), PHP_HTTP_ARGS(HttpCURL, al), vis)

PHP_HTTP_EMPTY_ARGS(__construct);

zend_class_entry *php_http_curl_class_entry;
zend_function_entry php_http_curl_method_entry[] = {
	PHP_HTTP_CURL_ME(__construct, ZEND_ACC_PRIVATE|ZEND_ACC_CTOR)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpCURL, __construct) {
}

PHP_MINIT_FUNCTION(http_curl)
{
	php_http_request_factory_driver_t driver = {
		&php_http_curl_request_ops,
		&php_http_curl_request_pool_ops,
		&php_http_curl_request_datashare_ops
	};

#ifdef PHP_HTTP_NEED_OPENSSL_TSL
	/* mod_ssl, libpq or ext/curl might already have set thread lock callbacks */
	if (!CRYPTO_get_id_callback()) {
		int i, c = CRYPTO_num_locks();

		php_http_openssl_tsl = malloc(c * sizeof(MUTEX_T));

		for (i = 0; i < c; ++i) {
			php_http_openssl_tsl[i] = tsrm_mutex_alloc();
		}

		CRYPTO_set_id_callback(php_http_openssl_thread_id);
		CRYPTO_set_locking_callback(php_http_openssl_thread_lock);
	}
#endif
#ifdef PHP_HTTP_NEED_GNUTLS_TSL
	gcry_control(GCRYCTL_SET_THREAD_CBS, &php_http_gnutls_tsl);
#endif

	if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL)) {
		return FAILURE;
	}

	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_request_datashare.curl"), &php_http_curlsh_resource_factory_ops, NULL, NULL)) {
		return FAILURE;
	}

	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_request_pool.curl"), &php_http_curlm_resource_factory_ops, NULL, NULL)) {
		return FAILURE;
	}

	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_request.curl"), &php_http_curl_resource_factory_ops, NULL, NULL)) {
		return FAILURE;
	}

	if (SUCCESS != php_http_request_factory_add_driver(ZEND_STRL("curl"), &driver)) {
		return FAILURE;
	}

	PHP_HTTP_REGISTER_CLASS(http, CURL, http_curl, php_http_curl_class_entry, 0);

	/*
	* HTTP Protocol Version Constants
	*/
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("HTTP_VERSION_1_0"), CURL_HTTP_VERSION_1_0 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("HTTP_VERSION_1_1"), CURL_HTTP_VERSION_1_1 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("HTTP_VERSION_NONE"), CURL_HTTP_VERSION_NONE TSRMLS_CC); /* to be removed */
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("HTTP_VERSION_ANY"), CURL_HTTP_VERSION_NONE TSRMLS_CC);

	/*
	* SSL Version Constants
	*/
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("SSL_VERSION_TLSv1"), CURL_SSLVERSION_TLSv1 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("SSL_VERSION_SSLv2"), CURL_SSLVERSION_SSLv2 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("SSL_VERSION_SSLv3"), CURL_SSLVERSION_SSLv3 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("SSL_VERSION_ANY"), CURL_SSLVERSION_DEFAULT TSRMLS_CC);

	/*
	* DNS IPvX resolving
	*/
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("IPRESOLVE_V4"), CURL_IPRESOLVE_V4 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("IPRESOLVE_V6"), CURL_IPRESOLVE_V6 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("IPRESOLVE_ANY"), CURL_IPRESOLVE_WHATEVER TSRMLS_CC);

	/*
	* Auth Constants
	*/
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("AUTH_BASIC"), CURLAUTH_BASIC TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("AUTH_DIGEST"), CURLAUTH_DIGEST TSRMLS_CC);
#if PHP_HTTP_CURL_VERSION(7,19,3)
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("AUTH_DIGEST_IE"), CURLAUTH_DIGEST_IE TSRMLS_CC);
#endif
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("AUTH_NTLM"), CURLAUTH_NTLM TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("AUTH_GSSNEG"), CURLAUTH_GSSNEGOTIATE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("AUTH_ANY"), CURLAUTH_ANY TSRMLS_CC);

	/*
	* Proxy Type Constants
	*/
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("PROXY_SOCKS4"), CURLPROXY_SOCKS4 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("PROXY_SOCKS4A"), CURLPROXY_SOCKS5 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("PROXY_SOCKS5_HOSTNAME"), CURLPROXY_SOCKS5 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("PROXY_SOCKS5"), CURLPROXY_SOCKS5 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("PROXY_HTTP"), CURLPROXY_HTTP TSRMLS_CC);
#	if PHP_HTTP_CURL_VERSION(7,19,4)
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("PROXY_HTTP_1_0"), CURLPROXY_HTTP_1_0 TSRMLS_CC);
#	endif

	/*
	* Post Redirection Constants
	*/
#if PHP_HTTP_CURL_VERSION(7,19,1)
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("POSTREDIR_301"), CURL_REDIR_POST_301 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("POSTREDIR_302"), CURL_REDIR_POST_302 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_curl_class_entry, ZEND_STRL("POSTREDIR_ALL"), CURL_REDIR_POST_ALL TSRMLS_CC);
#endif

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_curl)
{
	curl_global_cleanup();
#ifdef PHP_HTTP_NEED_OPENSSL_TSL
	if (php_http_openssl_tsl) {
		int i, c = CRYPTO_num_locks();

		CRYPTO_set_id_callback(NULL);
		CRYPTO_set_locking_callback(NULL);

		for (i = 0; i < c; ++i) {
			tsrm_mutex_free(php_http_openssl_tsl[i]);
		}

		free(php_http_openssl_tsl);
		php_http_openssl_tsl = NULL;
	}
#endif
	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_curl)
{
#if PHP_HTTP_HAVE_EVENT
	if (!PHP_HTTP_G->curl.event_base && !(PHP_HTTP_G->curl.event_base = event_init())) {
		return FAILURE;
	}
#endif

	return SUCCESS;
}

#endif /* PHP_HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

