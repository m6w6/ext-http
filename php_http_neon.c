
#include "php_http.h"
#include "php_http_request.h"

#include <ext/date/php_date.h>

#include <neon/ne_auth.h>
#include <neon/ne_compress.h>
#include <neon/ne_session.h>
#include <neon/ne_request.h>
#include <neon/ne_redirect.h>

typedef struct php_http_neon_auth {
	long type;
	char *user;
	char *pass;
} php_http_neon_auth_t;

typedef struct php_http_neon_request {
	php_http_message_body_t *body;

	struct {
		HashTable cache;

		php_http_buffer_t headers;
		char *useragent;
		char *referer;
		char *url;
		short port;

		struct {
			int type;
			short port;
			char *host;
		} proxy;

		struct {
			php_http_neon_auth_t proxy;
			php_http_neon_auth_t http;
		} auth;

		struct {
			unsigned noverify:1;
			ne_ssl_client_cert *clicert;
			ne_ssl_certificate *trucert;
		} ssl;

		long redirects;
		char *cookiestore;
		ne_inet_addr *interface;
		long maxfilesize;

		struct {
			unsigned count;
			double delay;
		} retry;

		struct {
			long connect;
			long read;
		} timeout;
	} options;

	php_http_request_progress_t progress;

} php_http_neon_request_t;

/* callbacks */

static ssize_t php_http_neon_read_callback(void *ctx, char *buf, size_t len)
{
	php_http_request_t *h = ctx;
	php_http_neon_request_t *neon = h->ctx;
	php_http_message_body_t *body = neon->body;

	if (body) {
		TSRMLS_FETCH_FROM_CTX(body->ts);

		if (buf) {
			size_t read = php_stream_read(php_http_message_body_stream(body), buf, len);

			php_http_buffer_append(h->buffer, buf, read);
			php_http_message_parser_parse(h->parser, h->buffer, 0, &h->message);
			return read;
		} else {
			return php_stream_rewind(php_http_message_body_stream(body));
		}
	}
	return 0;
}

static void php_http_neon_pre_send_callback(ne_request *req, void *ctx, ne_buffer *header)
{
	php_http_request_t *h = ctx;
	php_http_neon_request_t *neon = h->ctx;

	ne_buffer_append(header, neon->options.headers.data, neon->options.headers.used);

	php_http_buffer_append(h->buffer, header->data, header->used - 1 /* ne_buffer counts \0 */);
	php_http_buffer_appends(h->buffer, PHP_HTTP_CRLF);
	php_http_message_parser_parse(h->parser, h->buffer, 0, &h->message);
}

static void php_http_neon_post_headers_callback(ne_request *req, void *ctx, const ne_status *status)
{
	php_http_request_t *h = ctx;
	HashTable *hdrs = &h->message->hdrs;
	php_http_info_t i;
	void *iter = NULL;
	zval tmp;
	const char *name, *value;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	php_http_info_init(&i TSRMLS_CC);
	i.type = PHP_HTTP_RESPONSE;
	php_http_version_init(&i.http.version, status->major_version, status->minor_version TSRMLS_CC);
	i.http.info.response.code = status->code;
	i.http.info.response.status = estrdup(status->reason_phrase);
	php_http_message_info_callback(&h->message, &hdrs, &i TSRMLS_CC);
	php_http_info_dtor(&i);

	INIT_PZVAL_ARRAY(&tmp, hdrs);
	while ((iter = ne_response_header_iterate(req, iter, &name, &value))) {
		char *key = php_http_pretty_key(estrdup(name), strlen(name), 1, 1);
		add_assoc_string(&tmp, key, estrdup(value), 0);
		efree(key);
	}
	php_http_message_parser_state_push(h->parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE);
}

static int php_http_neon_ssl_verify_callback(void *ctx, int failures, const ne_ssl_certificate *cert)
{
	php_http_request_t *h = ctx;
	php_http_neon_request_t *neon = h->ctx;

	if (neon->options.ssl.noverify) {
		return 0;
	}
	return failures;
}

static void php_http_neon_progress_callback(void *ctx, ne_session_status status, const ne_session_status_info *info)
{
	php_http_request_t *h = ctx;
	php_http_neon_request_t *neon = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	switch (status) {
		case ne_status_lookup:
			neon->progress.state.info = "resolve";
			break;
		case ne_status_connecting:
			neon->progress.state.info = "connect";
			break;
		case ne_status_connected:
			neon->progress.state.info = "connected";
			break;
		case ne_status_sending:
			neon->progress.state.info = "send";
			neon->progress.state.ul.total = info->sr.total;
			neon->progress.state.ul.now = info->sr.progress;
			break;
		case ne_status_recving:
			neon->progress.state.info = "receive";
			neon->progress.state.dl.total = info->sr.total;
			neon->progress.state.dl.now = info->sr.progress;
			break;
		case ne_status_disconnected:
			neon->progress.state.info = "disconnected";
			break;
	}

	php_http_request_progress_notify(&neon->progress TSRMLS_CC);
}

/* helpers */

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
	php_http_neon_request_t *neon = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	/* proxy */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("proxyhost"), IS_STRING))) {
		neon->options.proxy.host = Z_STRVAL_P(zoption);

		/* user:pass */
		if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("proxyauth"), IS_STRING)) && Z_STRLEN_P(zoption)) {
			char *colon = strchr(Z_STRVAL_P(zoption), ':');

			if (colon) {
				STR_SET(neon->options.auth.proxy.user, estrndup(Z_STRVAL_P(zoption), colon - Z_STRVAL_P(zoption)));
				STR_SET(neon->options.auth.proxy.pass, estrdup(colon + 1));
			} else {
				STR_SET(neon->options.auth.proxy.user, estrdup(Z_STRVAL_P(zoption)));
			}
		}

		if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("proxyauthtype"), IS_LONG))) {
			neon->options.auth.proxy.type = Z_LVAL_P(zoption);
		} else {
			neon->options.auth.proxy.type = NE_AUTH_ALL;
		}


		/* port */
		if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("proxyport"), IS_LONG))) {
			neon->options.proxy.port = Z_LVAL_P(zoption);
		} else {
			neon->options.proxy.port = 0;
		}

		/* type */
		if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("proxytype"), IS_LONG))) {
			neon->options.proxy.type = Z_LVAL_P(zoption);
		} else {
			neon->options.proxy.type = -1;
		}
	}

	/* outgoing interface */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("interface"), IS_STRING))) {
		if (!(neon->options.interface = ne_iaddr_parse(Z_STRVAL_P(zoption), ne_iaddr_ipv4))) {
			neon->options.interface = ne_iaddr_parse(Z_STRVAL_P(zoption), ne_iaddr_ipv6);
		}
	}

	/* another port */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("port"), IS_LONG))) {
		neon->options.port = Z_LVAL_P(zoption);
	}

	/* auth */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("httpauth"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		char *colon = strchr(Z_STRVAL_P(zoption), ':');

		if (colon) {
			STR_SET(neon->options.auth.http.user, estrndup(Z_STRVAL_P(zoption), colon - Z_STRVAL_P(zoption)));
			STR_SET(neon->options.auth.http.pass, estrdup(colon + 1));
		} else {
			STR_SET(neon->options.auth.http.user, estrdup(Z_STRVAL_P(zoption)));
		}
	}
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("httpauthtype"), IS_LONG))) {
		neon->options.auth.http.type = Z_LVAL_P(zoption);
	} else {
		neon->options.auth.http.type = NE_AUTH_ALL;
	}

	/* redirects, defaults to 0 */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("redirect"), IS_LONG))) {
		neon->options.redirects = Z_LVAL_P(zoption);
	} else {
		neon->options.redirects = 0;
	}

	/* retries, defaults to 0 */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("retrycount"), IS_LONG))) {
		neon->options.retry.count = Z_LVAL_P(zoption);
		if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("retrydelay"), IS_DOUBLE))) {
			neon->options.retry.delay = Z_DVAL_P(zoption);
		} else {
			neon->options.retry.delay = 0;
		}
	} else {
		neon->options.retry.count = 0;
	}

	/* referer */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("referer"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		neon->options.referer = Z_STRVAL_P(zoption);
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("useragent"), IS_STRING))) {
		/* allow to send no user agent, not even default one */
		if (Z_STRLEN_P(zoption)) {
			neon->options.useragent = Z_STRVAL_P(zoption);
		} else {
			neon->options.useragent = NULL;
		}
	}

	/* resume */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("resume"), IS_LONG)) && (Z_LVAL_P(zoption) > 0)) {
		php_http_buffer_appendf(&neon->options.headers, "Range: bytes=%ld-" PHP_HTTP_CRLF, Z_LVAL_P(zoption));
		range_req = 1;
	}
	/* or range of kind array(array(0,499), array(100,1499)) */
	else if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("range"), IS_ARRAY)) && zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
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
			/* ignore last comma */
			int used = rs.used > INT_MAX ? INT_MAX : rs.used;
			php_http_buffer_appendf(&neon->options.headers, "Range: bytes=%.*s" PHP_HTTP_CRLF, used - 1, rs.data);
			range_req = 1;
		}
		php_http_buffer_dtor(&rs);
	}

	/* additional headers, array('name' => 'value') */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("headers"), IS_ARRAY))) {
		php_http_array_hashkey_t header_key = php_http_array_hashkey_init(0);
		zval **header_val;
		HashPosition pos;

		FOREACH_KEYVAL(pos, zoption, header_key, header_val) {
			if (header_key.type == HASH_KEY_IS_STRING) {
				zval *header_cpy = php_http_ztyp(IS_STRING, *header_val);

				if (!strcasecmp(header_key.str, "range")) {
					range_req = 1;
				}
				php_http_buffer_appendf(&neon->options.headers, "%s: %s" PHP_HTTP_CRLF, header_key.str, Z_STRVAL_P(header_cpy));
				zval_ptr_dtor(&header_cpy);
			}
		}
	}
	/* etag */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("etag"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		php_http_buffer_appends(&neon->options.headers, "If-");
		if (range_req) {
			php_http_buffer_appends(&neon->options.headers, "None-");
		}
		php_http_buffer_appends(&neon->options.headers, "Match: ");

		if ((Z_STRVAL_P(zoption)[0] == '"') && (Z_STRVAL_P(zoption)[Z_STRLEN_P(zoption)-1] == '"')) {
			php_http_buffer_appendl(&neon->options.headers, Z_STRVAL_P(zoption));
		} else {
			php_http_buffer_appendf(&neon->options.headers, "\"%s\"" PHP_HTTP_CRLF, Z_STRVAL_P(zoption));
		}
	}
	/* compression */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("compress"), IS_BOOL)) && Z_LVAL_P(zoption)) {
		php_http_buffer_appends(&neon->options.headers, "Accept-Encoding: gzip;q=1.0,deflate;q=0.5" PHP_HTTP_CRLF);
	}

	/* lastmodified */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("lastmodified"), IS_LONG))) {
		if (Z_LVAL_P(zoption)) {
			time_t time = Z_LVAL_P(zoption) > 0 ? Z_LVAL_P(zoption) : PHP_HTTP_G->env.request.time + Z_LVAL_P(zoption);
			char *date = php_format_date(ZEND_STRS(PHP_HTTP_DATE_FORMAT), time, 0 TSRMLS_CC);

			php_http_buffer_appends(&neon->options.headers, "If-");
			if (range_req) {
				php_http_buffer_appends(&neon->options.headers, "Un");
			}
			php_http_buffer_appendf(&neon->options.headers, "Modified-Since: %s" PHP_HTTP_CRLF, date);
			efree(date);
		}
	}

	/* cookies, array('name' => 'value') */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("cookies"), IS_ARRAY))) {
		php_http_buffer_t cookies;

		php_http_buffer_init(&cookies);
		if (zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
			zval *urlenc_cookies = NULL;

			php_http_buffer_appends(&neon->options.headers, "Cookie: ");

			/* check whether cookies should not be urlencoded; default is to urlencode them */
			if ((!(urlenc_cookies = get_option(&neon->options.cache, options, ZEND_STRS("encodecookies"), IS_BOOL))) || Z_BVAL_P(urlenc_cookies)) {
				php_http_url_encode_hash_recursive(HASH_OF(zoption), &neon->options.headers, "; ", lenof("; "), NULL, 0 TSRMLS_CC);
			} else {
				HashPosition pos;
				php_http_array_hashkey_t cookie_key = php_http_array_hashkey_init(0);
				zval **cookie_val;

				FOREACH_KEYVAL(pos, zoption, cookie_key, cookie_val) {
					if (cookie_key.type == HASH_KEY_IS_STRING) {
						zval *val = php_http_ztyp(IS_STRING, *cookie_val);
						php_http_buffer_appendf(&neon->options.headers, "%s=%s; ", cookie_key.str, Z_STRVAL_P(val));
						zval_ptr_dtor(&val);
					}
				}
			}
		}
		neon->options.headers.used -= lenof("; ");
		php_http_buffer_appends(&neon->options.headers, PHP_HTTP_CRLF);
	}
	/* cookiestore, read initial cookies from that file and store cookies back into that file */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("cookiestore"), IS_STRING))) {
		neon->options.cookiestore = Z_STRVAL_P(zoption);
	}

	/* maxfilesize */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("maxfilesize"), IS_LONG))) {
		neon->options.maxfilesize = Z_LVAL_P(zoption);
	}

	/* READ timeout */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("timeout"), IS_DOUBLE))) {
		neon->options.timeout.read = Z_DVAL_P(zoption) > 0 && Z_DVAL_P(zoption) < 1 ? 1 : round(Z_DVAL_P(zoption));
	}
	/* connecttimeout, defaults to 0 */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("connecttimeout"), IS_DOUBLE))) {
		neon->options.timeout.connect = Z_DVAL_P(zoption) > 0 && Z_DVAL_P(zoption) < 1 ? 1 : round(Z_DVAL_P(zoption));
	}

	/* ssl */
	if ((zoption = get_option(&neon->options.cache, options, ZEND_STRS("ssl"), IS_ARRAY))) {
		zval **zssl;

		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(zoption), ZEND_STRS("verifypeer"), (void *) &zssl)) {
			if (!i_zend_is_true(*zssl)) {
				neon->options.ssl.noverify = 1;
			}
		}
		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(zoption), ZEND_STRS("key"), (void *) &zssl)) {
			zval *cpy = php_http_ztyp(IS_STRING, *zssl);
			ne_ssl_client_cert *cc = ne_ssl_clicert_read(Z_STRVAL_P(cpy));

			if (cc) {
				if (ne_ssl_clicert_encrypted(cc)) {
					if (SUCCESS == zend_hash_find(Z_ARRVAL_P(zoption), ZEND_STRS("keypasswd"), (void *) &zssl)) {
						zval *cpy = php_http_ztyp(IS_STRING, *zssl);

						if (NE_OK == ne_ssl_clicert_decrypt(cc, Z_STRVAL_P(cpy))) {
							neon->options.ssl.clicert = cc;
						}
						zval_ptr_dtor(&cpy);
					}
				}
			}

			if (cc && !neon->options.ssl.clicert) {
				ne_ssl_clicert_free(cc);
			}

			zval_ptr_dtor(&cpy);
		}
		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(zoption), ZEND_STRS("cert"), (void *) &zssl)) {
			zval *cpy = php_http_ztyp(IS_STRING, *zssl);
			ne_ssl_certificate *tc = ne_ssl_cert_read(Z_STRVAL_P(cpy));

			if (tc) {
				neon->options.ssl.trucert = tc;
			}
			zval_ptr_dtor(&cpy);
		}
	}

	return SUCCESS;
}

/* request handler ops */

static STATUS php_http_neon_request_reset(php_http_request_t *h);

static php_http_request_t *php_http_neon_request_init(php_http_request_t *h, void *dummy)
{
	php_http_neon_request_t *ctx;

	ctx = ecalloc(1, sizeof(*ctx));
	php_http_buffer_init(&ctx->options.headers);
	zend_hash_init(&ctx->options.cache, 0, NULL, ZVAL_PTR_DTOR, 0);
	h->ctx = ctx;

	return h;
}

static php_http_request_t *php_http_neon_request_copy(php_http_request_t *from, php_http_request_t *to)
{
	TSRMLS_FETCH_FROM_CTX(from->ts);

	if (to) {
		return php_http_neon_request_init(to, NULL);
	} else {
		return php_http_request_init(NULL, from->ops, from->rf, NULL TSRMLS_CC);
	}
}

static void php_http_neon_request_dtor(php_http_request_t *h)
{
	php_http_neon_request_t *ctx = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	php_http_neon_request_reset(h);
	php_http_buffer_dtor(&ctx->options.headers);
	zend_hash_destroy(&ctx->options.cache);

	php_http_request_progress_dtor(&ctx->progress TSRMLS_CC);

	efree(ctx);
	h->ctx = NULL;
}

static STATUS php_http_neon_request_reset(php_http_request_t *h)
{
	php_http_neon_request_t *neon = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	php_http_buffer_reset(&neon->options.headers);
	STR_SET(neon->options.useragent, NULL);
	STR_SET(neon->options.url, NULL);
	neon->options.port = 0;
	neon->options.proxy.type = -1;
	neon->options.proxy.port = 0;
	STR_SET(neon->options.proxy.host, NULL);
	neon->options.auth.proxy.type = 0;
	STR_SET(neon->options.auth.proxy.user, NULL);
	STR_SET(neon->options.auth.proxy.pass, NULL);
	neon->options.auth.http.type = 0;
	STR_SET(neon->options.auth.http.user, NULL);
	STR_SET(neon->options.auth.http.pass, NULL);
	neon->options.ssl.noverify = 0;
	if (neon->options.ssl.clicert) {
		ne_ssl_clicert_free(neon->options.ssl.clicert);
		neon->options.ssl.clicert = NULL;
	}
	if (neon->options.ssl.trucert) {
		ne_ssl_cert_free(neon->options.ssl.trucert);
		neon->options.ssl.trucert = NULL;
	}
	neon->options.redirects = 0;
	STR_SET(neon->options.cookiestore, NULL);
	if (neon->options.interface) {
		ne_iaddr_free(neon->options.interface);
		neon->options.interface = NULL;
	}
	neon->options.maxfilesize = 0;
	neon->options.retry.delay = 0;
	neon->options.retry.count = 0;
	neon->options.timeout.read = 0;
	neon->options.timeout.connect = 0;

	php_http_request_progress_dtor(&neon->progress TSRMLS_CC);

	return SUCCESS;
}

static STATUS php_http_neon_request_exec(php_http_request_t *h, php_http_request_method_t meth_id, const char *url, php_http_message_body_t *body)
{
	unsigned tries = 0;
	STATUS retval = SUCCESS;
	int result;
	php_url *purl;
	const char *meth;
	ne_session *session;
	ne_request *request;
	php_http_neon_request_t *neon = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (!(meth = php_http_request_method_name(meth_id TSRMLS_CC))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_METHOD, "Unsupported request method: %d (%s)", meth, url);
		return FAILURE;
	}

	if (!(purl = php_url_parse(url))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_URL, "Could not parse url %s", url);
		return FAILURE;
	}

	if (neon->options.port) {
		purl->port = neon->options.port;
	} else if (!purl->port) {
		purl->port = 80;
		if (strncasecmp(purl->scheme, "http", 4)) {
#ifdef HAVE_GETSERVBYNAME
			struct servent *se;

			if ((se = getservbyname(purl->scheme, "tcp")) && se->s_port) {
				purl->port = ntohs(se->s_port);
			}
#endif
		} else if (purl->scheme[4] == 's') {
			purl->port = 443;
		}
	}

	/* never returns NULL */
	session = ne_session_create(purl->scheme, purl->host, purl->port);
	if (neon->options.proxy.host) {
		switch (neon->options.proxy.type) {
			case NE_SOCK_SOCKSV4:
			case NE_SOCK_SOCKSV4A:
			case NE_SOCK_SOCKSV5:
				ne_session_socks_proxy(session, neon->options.proxy.type, neon->options.proxy.host, neon->options.proxy.port, neon->options.auth.proxy.user, neon->options.auth.proxy.pass);
				break;

			default:
				ne_session_proxy(session, neon->options.proxy.host, neon->options.proxy.port);
				break;
		}
	}
	if (neon->options.interface) {
		ne_set_localaddr(session, neon->options.interface);
	}
	if (neon->options.useragent) {
		ne_set_useragent(session, neon->options.useragent);
	}
	if (neon->options.timeout.read) {
		ne_set_read_timeout(session, neon->options.timeout.read);
	}
	if (neon->options.timeout.read) {
		ne_set_connect_timeout(session, neon->options.timeout.connect);
	}
	if (neon->options.redirects) {
		ne_redirect_register(session);
	}
	ne_hook_pre_send(session, php_http_neon_pre_send_callback, h);
	ne_hook_post_headers(session, php_http_neon_post_headers_callback, h);
	ne_set_notifier(session, php_http_neon_progress_callback, h);
	ne_ssl_set_verify(session, php_http_neon_ssl_verify_callback, h);
	if (neon->options.ssl.clicert) {
		ne_ssl_set_clicert(session, neon->options.ssl.clicert);
	}
	/* this crashes
	ne_ssl_trust_default_ca(session); */
	if (neon->options.ssl.trucert) {
		ne_ssl_trust_cert(session, neon->options.ssl.trucert);
	}

	request = ne_request_create(session, meth, purl->path /* . purl->query */);
	if (body) {
		/* RFC2616, section 4.3 (para. 4) states that »a message-body MUST NOT be included in a request if the
		 * specification of the request method (section 5.1.1) does not allow sending an entity-body in request.«
		 * Following the clause in section 5.1.1 (para. 2) that request methods »MUST be implemented with the
		 * same semantics as those specified in section 9« reveal that not any single defined HTTP/1.1 method
		 * does not allow a request body.
		 */
		switch (meth_id) {
			default:
				neon->body = body;
				ne_set_request_body_provider(request, php_http_message_body_size(body), php_http_neon_read_callback, h);
				break;
		}
	}

retry:
	switch (result = ne_begin_request(request)) {
		case NE_OK: {
			ssize_t len;
			char *buf = emalloc(0x1000);

			while (0 < (len = ne_read_response_block(request, buf, 0x1000))) {
				php_http_buffer_append(h->buffer, buf, len);
				php_http_message_parser_parse(h->parser, h->buffer, PHP_HTTP_MESSAGE_PARSER_DUMB_BODIES, &h->message);
				// php_http_message_body_append(&h->message->body, buf, len);
			}

			efree(buf);
			break;
		}

		case NE_REDIRECT:
			if (neon->options.redirects-- > 0){
				const ne_uri *uri = ne_redirect_location(session);

				if (uri) {
					char *url = ne_uri_unparse(uri);

					retval = php_http_neon_request_exec(h, meth_id, url, body);
					free(url);
				}
			}
			break;

		default:
			php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "%s; (%s)", ne_get_error(session), url);
			if (EG(exception)) {
				add_property_long(EG(exception), "neonCode", result);
			}
			retval = FAILURE;
			break;
	}

	switch (result = ne_end_request(request)) {
		case NE_OK:
			break;

		case NE_RETRY:
			if (neon->options.retry.count > tries++) {
				if (neon->options.retry.delay >= PHP_HTTP_DIFFSEC) {
					php_http_sleep(neon->options.retry.delay);
				}
				goto retry;
				break;
			}

		default:
			php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "%s; (%s)", ne_get_error(session), url);
			if (EG(exception)) {
				add_property_long(EG(exception), "neonCode", result);
			}
			retval = FAILURE;
			break;
	}

	ne_session_destroy(session);
	php_url_free(purl);

	return retval;
}

static STATUS php_http_neon_request_setopt(php_http_request_t *h, php_http_request_setopt_opt_t opt, void *arg)
{
	php_http_neon_request_t *neon = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	switch (opt) {
		case PHP_HTTP_REQUEST_OPT_SETTINGS:
			return set_options(h, arg);
			break;

		case PHP_HTTP_REQUEST_OPT_PROGRESS_CALLBACK:
			if (neon->progress.in_cb) {
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Cannot change progress callback while executing it");
				return FAILURE;
			}
			if (neon->progress.callback) {
				php_http_request_progress_dtor(&neon->progress TSRMLS_CC);
			}
			neon->progress.callback = arg;
			break;

		case PHP_HTTP_REQUEST_OPT_COOKIES_ENABLE:
		case PHP_HTTP_REQUEST_OPT_COOKIES_RESET:
		case PHP_HTTP_REQUEST_OPT_COOKIES_RESET_SESSION:
		case PHP_HTTP_REQUEST_OPT_COOKIES_FLUSH:
			/* still NOOPs */
			break;

		default:
			return FAILURE;
	}

	return SUCCESS;
}

static STATUS php_http_neon_request_getopt(php_http_request_t *h, php_http_request_getopt_opt_t opt, void *arg)
{
	php_http_neon_request_t *neon = h->ctx;

	switch (opt) {
		case PHP_HTTP_REQUEST_OPT_PROGRESS_INFO:
			*((php_http_request_progress_t **) arg) = &neon->progress;
			break;

		case PHP_HTTP_REQUEST_OPT_TRANSFER_INFO:
			break;

		default:
			return FAILURE;
	}

	return SUCCESS;
}

static php_http_resource_factory_ops_t php_http_neon_resource_factory_ops = {
		NULL,
		NULL,
		NULL
};

static php_http_request_ops_t php_http_neon_request_ops = {
	&php_http_neon_resource_factory_ops,
	php_http_neon_request_init,
	php_http_neon_request_copy,
	php_http_neon_request_dtor,
	php_http_neon_request_reset,
	php_http_neon_request_exec,
	php_http_neon_request_setopt,
	php_http_neon_request_getopt
};

PHP_HTTP_API php_http_request_ops_t *php_http_neon_get_request_ops(void)
{
	return &php_http_neon_request_ops;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpNEON, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpNEON, method, 0)
#define PHP_HTTP_NEON_ME(method, visibility)	PHP_ME(HttpNEON, method, PHP_HTTP_ARGS(HttpNEON, method), visibility)
#define PHP_HTTP_NEON_ALIAS(method, func)	PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpNEON, method))
#define PHP_HTTP_NEON_MALIAS(me, al, vis)	ZEND_FENTRY(me, ZEND_MN(HttpNEON_##al), PHP_HTTP_ARGS(HttpNEON, al), vis)

PHP_HTTP_EMPTY_ARGS(__construct);

zend_class_entry *php_http_neon_class_entry;
zend_function_entry php_http_neon_method_entry[] = {
	PHP_HTTP_NEON_ME(__construct, ZEND_ACC_PRIVATE|ZEND_ACC_CTOR)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpNEON, __construct) {
}


PHP_MINIT_FUNCTION(http_neon)
{
	php_http_request_factory_driver_t driver = {
		&php_http_neon_request_ops,
		NULL
	};
	if (SUCCESS != php_http_request_factory_add_driver(ZEND_STRL("neon"), &driver)) {
		return FAILURE;
	}

	PHP_HTTP_REGISTER_CLASS(http, NEON, http_neon, php_http_neon_class_entry, 0);

	/*
	* Auth Constants
	*/
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("AUTH_BASIC"), NE_AUTH_BASIC TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("AUTH_DIGEST"), NE_AUTH_DIGEST TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("AUTH_NTLM"), NE_AUTH_NTLM TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("AUTH_GSSAPI"), NE_AUTH_GSSAPI TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("AUTH_GSSNEG"), NE_AUTH_NEGOTIATE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("AUTH_ANY"), NE_AUTH_ALL TSRMLS_CC);

	/*
	* Proxy Type Constants
	*/
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("PROXY_SOCKS4"), NE_SOCK_SOCKSV4 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("PROXY_SOCKS4A"), NE_SOCK_SOCKSV4A TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("PROXY_SOCKS5"), NE_SOCK_SOCKSV5 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_neon_class_entry, ZEND_STRL("PROXY_HTTP"), -1 TSRMLS_CC);

	if (NE_OK != ne_sock_init()) {
		return FAILURE;
	}
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_neon)
{
	ne_sock_exit();
	return SUCCESS;
}
