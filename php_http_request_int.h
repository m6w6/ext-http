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

#if defined(ZTS) && defined(HTTP_HAVE_SSL)
#	ifdef PHP_WIN32
#		define HTTP_NEED_OPENSSL_TSL
#		include <openssl/crypto.h>
#	else /* !PHP_WIN32 */
#		if defined(HTTP_HAVE_OPENSSL)
#			define HTTP_NEED_OPENSSL_TSL
#			include <openssl/crypto.h>
#		elif defined(HTTP_HAVE_GNUTLS)
#			define HTTP_NEED_GNUTLS_TSL
#			include <gcrypt.h>
#		else
#			warning \
				"libcurl was compiled with SSL support, but configure could not determine which" \
				"library was used; thus no SSL crypto locking callbacks will be set, which may " \
				"cause random crashes on SSL requests"
#		endif /* HTTP_HAVE_OPENSSL || HTTP_HAVE_GNUTLS */
#	endif /* PHP_WIN32 */
#endif /* ZTS && HTTP_HAVE_SSL */

#define HTTP_CURL_OPT(OPTION, p) curl_easy_setopt((request->ch), OPTION, (p))

#define HTTP_CURL_OPT_STRING(OPTION, ldiff, obdc) \
	{ \
		char *K = #OPTION; \
		HTTP_CURL_OPT_STRING_EX(K+lenof("CURLOPT_KEY")+ldiff, OPTION, obdc); \
	}
#define HTTP_CURL_OPT_STRING_EX(keyname, optname, obdc) \
	if (!strcasecmp(key.str, keyname)) { \
		zval *copy = http_request_option_cache_ex(request, keyname, strlen(keyname)+1, 0, zval_copy(IS_STRING, *param)); \
		if (obdc) { \
			HTTP_CHECK_OPEN_BASEDIR(Z_STRVAL_P(copy), return FAILURE); \
		} \
		HTTP_CURL_OPT(optname, Z_STRVAL_P(copy)); \
		continue; \
	}
#define HTTP_CURL_OPT_LONG(OPTION, ldiff) \
	{ \
		char *K = #OPTION; \
		HTTP_CURL_OPT_LONG_EX(K+lenof("CURLOPT_KEY")+ldiff, OPTION); \
	}
#define HTTP_CURL_OPT_LONG_EX(keyname, optname) \
	if (!strcasecmp(key.str, keyname)) { \
		zval *copy = zval_copy(IS_LONG, *param); \
		HTTP_CURL_OPT(optname, Z_LVAL_P(copy)); \
		zval_free(&copy); \
		continue; \
	}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
