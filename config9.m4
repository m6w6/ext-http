dnl config.m4 for pecl/http
dnl $Id$
dnl vim: noet ts=1 sw=1

PHP_ARG_ENABLE([http], [whether to enable extended HTTP support],
[  --enable-http           Enable extended HTTP support])
PHP_ARG_WITH([http-shared-deps], [whether to depend on extensions which have been built shared],
[  --with-http-shared-deps
                           HTTP: disable to not depend on extensions like hash,
                                 iconv and session (when built shared)], $PHP_HTTP, $PHP_HTTP)
PHP_ARG_WITH([http-curl-requests], [whether to enable cURL HTTP request support],
[  --with-http-curl-requests[=LIBCURLDIR]
                           HTTP: with cURL request support], $PHP_HTTP, $PHP_HTTP)
PHP_ARG_WITH([http-curl-libevent], [whether to enable libevent support fur cURL],
[  --with-http-curl-libevent[=LIBEVENTDIR]
                           HTTP: libevent install directory], $PHP_HTTP_CURL_REQUESTS, "")
PHP_ARG_WITH([http-zlib-compression], [whether to enable zlib encodings support],
[  --with-http-zlib-compression[=LIBZDIR]
                           HTTP: with zlib encodings support], $PHP_HTTP, $PHP_HTTP)
PHP_ARG_WITH([http-magic-mime], [whether to enable response content type guessing],
[  --with-http-magic-mime[=LIBMAGICDIR]
                           HTTP: with magic mime response content type guessing], "no", "no")

if test "$PHP_HTTP" != "no"; then

	ifdef([AC_PROG_EGREP], [
		AC_PROG_EGREP
	], [
		AC_CHECK_PROG(EGREP, egrep, egrep)
	])
	ifdef([AC_PROG_SED], [
		AC_PROG_SED
	], [
		ifdef([LT_AC_PROG_SED], [
			LT_AC_PROG_SED
		], [
			AC_CHECK_PROG(SED, sed, sed)
		])
	])
	
	AC_PROG_CPP
	
	if test "$PHP_HTTP_SHARED_DEPS" != "no"; then
		AC_DEFINE([HTTP_SHARED_DEPS], [1], [ ])
	else
		AC_DEFINE([HTTP_SHARED_DEPS], [0], [ ])
	fi
	
	dnl
	dnl HTTP_SHARED_DEP(name[, code-if-yes[, code-if-not]])
	dnl
	AC_DEFUN([HTTP_SHARED_DEP], [
		extname=$1
		haveext=$[HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)
		
		AC_MSG_CHECKING([whether to add a dependency on ext/$extname])
		if test "$PHP_HTTP_SHARED_DEPS" = "no"; then
			AC_MSG_RESULT([no])
			$3
		elif test "$haveext"; then
			AC_MSG_RESULT([yes])
			ifdef([PHP_ADD_EXTENSION_DEP], [
				PHP_ADD_EXTENSION_DEP([http], $1, true)
			])
			$2
		else
			AC_MSG_RESULT([no])
			$3
		fi
	])
	
	dnl
	dnl HTTP_HAVE_PHP_EXT(name[, code-if-yes[, code-if-not]])
	dnl
	AC_DEFUN([HTTP_HAVE_PHP_EXT], [
		extname=$1
		haveext=$[PHP_]translit($1,a-z_-,A-Z__)
		ishared=$[PHP_]translit($1,a-z_-,A-Z__)_SHARED
		
		AC_MSG_CHECKING([for ext/$extname support])
		if test -x "$PHP_EXECUTABLE"; then
			if test "`$PHP_EXECUTABLE -m | $EGREP ^$extname\$`" = "$extname"; then
				[HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=1
				AC_MSG_RESULT([yes])
				$2
			else
				[HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=
				AC_MSG_RESULT([no])
				$3
			fi
		elif test "$haveext" != "no" && test "x$haveext" != "x"; then
			[HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=1
			AC_MSG_RESULT([yes])
			$2
		else
			[HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=
			AC_MSG_RESULT([no])
			$3
		fi
	])

dnl ----
dnl STDC
dnl ----
	AC_CHECK_HEADERS([netdb.h unistd.h])
	PHP_CHECK_FUNC(gethostname, nsl)
	PHP_CHECK_FUNC(getdomainname, nsl)
	PHP_CHECK_FUNC(getservbyport, nsl)
	PHP_CHECK_FUNC(getservbyname, nsl)

dnl ----
dnl ZLIB
dnl ----
	if test "$PHP_HTTP_ZLIB_COMPRESSION" != "no"; then
		AC_MSG_CHECKING([for zlib.h])
		ZLIB_DIR=
		for i in "$PHP_HTTP_ZLIB_COMPRESSION" "$PHP_ZLIB_DIR" "$PHP_ZLIB" /usr/local /usr /opt; do
			if test -f "$i/include/zlib.h"; then
				ZLIB_DIR=$i
				break;
			fi
		done
		if test -z "$ZLIB_DIR"; then
			AC_MSG_RESULT([not found])
			AC_MSG_ERROR([could not find zlib.h])
		else
			AC_MSG_RESULT([found in $ZLIB_DIR])
			AC_MSG_CHECKING([for zlib version >= 1.2.0.4])
			ZLIB_VERSION=`$EGREP "define ZLIB_VERSION" $ZLIB_DIR/include/zlib.h | $SED -e 's/[[^0-9\.]]//g'`
			AC_MSG_RESULT([$ZLIB_VERSION])
			if test `echo $ZLIB_VERSION | $SED -e 's/[[^0-9]]/ /g' | $AWK '{print $1*1000000 + $2*10000 + $3*100 + $4}'` -lt 1020004; then
				AC_MSG_ERROR([libz version greater or equal to 1.2.0.4 required])
			else
				PHP_ADD_INCLUDE($ZLIB_DIR/include)
				PHP_ADD_LIBRARY_WITH_PATH(z, $ZLIB_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
				AC_DEFINE([HTTP_HAVE_ZLIB], [1], [Have zlib support])
			fi
		fi
	fi
	
dnl ----
dnl CURL
dnl ----
	if test "$PHP_HTTP_CURL_REQUESTS" != "no"; then
		AC_MSG_CHECKING([for curl/curl.h])
		CURL_DIR=
		for i in "$PHP_HTTP_CURL_REQUESTS" /usr/local /usr /opt; do
			if test -f "$i/include/curl/curl.h"; then
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
		
		dnl Debian stable has currently 7.13.2 (this is not a typo)
		AC_MSG_CHECKING([for curl version >= 7.12.3])
		CURL_VERSION=`$CURL_CONFIG --version | $SED -e 's/[[^0-9\.]]//g'`
		AC_MSG_RESULT([$CURL_VERSION])
		if test `echo $CURL_VERSION | $SED -e 's/[[^0-9]]/ /g' | $AWK '{print $1*10000 + $2*100 + $3}'` -lt 71203; then
			AC_MSG_ERROR([libcurl version greater or equal to 7.12.3 required])
		fi
		
		dnl
		dnl compile tests
		dnl
		
		save_CFLAGS="$CFLAGS"
		CFLAGS="`$CURL_CONFIG --cflags`"
		save_LDFLAGS="$LDFLAGS"
		LDFLAGS="`$CURL_CONFIG --libs` $ld_runpath_switch$CURL_DIR/$PHP_LIBDIR"
		
		AC_MSG_CHECKING([for SSL support in libcurl])
		CURL_SSL=`$CURL_CONFIG --feature | $EGREP SSL`
		if test "$CURL_SSL" = "SSL"; then
			AC_MSG_RESULT([yes])
			AC_DEFINE([HTTP_HAVE_SSL], [1], [ ])
			
			AC_MSG_CHECKING([for openssl support in libcurl])
			AC_TRY_RUN([
				#include <curl/curl.h>
				int main(int argc, char *argv[]) {
					curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
					if (data && data->ssl_version && *data->ssl_version) {
						const char *ptr = data->ssl_version;
						while(*ptr == ' ') ++ptr;
						return strncasecmp(ptr, "OpenSSL", sizeof("OpenSSL")-1);
					}
					return 1;
				}
			], [
				AC_MSG_RESULT([yes])
				AC_CHECK_HEADER([openssl/crypto.h], [
					AC_DEFINE([HTTP_HAVE_OPENSSL], [1], [ ])
				])
			], [
				AC_MSG_RESULT([no])
			], [
				AC_MSG_RESULT([no])
			])
			
			AC_MSG_CHECKING([for gnutls support in libcurl])
			AC_TRY_RUN([
				#include <curl/curl.h>
				int main(int argc, char *argv[]) {
					curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
					if (data && data->ssl_version && *data->ssl_version) {
						const char *ptr = data->ssl_version;
						while(*ptr == ' ') ++ptr;
						return strncasecmp(ptr, "GnuTLS", sizeof("GnuTLS")-1);
					}
					return 1;
				}
			], [
				AC_MSG_RESULT([yes])
				AC_CHECK_HEADER([gcrypt.h], [
					AC_DEFINE([HTTP_HAVE_GNUTLS], [1], [ ])
				])
			], [
				AC_MSG_RESULT([no])
			], [
				AC_MSG_RESULT([no])
			])
		else
			AC_MSG_RESULT([no])
		fi
		
		CFLAGS="$save_CFLAGS"
		LDFLAGS="$save_LDFLAGS"
		
		dnl end compile tests
		
		AC_MSG_CHECKING([for bundled SSL CA info])
		CURL_CAINFO=
		for i in `$CURL_CONFIG --ca` "/etc/ssl/certs/ca-certificates.crt"; do
			if test -f "$i"; then
				CURL_CAINFO="$i"
				break
			fi
		done
		if test -z "$CURL_CAINFO"; then
			AC_MSG_RESULT([not found])
		else
			AC_MSG_RESULT([$CURL_CAINFO])
			AC_DEFINE_UNQUOTED([HTTP_CURL_CAINFO], ["$CURL_CAINFO"], [path to bundled SSL CA info])
		fi
		
		PHP_ADD_INCLUDE($CURL_DIR/include)
		PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
		PHP_EVAL_LIBLINE($CURL_LIBS, HTTP_SHARED_LIBADD)
		AC_DEFINE([HTTP_HAVE_CURL], [1], [Have cURL support])
		
		PHP_CHECK_LIBRARY(curl, curl_share_strerror, 
			[AC_DEFINE([HAVE_CURL_SHARE_STRERROR], [1], [ ])], [ ],
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
		PHP_CHECK_LIBRARY(curl, curl_multi_strerror, 
			[AC_DEFINE([HAVE_CURL_MULTI_STRERROR], [1], [ ])], [ ], 
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
		PHP_CHECK_LIBRARY(curl, curl_easy_strerror,
			[AC_DEFINE([HAVE_CURL_EASY_STRERROR], [1], [ ])], [ ],
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
		PHP_CHECK_LIBRARY(curl, curl_easy_reset,
			[AC_DEFINE([HAVE_CURL_EASY_RESET], [1], [ ])], [ ],
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
		PHP_CHECK_LIBRARY(curl, curl_formget,
			[AC_DEFINE([HAVE_CURL_FORMGET], [1], [ ])], [ ],
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
		PHP_CHECK_LIBRARY(curl, curl_multi_setopt, 
			[AC_DEFINE([HAVE_CURL_MULTI_SETOPT], [1], [ ])], [ ], 
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
		PHP_CHECK_LIBRARY(curl, curl_multi_timeout, 
			[AC_DEFINE([HAVE_CURL_MULTI_TIMEOUT], [1], [ ])], [ ],
			[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
		)
		
		dnl ----
		dnl EVENT
		dnl ----
		
		if test "$PHP_HTTP_CURL_LIBEVENT" != "no"; then
			AC_MSG_CHECKING([for event.h])
			EVENT_DIR=
			for i in "$PHP_HTTP_CURL_LIBEVENT" /usr/local /usr /opt; do
				if test -f "$i/include/event.h"; then
					EVENT_DIR=$i
					break
				fi
			done
			if test -z "$EVENT_DIR"; then
				AC_MSG_RESULT([not found])
				AC_MSG_WARN([continuing without libevent support])
			else
				AC_MSG_RESULT([found in $EVENT_DIR])
				
				AC_MSG_CHECKING([for libevent version, roughly])
				EVENT_VER="1.1b or lower"
				if test -f "$EVENT_DIR/include/evhttp.h" && test -f "$EVENT_DIR/include/evdns.h"; then
					if test -f "$EVENT_DIR/include/evrpc.h"; then
						EVENT_VER="1.4 or greater"
					else
						EVENT_VER="1.2 or greater"
					fi
				fi
				AC_DEFINE_UNQUOTED([HTTP_EVENT_VERSION], ["$EVENT_VER"], [ ])
				AC_MSG_RESULT([$EVENT_VER])
				
				AC_MSG_CHECKING([for libcurl version >= 7.16.0])
				AC_MSG_RESULT([$CURL_VERSION])
				if test `echo $CURL_VERSION | $SED -e 's/[[^0-9]]/ /g' | $AWK '{print $1*10000 + $2*100 + $3}'` -lt 71600; then
					AC_MSG_WARN([libcurl version greater or equal to 7.16.0 required; continuing without libevent support])
				else
					PHP_ADD_INCLUDE($EVENT_DIR/include)
					PHP_ADD_LIBRARY_WITH_PATH(event, $EVENT_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
					AC_DEFINE([HTTP_HAVE_EVENT], [1], [Have libevent support for cURL])
					PHP_CHECK_LIBRARY(curl, curl_multi_socket_action, 
						[AC_DEFINE([HAVE_CURL_MULTI_SOCKET_ACTION], [1], [ ])], [ ],
						[$CURL_LIBS -L$CURL_DIR/$PHP_LIBDIR]
					)
				fi
			fi
		fi
	fi

dnl ----
dnl MAGIC
dnl ----
	if test "$PHP_HTTP_MAGIC_MIME" != "no"; then
		AC_MSG_CHECKING([for magic.h])
		MAGIC_DIR=
		for i in "$PHP_HTTP_MAGIC_MIME" /usr/local /usr /opt; do
			if test -f "$i/include/magic.h"; then
				MAGIC_DIR=$i
				break
			fi
		done
		if test -z "$MAGIC_DIR"; then
			AC_MSG_RESULT([not found])
			AC_MSG_ERROR([could not find magic.h])
		else
			AC_MSG_RESULT([found in $MAGIC_DIR])
		fi
		
		PHP_ADD_INCLUDE($MAGIC_DIR/include)
		PHP_ADD_LIBRARY_WITH_PATH(magic, $MAGIC_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
		AC_DEFINE([HTTP_HAVE_MAGIC], [1], [Have magic mime support])
	fi

dnl ----
dnl HASH
dnl ----
	HTTP_HAVE_PHP_EXT([hash], [
		AC_MSG_CHECKING([for php_hash.h])
		HTTP_EXT_HASH_INCDIR=
		for i in `echo $INCLUDES | $SED -e's/-I//g'` $abs_srcdir ../hash; do
			if test -d $i; then
				if test -f $i/php_hash.h; then
					HTTP_EXT_HASH_INCDIR=$i
					break
				elif test -f $i/ext/hash/php_hash.h; then
					HTTP_EXT_HASH_INCDIR=$i/ext/hash
					break
				fi
			fi
		done
		if test -z "$HTTP_EXT_HASH_INCDIR"; then
			AC_MSG_RESULT([not found])
		else
			AC_MSG_RESULT([$HTTP_EXT_HASH_INCDIR])
			AC_DEFINE([HTTP_HAVE_PHP_HASH_H], [1], [Have ext/hash support])
			PHP_ADD_INCLUDE([$HTTP_EXT_HASH_INCDIR])
		fi
	])

dnl ----
dnl ICONV
dnl ----
	HTTP_HAVE_PHP_EXT([iconv])

dnl ----
dnl SESSION
dnl ----
	HTTP_HAVE_PHP_EXT([session])

dnl ----
dnl DONE
dnl ----
	PHP_HTTP_SOURCES="missing.c http.c http_functions.c phpstr/phpstr.c \
		http_util_object.c http_message_object.c http_request_object.c http_request_pool_api.c \
		http_response_object.c http_exception_object.c http_requestpool_object.c \
		http_api.c http_cache_api.c http_request_api.c http_request_info.c http_date_api.c \
		http_headers_api.c http_message_api.c http_send_api.c http_url_api.c \
		http_info_api.c http_request_method_api.c http_encoding_api.c \
		http_filter_api.c http_request_body_api.c http_querystring_object.c \
		http_deflatestream_object.c http_inflatestream_object.c http_cookie_api.c \
		http_querystring_api.c http_request_datashare_api.c http_requestdatashare_object.c \
		http_persistent_handle_api.c"
	
	PHP_NEW_EXTENSION([http], $PHP_HTTP_SOURCES, $ext_shared)
	
	dnl shared extension deps
	HTTP_SHARED_DEP([hash])
	HTTP_SHARED_DEP([iconv])
	HTTP_SHARED_DEP([session])
	
	PHP_ADD_BUILD_DIR($ext_builddir/phpstr, 1)
	PHP_SUBST([HTTP_SHARED_LIBADD])

	PHP_HTTP_HEADERS="php_http_std_defs.h php_http.h php_http_api.h php_http_cache_api.h \
		php_http_date_api.h php_http_headers_api.h php_http_info_api.h php_http_message_api.h \
		php_http_request_api.h php_http_request_method_api.h php_http_send_api.h php_http_url_api.h \
		php_http_encoding_api.h phpstr/phpstr.h missing.h php_http_request_body_api.h \
		php_http_exception_object.h php_http_message_object.h php_http_request_object.h \
		php_http_requestpool_object.h php_http_response_object.h php_http_util_object.h \
		php_http_querystring_object.h php_http_deflatestream_object.h php_http_inflatestream_object.h \
		php_http_cookie_api.h php_http_querystring_api.h php_http_request_datashare_api.h php_http_requestdatashare_object.h \
		php_http_persistent_handle_api.h"
	ifdef([PHP_INSTALL_HEADERS], [
		PHP_INSTALL_HEADERS(ext/http, $PHP_HTTP_HEADERS)
	], [
		PHP_SUBST([PHP_HTTP_HEADERS])
		PHP_ADD_MAKEFILE_FRAGMENT
	])

	AC_DEFINE([HAVE_HTTP], [1], [Have extended HTTP support])
fi
