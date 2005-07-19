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
#include "php.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "php_http_std_defs.h"
#include "php_http_request_object.h"
#include "php_http_request_api.h"
#include "php_http_request_pool_api.h"
#include "php_http_api.h"
#include "php_http_url_api.h"
#include "php_http_message_api.h"
#include "php_http_message_object.h"

#ifdef PHP_WIN32
#	include <winsock2.h>
#endif
#include <curl/curl.h>

#define HTTP_BEGIN_ARGS(method, req_args) 		HTTP_BEGIN_ARGS_EX(HttpRequest, method, ZEND_RETURN_REFERENCE_AGNOSTIC, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)		HTTP_EMPTY_ARGS_EX(HttpRequest, method, ret_ref)
#define HTTP_REQUEST_ME(method, visibility)		PHP_ME(HttpRequest, method, HTTP_ARGS(HttpRequest, method), visibility)

HTTP_EMPTY_ARGS(__destruct, 0);
HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(method, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getOptions, 0);
HTTP_EMPTY_ARGS(unsetOptions, 0);
HTTP_BEGIN_ARGS(setOptions, 1)
	HTTP_ARG_VAL(options, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getSslOptions, 0);
HTTP_EMPTY_ARGS(unsetSslOptions, 0);
HTTP_BEGIN_ARGS(setSslOptions, 1)
	HTTP_ARG_VAL(ssl_options, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getHeaders, 0);
HTTP_EMPTY_ARGS(unsetHeaders, 0);
HTTP_BEGIN_ARGS(addHeaders, 1)
	HTTP_ARG_VAL(headers, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getCookies, 0);
HTTP_EMPTY_ARGS(unsetCookies, 0);
HTTP_BEGIN_ARGS(addCookies, 1)
	HTTP_ARG_VAL(cookies, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getUrl, 0);
HTTP_BEGIN_ARGS(setUrl, 1)
	HTTP_ARG_VAL(url, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getMethod, 0);
HTTP_BEGIN_ARGS(setMethod, 1)
	HTTP_ARG_VAL(request_method, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getContentType, 0);
HTTP_BEGIN_ARGS(setContentType, 1)
	HTTP_ARG_VAL(content_type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getQueryData, 0);
HTTP_EMPTY_ARGS(unsetQueryData, 0);
HTTP_BEGIN_ARGS(setQueryData, 1)
	HTTP_ARG_VAL(query_data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addQueryData, 1)
	HTTP_ARG_VAL(query_data, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getPostFields, 0);
HTTP_EMPTY_ARGS(unsetPostFields, 0);
HTTP_BEGIN_ARGS(setPostFields, 1)
	HTTP_ARG_VAL(post_fields, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addPostFields, 1)
	HTTP_ARG_VAL(post_fields, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getPostFiles, 0);
HTTP_EMPTY_ARGS(unsetPostFiles, 0);
HTTP_BEGIN_ARGS(setPostFiles, 1)
	HTTP_ARG_VAL(post_files, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addPostFile, 2)
	HTTP_ARG_VAL(formname, 0)
	HTTP_ARG_VAL(filename, 0)
	HTTP_ARG_VAL(content_type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getPutFile, 0);
HTTP_EMPTY_ARGS(unsetPutFile, 0);
HTTP_BEGIN_ARGS(setPutFile, 1)
	HTTP_ARG_VAL(filename, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseData, 0);
HTTP_BEGIN_ARGS(getResponseHeader, 0)
	HTTP_ARG_VAL(name, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(getResponseCookie, 0)
	HTTP_ARG_VAL(name, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseBody, 0);
HTTP_EMPTY_ARGS(getResponseCode, 0);
HTTP_BEGIN_ARGS(getResponseInfo, 0)
	HTTP_ARG_VAL(name, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseMessage, 1);
HTTP_EMPTY_ARGS(send, 0);

#define http_request_object_declare_default_properties() _http_request_object_declare_default_properties(TSRMLS_C)
static inline void _http_request_object_declare_default_properties(TSRMLS_D);

zend_class_entry *http_request_object_ce;
zend_function_entry http_request_object_fe[] = {
	HTTP_REQUEST_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_REQUEST_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	HTTP_REQUEST_ME(setOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(setSslOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getSslOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetSslOptions, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(addHeaders, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getHeaders, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetHeaders, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addCookies, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getCookies, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetCookies, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setMethod, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getMethod, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setUrl, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getUrl, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setContentType, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getContentType, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setQueryData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getQueryData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addQueryData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetQueryData, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setPostFields, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getPostFields, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addPostFields, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetPostFields, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setPostFiles, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addPostFile, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getPostFiles, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetPostFiles, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setPutFile, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getPutFile, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(unsetPutFile, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(send, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(getResponseData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseHeader, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseCookie, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseCode, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseBody, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseInfo, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseMessage, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};
static zend_object_handlers http_request_object_handlers;

void _http_request_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS_EX(HttpRequest, http_request_object, NULL, 0);

	/* HTTP/1.1 */
	HTTP_LONG_CONSTANT("HTTP_GET", HTTP_GET);
	HTTP_LONG_CONSTANT("HTTP_HEAD", HTTP_HEAD);
	HTTP_LONG_CONSTANT("HTTP_POST", HTTP_POST);
	HTTP_LONG_CONSTANT("HTTP_PUT", HTTP_PUT);
	HTTP_LONG_CONSTANT("HTTP_DELETE", HTTP_DELETE);
	HTTP_LONG_CONSTANT("HTTP_OPTIONS", HTTP_OPTIONS);
	HTTP_LONG_CONSTANT("HTTP_TRACE", HTTP_TRACE);
	HTTP_LONG_CONSTANT("HTTP_CONNECT", HTTP_CONNECT);
	/* WebDAV - RFC 2518 */
	HTTP_LONG_CONSTANT("HTTP_PROPFIND", HTTP_PROPFIND);
	HTTP_LONG_CONSTANT("HTTP_PROPPATCH", HTTP_PROPPATCH);
	HTTP_LONG_CONSTANT("HTTP_MKCOL", HTTP_MKCOL);
	HTTP_LONG_CONSTANT("HTTP_COPY", HTTP_COPY);
	HTTP_LONG_CONSTANT("HTTP_MOVE", HTTP_MOVE);
	HTTP_LONG_CONSTANT("HTTP_LOCK", HTTP_LOCK);
	HTTP_LONG_CONSTANT("HTTP_UNLOCK", HTTP_UNLOCK);
	/* WebDAV Versioning - RFC 3253 */
	HTTP_LONG_CONSTANT("HTTP_VERSION_CONTROL", HTTP_VERSION_CONTROL);
	HTTP_LONG_CONSTANT("HTTP_REPORT", HTTP_REPORT);
	HTTP_LONG_CONSTANT("HTTP_CHECKOUT", HTTP_CHECKOUT);
	HTTP_LONG_CONSTANT("HTTP_CHECKIN", HTTP_CHECKIN);
	HTTP_LONG_CONSTANT("HTTP_UNCHECKOUT", HTTP_UNCHECKOUT);
	HTTP_LONG_CONSTANT("HTTP_MKWORKSPACE", HTTP_MKWORKSPACE);
	HTTP_LONG_CONSTANT("HTTP_UPDATE", HTTP_UPDATE);
	HTTP_LONG_CONSTANT("HTTP_LABEL", HTTP_LABEL);
	HTTP_LONG_CONSTANT("HTTP_MERGE", HTTP_MERGE);
	HTTP_LONG_CONSTANT("HTTP_BASELINE_CONTROL", HTTP_BASELINE_CONTROL);
	HTTP_LONG_CONSTANT("HTTP_MKACTIVITY", HTTP_MKACTIVITY);
	/* WebDAV Access Control - RFC 3744 */
	HTTP_LONG_CONSTANT("HTTP_ACL", HTTP_ACL);


#	if LIBCURL_VERSION_NUM >= 0x070a05
	HTTP_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC);
	HTTP_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST);
	HTTP_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM);
#	endif /* LIBCURL_VERSION_NUM */
}

zend_object_value _http_request_object_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_request_object *o;

	o = ecalloc(1, sizeof(http_request_object));
	o->zo.ce = ce;
	o->ch = curl_easy_init();
	o->pool = NULL;

	phpstr_init_ex(&o->response, HTTP_CURLBUF_SIZE, 0);

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, http_request_object_free, NULL TSRMLS_CC);
	ov.handlers = &http_request_object_handlers;

	return ov;
}

static inline void _http_request_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_request_object_ce;

	DCL_PROP_N(PROTECTED, options);
	DCL_PROP_N(PROTECTED, responseInfo);
	DCL_PROP_N(PROTECTED, responseData);
	DCL_PROP_N(PROTECTED, responseCode);
	DCL_PROP_N(PROTECTED, responseMessage);
	DCL_PROP_N(PROTECTED, postFields);
	DCL_PROP_N(PROTECTED, postFiles);

	DCL_PROP(PROTECTED, long, method, HTTP_GET);

	DCL_PROP(PROTECTED, string, url, "");
	DCL_PROP(PROTECTED, string, contentType, "");
	DCL_PROP(PROTECTED, string, queryData, "");
	DCL_PROP(PROTECTED, string, putFile, "");
}

void _http_request_object_free(zend_object *object TSRMLS_DC)
{
	http_request_object *o = (http_request_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->ch) {
		curl_easy_cleanup(o->ch);
	}
	phpstr_dtor(&o->response);
	efree(o);
}

STATUS _http_request_object_requesthandler(http_request_object *obj, zval *this_ptr, http_request_body *body TSRMLS_DC)
{
	zval *meth, *URL, *qdata, *opts;
	char *request_uri;
	STATUS status;

	if (!body) {
		return FAILURE;
	}
	if ((!obj->ch) && (!(obj->ch = curl_easy_init()))) {
		http_error(E_WARNING, HTTP_E_CURL, "Could not initilaize curl");
		return FAILURE;
	}

	meth  = GET_PROP(obj, method);
	URL   = GET_PROP(obj, url);
	qdata = GET_PROP(obj, queryData);
	opts  = GET_PROP(obj, options);

	// HTTP_URI_MAXLEN+1 long char *
	if (!(request_uri = http_absolute_uri_ex(Z_STRVAL_P(URL), Z_STRLEN_P(URL), NULL, 0, NULL, 0, 0))) {
		return FAILURE;
	}

	if (Z_STRLEN_P(qdata) && (strlen(request_uri) < HTTP_URI_MAXLEN)) {
		if (!strchr(request_uri, '?')) {
			strcat(request_uri, "?");
		} else {
			strcat(request_uri, "&");
		}
		strncat(request_uri, Z_STRVAL_P(qdata), HTTP_URI_MAXLEN - strlen(request_uri));
	}

	switch (Z_LVAL_P(meth))
	{
		case HTTP_GET:
		case HTTP_HEAD:
			body->type = -1;
			status = http_request_init(obj->ch, Z_LVAL_P(meth), request_uri, NULL, Z_ARRVAL_P(opts), &obj->response);
		break;

		case HTTP_PUT:
		{
			php_stream *stream;
			php_stream_statbuf ssb;
			zval *file = GET_PROP(obj, putFile);

			if (	(stream = php_stream_open_wrapper(Z_STRVAL_P(file), "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL)) &&
					!php_stream_stat(stream, &ssb)) {
				body->type = HTTP_REQUEST_BODY_UPLOADFILE;
				body->data = stream;
				body->size = ssb.sb.st_size;

				status = http_request_init(obj->ch, HTTP_PUT, request_uri, body, Z_ARRVAL_P(opts), &obj->response);
			} else {
				status = FAILURE;
			}
		}
		break;

		case HTTP_POST:
		{
			zval *fields = GET_PROP(obj, postFields), *files = GET_PROP(obj, postFiles);

			if (SUCCESS == (status = http_request_body_fill(body, Z_ARRVAL_P(fields), Z_ARRVAL_P(files)))) {
				status = http_request_init(obj->ch, HTTP_POST, request_uri, body, Z_ARRVAL_P(opts), &obj->response);
			}
		}
		break;

		default:
		{
			zval *post = GET_PROP(obj, postData);

			body->type = HTTP_REQUEST_BODY_CSTRING;
			body->data = Z_STRVAL_P(post);
			body->size = Z_STRLEN_P(post);

			status = http_request_init(obj->ch, Z_LVAL_P(meth), request_uri, body, Z_ARRVAL_P(opts), &obj->response);
		}
		break;
	}

	efree(request_uri);
	return status;
}

STATUS _http_request_object_responsehandler(http_request_object *obj, zval *this_ptr TSRMLS_DC)
{
	http_message *msg;

	phpstr_fix(&obj->response);

	if (msg = http_message_parse(PHPSTR_VAL(&obj->response), PHPSTR_LEN(&obj->response))) {
		char *body;
		size_t body_len;
		zval *headers, *message, *resp = GET_PROP(obj, responseData), *info = GET_PROP(obj, responseInfo);

		UPD_PROP(obj, long, responseCode, msg->info.response.code);

		MAKE_STD_ZVAL(headers)
		array_init(headers);

		zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		phpstr_data(PHPSTR(msg), &body, &body_len);

		add_assoc_zval(resp, "headers", headers);
		add_assoc_stringl(resp, "body", body, body_len, 0);

		MAKE_STD_ZVAL(message);
		message->type = IS_OBJECT;
		message->is_ref = 1;
		message->value.obj = http_message_object_from_msg(msg);
		SET_PROP(obj, responseMessage, message);
		zval_ptr_dtor(&message);

		http_request_info(obj->ch, Z_ARRVAL_P(info));
		SET_PROP(obj, responseInfo, info);

		return SUCCESS;
	}
	return FAILURE;
}

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

