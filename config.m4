dnl config.m4 for pecl/http
dnl $Id$

PHP_ARG_ENABLE([http], [whether to enable extended HTTP support],
[  --enable-http           Enable extended HTTP support])
PHP_ARG_WITH([http-curl-requests], [wheter to enable cURL HTTP requests],
[  --with-http-curl-requests[=CURLDIR]
                           With cURL HTTP request support])
PHP_ARG_WITH([http-mhash-etags], [whether to enable mhash ETag generator],
[  --with-http-mhash-etags[=MHASHDIR]
                           With mhash ETag generator support])

if test "$PHP_HTTP" != "no"; then

dnl -------
dnl NETDB.H
dnl -------
	AC_MSG_CHECKING(for netdb.h)
	if test -r /usr/include/netdb.h -o -r /usr/local/include/netdb.h; then
		AC_DEFINE(HAVE_NETDB_H, 1, [Have netdb.h])
		AC_MSG_RESULT(found in default path)
	else
		AC_MSG_RESULT(not found in default path)
	fi

dnl ----
dnl CURL
dnl ----
	if test "$PHP_HTTP_CURL_REQUESTS" != "no"; then
	
		AC_MSG_CHECKING([for curl/curl.h])
		CURL_DIR=
		for i in "$PHP_HTTP_CURL_REQUESTS" /usr/local /usr /opt; do
			if test -r "$i/include/curl/curl.h"; then
				CURL_DIR=$i
				break
			fi
		done
		if test -z "$CURL_DIR"; then
			AC_MSG_RESULT([not found])
			AC_MSG_ERROR([could not find curl/curl.h])
		else
			AC_MSG_RESULT([found in $CURL_DIR])
		fi
		
		AC_MSG_CHECKING([for curl-config])
		CURL_CONFIG=
		for i in "$CURL_DIR/bin/curl-config" "$CURL_DIR/curl-config" `which curl-config`; do
			if test -x "$i"; then
				CURL_CONFIG=$i
				break
			fi
		done
		if test -z "$CURL_CONFIG"; then
			AC_MSG_RESULT([not found])
			AC_MSG_ERROR([could not find curl-config])
		else
			AC_MSG_RESULT([found: $CURL_CONFIG])
		fi
		
		PHP_ADD_INCLUDE($CURL_DIR/include)
		PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
		PHP_EVAL_LIBLINE(`$CURL_CONFIG --libs`, HTTP_SHARED_LIBADD)
		AC_DEFINE([HTTP_HAVE_CURL], [1], [Have cURL support])
		
		PHP_CHECK_LIBRARY(curl, curl_multi_strerror, 
			[AC_DEFINE([HAVE_CURL_MULTI_STRERROR], [1], [ ])], [ ], 
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
	fi

dnl ----
dnl MHASH
dnl ----
	if test "$PHP_HTTP_MHASH_ETAGS" != "no"; then
	
		AC_MSG_CHECKING([for mhash.h])
		MHASH_DIR=
		for i in "$PHP_HTTP_MHASH_ETAGS" /usr/local /usr /opt; do
			if test -f "$i/include/mhash.h"; then
				MHASH_DIR=$i
				break
			fi
		done
		if test -z "$MHASH_DIR"; then
			AC_MSG_RESULT([not found])
			AC_MSG_ERROR([could not find mhash.h])
		else
			AC_MSG_RESULT([found in $MHASH_DIR])
		fi
	
		PHP_ADD_INCLUDE($MHASH_DIR/include)
		PHP_ADD_LIBRARY_WITH_PATH(mhash, $MHASH_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
		AC_DEFINE([HAVE_LIBMHASH], [1], [Have mhash support])
	fi

dnl ----
dnl DONE
dnl ----
	PHP_HTTP_SOURCES="missing.c http.c http_functions.c phpstr/phpstr.c \
		http_util_object.c http_message_object.c http_request_object.c http_request_pool_api.c \
		http_response_object.c http_exception_object.c http_requestpool_object.c \
		http_api.c http_cache_api.c http_request_api.c http_date_api.c \
		http_headers_api.c http_message_api.c http_send_api.c http_url_api.c \
		http_info_api.c"
	PHP_NEW_EXTENSION([http], $PHP_HTTP_SOURCES, [$ext_shared])
	PHP_ADD_BUILD_DIR($ext_builddir/phpstr, 1)
	PHP_SUBST([HTTP_SHARED_LIBADD])
	PHP_ADD_MAKEFILE_FRAGMENT
	AC_DEFINE([HAVE_HTTP], [1], [Have extended HTTP support])

dnl ---
dnl odd warnings
dnl ---
dnl		CFLAGS=" -g -O2 -W -Wchar-subscripts -Wformat=2 -Wno-format-y2k -Wimplicit -Wmissing-braces -Wunused-variable -Wbad-function-cast -Wpointer-arith -Wsign-compare -Winline"
dnl		PHP_SUBST([CFLAGS])

fi
