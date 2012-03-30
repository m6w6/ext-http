dnl config.m4 for pecl/http
dnl $Id: config9.m4 242664 2007-09-18 19:13:37Z mike $
dnl vim: noet ts=4 sw=4

PHP_ARG_WITH([http], [whether to enable extended HTTP support],
[  --with-http             Enable extended HTTP support])
PHP_ARG_WITH([http-zlib-dir], [],
[  --with-http-zlib-dir[=DIR]     HTTP: where to find zlib], $PHP_HTTP, $PHP_HTTP)
PHP_ARG_WITH([http-libcurl-dir], [],
[  --with-http-libcurl-dir[=DIR]  HTTP: where to find libcurl], $PHP_HTTP, $PHP_HTTP)
PHP_ARG_WITH([http-libevent-dir], [],
[  --with-http-libevent-dir[=DIR] HTTP: where to find libevent], $PHP_HTTP_LIBCURL_DIR, "")

if test "$PHP_HTTP" != "no"; then

	HTTP_HAVE_A_REQUEST_LIB=false

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
		AC_DEFINE([PHP_HTTP_SHARED_DEPS], [1], [ ])
	else
		AC_DEFINE([PHP_HTTP_SHARED_DEPS], [0], [ ])
	fi
	
	dnl
	dnl HTTP_SHARED_DEP(name[, code-if-yes[, code-if-not]])
	dnl
	AC_DEFUN([HTTP_SHARED_DEP], [
		extname=$1
		haveext=$[PHP_HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)
		
		AC_MSG_CHECKING([whether to add a dependency on ext/$extname])
		if test "$PHP_HTTP_SHARED_DEPS" = "no"; then
			AC_MSG_RESULT([no])
			$3
		elif test "$haveext"; then
			AC_MSG_RESULT([yes])
			AC_DEFINE([PHP_HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__), [1], [ ])
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
		
		AC_MSG_CHECKING([for ext/$extname support])
		if test -x "$PHP_EXECUTABLE"; then
			grepext=`$PHP_EXECUTABLE -m | $EGREP ^$extname\$`
			if test "$grepext" = "$extname"; then
				[PHP_HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=1
				AC_MSG_RESULT([yes])
				$2
			else
				[PHP_HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=
				AC_MSG_RESULT([no])
				$3
			fi
		elif test "$haveext" != "no" && test "x$haveext" != "x"; then
			[PHP_HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=1
			AC_MSG_RESULT([yes])
			$2
		else
			[PHP_HTTP_HAVE_EXT_]translit($1,a-z_-,A-Z__)=
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
	AC_MSG_CHECKING([for zlib.h])
	ZLIB_DIR=
	for i in "$PHP_HTTP_ZLIB_DIR" "$PHP_ZLIB_DIR" "$PHP_ZLIB" /usr/local /usr /opt; do
		if test -f "$i/include/zlib.h"; then
			ZLIB_DIR=$i
			break;
		fi
	done
	if test "x$ZLIB_DIR" = "x"; then
		AC_MSG_RESULT([not found])
		AC_MSG_ERROR([could not find zlib.h])
	else
		AC_MSG_RESULT([found in $ZLIB_DIR])
		AC_MSG_CHECKING([for zlib version >= 1.2.0.4])
		ZLIB_VERSION=`$EGREP "define ZLIB_VERSION" $ZLIB_DIR/include/zlib.h | $SED -e 's/[[^0-9\.]]//g'`
		AC_MSG_RESULT([$ZLIB_VERSION])
		if test `echo $ZLIB_VERSION | $SED -e 's/[[^0-9]]/ /g' | $AWK '{print $1*1000000 + $2*10000 + $3*100 + $4}'` -lt 1020004; then
			AC_MSG_ERROR([zlib version greater or equal to 1.2.0.4 required])
		else
			PHP_ADD_INCLUDE($ZLIB_DIR/include)
			PHP_ADD_LIBRARY_WITH_PATH(z, $ZLIB_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
		fi
	fi
	
dnl ----
dnl SERF
dnl ----

	if test "$PHP_HTTP_LIBSERF_DIR" = "no"; then
		AC_DEFINE([PHP_HTTP_HAVE_SERF], [0], [ ])
	else
		AC_MSG_CHECKING([for serf-?/serf.h])
		SERF_DIR=
		for i in "$PHP_HTTP_LIBSERF_DIR" /usr/local /usr /opt; do
			if test -f "$i/include/serf-0/serf.h"; then
				SERF_DIR=$i
				SERF_VER=0
				break
			elif test -f "$i/include/serf-1/serf.h"; then
				SERF_DIR=$i
				SERF_VER=1
			fi
		done

		if test "x$SERF_DIR" = "x"; then
			AC_MSG_RESULT([not found])
			AC_DEFINE([PHP_HTTP_HAVE_SERF], [0], [ ])
		else
			AC_MSG_RESULT([found in $SERF_DIR])

			PHP_ADD_INCLUDE($SERF_DIR/include/serf-$SERF_VER)
			PHP_ADD_LIBRARY_WITH_PATH(serf-$SERF_VER, $SERF_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
			AC_DEFINE([PHP_HTTP_HAVE_SERF], [1], [Have libserf support])
			HTTP_HAVE_A_REQUEST_LIB=true
		fi
	fi
	
dnl ----
dnl CURL
dnl ----

	if test "$PHP_HTTP_LIBCURL_DIR" = "no"; then
		AC_DEFINE([PHP_HTTP_HAVE_CURL], [0], [ ])
	else
		AC_MSG_CHECKING([for curl/curl.h])
		CURL_DIR=
		for i in "$PHP_HTTP_LIBCURL_DIR" /usr/local /usr /opt; do
			if test -f "$i/include/curl/curl.h"; then
				CURL_DIR=$i
				break
			fi
		done
		if test "x$CURL_DIR" = "x"; then
			AC_MSG_RESULT([not found])
		else
			AC_MSG_RESULT([found in $CURL_DIR])
		
			AC_MSG_CHECKING([for curl-config])
			CURL_CONFIG=
			for i in "$CURL_DIR/bin/curl-config" "$CURL_DIR/curl-config" `which curl-config`; do
				if test -x "$i"; then
					CURL_CONFIG=$i
					break
				fi
			done
			if test "x$CURL_CONFIG" = "x"; then
				AC_MSG_RESULT([not found])
				AC_MSG_ERROR([could not find curl-config])
			else
				AC_MSG_RESULT([found: $CURL_CONFIG])
			fi
		
			dnl Debian stable has currently 7.18.2
			AC_MSG_CHECKING([for curl version >= 7.18.2])
			CURL_VERSION=`$CURL_CONFIG --version | $SED -e 's/[[^0-9\.]]//g'`
			AC_MSG_RESULT([$CURL_VERSION])
			if test `echo $CURL_VERSION | $SED -e 's/[[^0-9]]/ /g' | $AWK '{print $1*10000 + $2*100 + $3}'` -lt 71802; then
				AC_MSG_ERROR([libcurl version greater or equal to 7.18.2 required])
			fi
		
			dnl
			dnl compile tests
			dnl
		
			save_INCLUDES="$INCLUDES"
			INCLUDES=
			save_LIBS="$LIBS"
			LIBS=
			save_CFLAGS="$CFLAGS"
			CFLAGS=`$CURL_CONFIG --cflags`
			save_LDFLAGS="$LDFLAGS"
			LDFLAGS=`$CURL_CONFIG --libs`
			LDFLAGS="$LDFLAGS $ld_runpath_switch$CURL_DIR/$PHP_LIBDIR"
		
			AC_MSG_CHECKING([for SSL support in libcurl])
			CURL_SSL=`$CURL_CONFIG --feature | $EGREP SSL`
			if test "$CURL_SSL" = "SSL"; then
				AC_MSG_RESULT([yes])
				AC_DEFINE([PHP_HTTP_HAVE_SSL], [1], [ ])
			
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
						AC_DEFINE([PHP_HTTP_HAVE_OPENSSL], [1], [ ])
						CURL_SSL="crypto"
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
						AC_DEFINE([PHP_HTTP_HAVE_GNUTLS], [1], [ ])
						CURL_SSL="gcrypt"
					])
				], [
					AC_MSG_RESULT([no])
				], [
					AC_MSG_RESULT([no])
				])
			else
				AC_MSG_RESULT([no])
			fi
		
			INCLUDES="$save_INCLUDES"
			LIBS="$save_LIBS"
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
			if test "x$CURL_CAINFO" = "x"; then
				AC_MSG_RESULT([not found])
			else
				AC_MSG_RESULT([$CURL_CAINFO])
				AC_DEFINE_UNQUOTED([PHP_HTTP_CURL_CAINFO], ["$CURL_CAINFO"], [path to bundled SSL CA info])
			fi
		
			PHP_ADD_INCLUDE($CURL_DIR/include)
			PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
			PHP_EVAL_LIBLINE(`$CURL_CONFIG --libs`, HTTP_SHARED_LIBADD)
			if test "x$CURL_SSL" != "x"; then
				PHP_ADD_LIBRARY_WITH_PATH([$CURL_SSL], $CURL_DIR/$PHP_LIBDIR, PHP_HTTP_SHARED_LIBADD)
			fi
			AC_DEFINE([PHP_HTTP_HAVE_CURL], [1], [Have libcurl support])
			HTTP_HAVE_A_REQUEST_LIB=true
		fi
	fi

dnl ----
dnl EVENT
dnl ----

	if test "$PHP_HTTP_LIBEVENT_DIR" = "no"; then
		AC_DEFINE([PHP_HTTP_HAVE_EVENT], [0], [ ])
	else
		HTTP_HAVE_PHP_EXT([event], [
			AC_MSG_WARN([event support is incompatible with pecl/event; continuing without libevent support])
			AC_DEFINE([PHP_HTTP_HAVE_EVENT], [0], [ ])
		], [
			AC_MSG_CHECKING([for event.h])
			EVENT_DIR=
			for i in "$PHP_HTTP_LIBEVENT_DIR" /usr/local /usr /opt; do
				if test -f "$i/include/event.h"; then
					EVENT_DIR=$i
					break
				fi
			done
			if test "x$EVENT_DIR" = "x"; then
				AC_MSG_RESULT([not found])
				AC_MSG_WARN([continuing without libevent support])
				AC_DEFINE([PHP_HTTP_HAVE_EVENT], [0], [ ])
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
				AC_DEFINE_UNQUOTED([PHP_HTTP_EVENT_VERSION], ["$EVENT_VER"], [ ])
				AC_MSG_RESULT([$EVENT_VER])
				
				PHP_ADD_INCLUDE($EVENT_DIR/include)
				PHP_ADD_LIBRARY_WITH_PATH(event, $EVENT_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
				AC_DEFINE([PHP_HTTP_HAVE_EVENT], [1], [Have libevent support for cURL])
			fi
		])
	fi

PHP_ARG_WITH([http-shared-deps], [whether to depend on extensions which have been built shared],
[  --without-http-shared-deps   HTTP: do not depend on extensions like hash
                                     and iconv (when they're built shared)], $PHP_HTTP, $PHP_HTTP)
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
		if test "x$HTTP_EXT_HASH_INCDIR" = "x"; then
			AC_MSG_RESULT([not found])
		else
			AC_MSG_RESULT([$HTTP_EXT_HASH_INCDIR])
			AC_DEFINE([PHP_HTTP_HAVE_PHP_HASH_H], [1], [Have ext/hash support])
			PHP_ADD_INCLUDE([$HTTP_EXT_HASH_INCDIR])
		fi
	])

dnl ----
dnl ICONV
dnl ----
	HTTP_HAVE_PHP_EXT([iconv])

dnl ----
dnl DONE
dnl ----
	PHP_HTTP_SOURCES="\
		php_http_buffer.c \
		php_http.c \
		php_http_client.c \
		php_http_curl_client.c \
		php_http_client_datashare.c \
		php_http_curl_client_datashare.c \
		php_http_client_factory.c \
		php_http_client_interface.c \
		php_http_client_pool.c \
		php_http_curl_client_pool.c \
		php_http_client_request.c \
		php_http_client_response.c \
		php_http_cookie.c \
		php_http_curl.c \
		php_http_encoding.c \
		php_http_env.c \
		php_http_env_request.c \
		php_http_env_response.c \
		php_http_etag.c \
		php_http_exception.c \
		php_http_filter.c \
		php_http_header_parser.c \
		php_http_headers.c \
		php_http_info.c \
		php_http_message_body.c \
		php_http_message.c \
		php_http_message_parser.c \
		php_http_misc.c \
		php_http_negotiate.c \
		php_http_object.c \
		php_http_params.c \
		php_http_persistent_handle.c \
		php_http_property_proxy.c \
		php_http_querystring.c \
		php_http_resource_factory.c \
		php_http_strlist.c \
		php_http_url.c \
		php_http_version.c \
	"
	PHP_NEW_EXTENSION([http], $PHP_HTTP_SOURCES, $ext_shared)
	
	dnl shared extension deps
	HTTP_SHARED_DEP([hash])
	HTTP_SHARED_DEP([iconv])
	
	PHP_SUBST([HTTP_SHARED_LIBADD])

	PHP_HTTP_HEADERS="
		php_http_api.h \
		php_http_buffer.h \
		php_http_curl_client.h \
		php_http_curl_client_datashare.h \
		php_http_client_datashare.h \
		php_http_client_factory.h \
		php_http_client.h \
		php_http_client_interface.h \
		php_http_curl_client_pool.h \
		php_http_client_pool.h \
		php_http_client_request.h \
		php_http_client_response.h \
		php_http_cookie.h \
		php_http_curl.h \
		php_http_encoding.h \
		php_http_env.h \
		php_http_env_request.h \
		php_http_env_response.h \
		php_http_etag.h \
		php_http_exception.h \
		php_http_filter.h \
		php_http.h \
		php_http_header_parser.h \
		php_http_headers.h \
		php_http_info.h \
		php_http_message_body.h \
		php_http_message.h \
		php_http_message_parser.h \
		php_http_misc.h \
		php_http_negotiate.h \
		php_http_object.h \
		php_http_params.h \
		php_http_persistent_handle.h \
		php_http_property_proxy.h \
		php_http_querystring.h \
		php_http_resource_factory.h \
		php_http_strlist.h \
		php_http_url.h \
		php_http_version.h \
	"
	PHP_INSTALL_HEADERS(ext/http, $PHP_HTTP_HEADERS)

	AC_DEFINE([HAVE_HTTP], [1], [Have extended HTTP support])
fi
