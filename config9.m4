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
PHP_ARG_WITH([http-libidn-dir], [],
[  --with-http-libidn-dir[=DIR]   HTTP: where to find libidn], $PHP_HTTP_LIBCURL_DIR, "")

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

	dnl
	dnl HTTP_CURL_SSL_LIB_CHECK(ssllib[, code-if-yes[, code-if-not])
	dnl
	AC_DEFUN([HTTP_CURL_SSL_LIB_CHECK], [
		AC_MSG_CHECKING([for $1 support in libcurl])
		AC_TRY_RUN([
			#include <curl/curl.h>
			int main(int argc, char *argv[]) {
				curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
				if (data && data->ssl_version && *data->ssl_version) {
					const char *ptr = data->ssl_version;
					while(*ptr == ' ') ++ptr;
					return strncasecmp(ptr, "$1", sizeof("$1")-1);
				}
				return 1;
			}
		], [
			AC_MSG_RESULT([yes])
			$2
		], [
			AC_MSG_RESULT([no])
			$3
		], [
			AC_MSG_RESULT([no])
			$3
		])
	])


dnl ----
dnl STDC
dnl ----
	AC_TYPE_OFF_T
	AC_TYPE_MBSTATE_T
	dnl getdomainname() is declared in netdb.h on some platforms: AIX, OSF
	AC_CHECK_HEADERS([netdb.h unistd.h wchar.h wctype.h arpa/inet.h])
	PHP_CHECK_FUNC(gethostname, nsl)
	PHP_CHECK_FUNC(getdomainname, nsl)
	PHP_CHECK_FUNC(mbrtowc)
	PHP_CHECK_FUNC(mbtowc)
	PHP_CHECK_FUNC(iswalnum)
	PHP_CHECK_FUNC(inet_pton)

dnl ----
dnl IDN
dnl ----

	AC_MSG_CHECKING([for idna.h])
	IDNA_DIR=
	for i in "$PHP_HTTP_LIBIDN_DIR" "$IDN_DIR" /usr/local /usr /opt; do
		if test -f "$i/include/idna.h"; then
			IDNA_DIR=$i
			break;
		fi
	done
	if test "x$IDNA_DIR" != "x"; then
		AC_MSG_RESULT([found in $IDNA_DIR])
		AC_DEFINE([PHP_HTTP_HAVE_IDN], [1], [Have libidn support])
		PHP_ADD_INCLUDE($IDNA_DIR/include)
		PHP_ADD_LIBRARY_WITH_PATH(idn, $IDNA_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
		AC_MSG_CHECKING([for libidn version])
		IDNA_VER=$(pkg-config --version libidn 2>/dev/null || echo unknown)
		AC_MSG_RESULT([$IDNA_VER])
		AC_DEFINE_UNQUOTED([PHP_HTTP_LIBIDN_VERSION], "$IDNA_VER", [ ])
	else
		AC_MSG_RESULT([not found])
		AC_MSG_CHECKING([for idn2.h])
		IDNA_DIR=
		for i in "$PHP_HTTP_LIBIDN_DIR" "$IDN_DIR" /usr/local /usr /opt; do
			if test -f "$i/include/idn2.h"; then
				IDNA_DIR=$i
				break;
			fi
		done
		if test "x$IDNA_DIR" != "x"; then
			AC_MSG_RESULT([found in $IDNA_DIR])
			AC_DEFINE([PHP_HTTP_HAVE_IDN2], [1], [Have libidn2 support])
			PHP_ADD_INCLUDE($IDNA_DIR/include)
			PHP_ADD_LIBRARY_WITH_PATH(idn2, $IDNA_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
			AC_MSG_CHECKING([for libidn2 version])
			IDNA_VER=`$EGREP "define IDN2_VERSION " $IDNA_DIR/include/idn2.h | $SED -e's/^.*VERSION //g' -e 's/[[^0-9\.]]//g'`
			AC_MSG_RESULT([$IDNA_VER])
			AC_DEFINE_UNQUOTED([PHP_HTTP_LIBIDN2_VERSION], "$IDNA_VER", [ ])
		else
			AC_MSG_RESULT([not found])
			AC_CHECK_HEADERS([unicode/uidna.h])
			case $host_os in
			darwin*)
				PHP_CHECK_FUNC(uidna_IDNToASCII, icucore);;
			*)
				AC_PATH_PROG(ICU_CONFIG, icu-config, no, [$PATH:/usr/local/bin])
				if test ! -x "$ICU_CONFIG"; then
					ICU_CONFIG="icu-config"
				fi
				AC_MSG_CHECKING([for uidna_IDNToASCII])
				if ! test -x "$ICU_CONFIG"; then
					ICU_CONFIG=icu-config
				fi
				if $ICU_CONFIG --exists 2>/dev/null >&2; then
					save_LIBS=$LIBS
					LIBS=$($ICU_CONFIG --ldflags)
					AC_TRY_RUN([
						#include <unicode/uidna.h>
						int main(int argc, char *argv[]) {
							return uidna_IDNToASCII(0, 0, 0, 0, 0, 0, 0);
						}
					], [
						AC_MSG_RESULT([yes])
						AC_DEFINE([HAVE_UIDNA_IDNTOASCII], [1], [ ])
						LIBS=$save_LIBS
						PHP_EVAL_LIBLINE(`$ICU_CONFIG --ldflags`, HTTP_SHARED_LIBADD)
					], [
						LIBS=$save_LIBS
						AC_MSG_RESULT([no])
					])
				fi
				;;
			esac
		fi
	fi

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

			dnl RHEL6:         7.19.7
			dnl SUSE11:        7.19.7
			dnl Debian wheezy: 7.26.0
			dnl Debian sqeeze: 7.21.0
			dnl Debian ancient 7.18.2
			AC_MSG_CHECKING([for curl version >= 7.18.2])
			CURL_VERSION=`$CURL_CONFIG --version | $SED -e 's/[[^0-9\.]]//g'`
			AC_MSG_RESULT([$CURL_VERSION])
			if test `echo $CURL_VERSION | $SED -e 's/[[^0-9]]/ /g' | $AWK '{print $1*10000 + $2*100 + $3}'` -lt 71802; then
				AC_MSG_ERROR([libcurl version greater or equal to 7.18.2 required])
			fi

			AC_MSG_CHECKING([for HTTP2 support in libcurl])
			if $CURL_CONFIG --features | $EGREP -q HTTP2; then
				AC_MSG_RESULT([yes])
				AC_DEFINE([PHP_HTTP_HAVE_HTTP2], [1], [ ])
			else
				AC_MSG_RESULT([no])
			fi

			dnl
			dnl compile tests
			dnl

			save_INCLUDES="$INCLUDES"
			INCLUDES=
			save_LIBS="$LIBS"
			LIBS=-lcurl
			save_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS `$CURL_CONFIG --cflags`"
			save_LDFLAGS="$LDFLAGS"
			LDFLAGS="$ld_runpath_switch$CURL_DIR/$PHP_LIBDIR"

			AC_MSG_CHECKING([for SSL support in libcurl])
			CURL_SSL=`$CURL_CONFIG --feature | $EGREP SSL`
			CURL_SSL_LIBS=""
			if test "$CURL_SSL" = "SSL"; then
				AC_MSG_RESULT([yes])
				AC_DEFINE([PHP_HTTP_HAVE_SSL], [1], [ ])

				HTTP_CURL_SSL_LIB_CHECK(OpenSSL, [
					AC_CHECK_HEADER([openssl/ssl.h], [
						AC_CHECK_HEADER([openssl/crypto.h], [
							AC_DEFINE([PHP_HTTP_HAVE_OPENSSL], [1], [ ])
							CURL_SSL_LIBS="ssl crypto"
						])
					])
				])
				HTTP_CURL_SSL_LIB_CHECK(GnuTLS, [
					AC_CHECK_HEADER([gnutls.h], [
						AC_CHECK_HEADER([gcrypt.h], [
							AC_DEFINE([PHP_HTTP_HAVE_GNUTLS], [1], [ ])
							CURL_SSL_LIBS="gnutls gcrypt"
						])
					])
				])
				HTTP_CURL_SSL_LIB_CHECK(NSS, [
					AC_DEFINE([PHP_HTTP_HAVE_NSS], [1], [ ])
				])
				HTTP_CURL_SSL_LIB_CHECK(SecureTransport, [
					AC_DEFINE([PHP_HTTP_HAVE_DARWINSSL], [1], [ ])
				])
				HTTP_CURL_SSL_LIB_CHECK(GSKit, [
					AC_DEFINE([PHP_HTTP_HAVE_GSKIT], [1], [ ])
				])
			else
				dnl no CURL_SSL
				AC_MSG_RESULT([no])
			fi

			AC_MSG_CHECKING([for ares support in libcurl])
			AC_TRY_RUN([
				#include <curl/curl.h>
				int main(int argc, char *argv[]) {
					curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
					if (data && data->ares && data->ares_num0) {
						return 0;
					}
					return 1;
				}
			], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([PHP_HTTP_HAVE_ARES], [1], [ ])
			], [
				AC_MSG_RESULT([no])
			], [
				AC_MSG_RESULT([no])
			])

			AC_MSG_CHECKING([whether CURLOPT_TLSAUTH_TYPE expects CURL_TLSAUTH_SRP or literal "SRP"])
			AC_TRY_RUN([
				#include <curl/curl.h>
				int main(int argc, char *argv[]) {
					CURL *ch = curl_easy_init();
					return curl_easy_setopt(ch, CURLOPT_TLSAUTH_TYPE, CURL_TLSAUTH_SRP);
				}
			], [
				AC_MSG_RESULT([CURL_TLSAUTH_SRP])
				AC_DEFINE([PHP_HTTP_CURL_TLSAUTH_SRP], [CURL_TLSAUTH_SRP], [ ])
				AC_DEFINE([PHP_HTTP_CURL_TLSAUTH_DEF], [CURL_TLSAUTH_NONE], [ ])
			], [
				AC_TRY_RUN([
					#include <curl/curl.h>
					int main(int argc, char *argv[]) {
						CURL *ch = curl_easy_init();
						return curl_easy_setopt(ch, CURLOPT_TLSAUTH_TYPE, "SRP");
					}
				], [
					AC_MSG_RESULT(["SRP"])
					AC_DEFINE([PHP_HTTP_CURL_TLSAUTH_SRP], ["SRP"], [ ])
					AC_DEFINE([PHP_HTTP_CURL_TLSAUTH_DEF], [""], [ ])
				], [
					AC_MSG_RESULT([neither])
				], [
					AC_MSG_RESULT([neither])
				])
			], [
				AC_MSG_RESULT([neither])
			])

			INCLUDES="$save_INCLUDES"
			LIBS="$save_LIBS"
			CFLAGS="$save_CFLAGS"
			LDFLAGS="$save_LDFLAGS"

			if test -n "$CURL_SSL_LIBS"; then
				for CURL_SSL_LIB in $CURL_SSL_LIBS; do
					PHP_ADD_LIBRARY_WITH_PATH([$CURL_SSL_LIB], $CURL_DIR/$PHP_LIBDIR, PHP_HTTP_SHARED_LIBADD)
				done
			fi

			dnl end compile tests

			AC_MSG_CHECKING([for default SSL CA info/path])
			CURL_CA_PATH=
			CURL_CA_INFO=
			CURL_CONFIG_CA=$($CURL_CONFIG --ca)
			if test -z "$CURL_CONFIG_CA"; then
				CURL_CONFIG_CA=$($CURL_CONFIG --configure  | $EGREP -o -- "--with-ca@<:@^'@:>@*" | $SED 's/.*=//')
			fi
			for i in \
				"$CURL_CONFIG_CA" \
				/etc/ssl/certs \
				/etc/ssl/certs/ca-bundle.crt \
				/etc/ssl/certs/ca-certificates.crt \
				/etc/pki/tls/certs/ca-bundle.crt \
				/etc/pki/tls/certs/ca-bundle.trust.crt \
				/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem \
				/System/Library/OpenSSL
			do
				if test -z "$CURL_CA_PATH" && test -d "$i"; then
					# check if it's actually a hashed directory
					if ls "$i"/@<:@0-9a-f@:>@@<:@0-9a-f@:>@@<:@0-9a-f@:>@@<:@0-9a-f@:>@@<:@0-9a-f@:>@@<:@0-9a-f@:>@@<:@0-9a-f@:>@@<:@0-9a-f@:>@.0 >/dev/null 2>&1; then
						CURL_CA_PATH="$i"
					fi
				elif test -z "$CURL_CA_INFO" && test -f "$i"; then
					CURL_CA_INFO="$i"
				fi
			done
			if test -n "$CURL_CA_PATH" && test -n "$CURL_CA_INFO"; then
				AC_MSG_RESULT([path:$CURL_CA_PATH, info:$CURL_CA_INFO])
				AC_DEFINE_UNQUOTED([PHP_HTTP_CURL_CAPATH], ["$CURL_CA_PATH"], [path to default SSL CA path])
				AC_DEFINE_UNQUOTED([PHP_HTTP_CURL_CAINFO], ["$CURL_CA_INFO"], [path to default SSL CA info])
			elif test -n "$CURL_CA_INFO"; then
				AC_MSG_RESULT([info:$CURL_CA_INFO])
				AC_DEFINE_UNQUOTED([PHP_HTTP_CURL_CAINFO], ["$CURL_CA_INFO"], [path to default SSL CA info])
			elif test -n "$CURL_CA_PATH"; then
				AC_MSG_RESULT([path:$CURL_CA_PATH])
				AC_DEFINE_UNQUOTED([PHP_HTTP_CURL_CAPATH], ["$CURL_CA_PATH"], [path to default SSL CA path])
			else
				AC_MSG_RESULT([none])
			fi

			PHP_ADD_INCLUDE($CURL_DIR/include)
			PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
			PHP_EVAL_LIBLINE(`$CURL_CONFIG --libs`, HTTP_SHARED_LIBADD)
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
		AC_MSG_CHECKING([for event2/event.h])
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

			if test -f "$EVENT_DIR/include/event2/event.h"; then
				EVENT_VER="`$AWK '/_EVENT_VERSION/ {gsub(/\"/,\"\",$3); print $3}' < $EVENT_DIR/include/event2/event-config.h`"
				AC_DEFINE([PHP_HTTP_HAVE_EVENT2], [1], [ ])
			else
				AC_DEFINE([PHP_HTTP_HAVE_EVENT2], [0], [ ])
				if test -f "$EVENT_DIR/include/evhttp.h" && test -f "$EVENT_DIR/include/evdns.h"; then
					if test -f "$EVENT_DIR/include/evrpc.h"; then
						EVENT_VER="1.4 or greater"
					else
						EVENT_VER="1.2 or greater"
					fi
				else
					EVENT_VER="1.1b or lower"
				fi
			fi
			AC_DEFINE_UNQUOTED([PHP_HTTP_EVENT_VERSION], ["$EVENT_VER"], [ ])
			AC_MSG_RESULT([$EVENT_VER])

			PHP_ADD_INCLUDE($EVENT_DIR/include)
			PHP_ADD_LIBRARY_WITH_PATH(event, $EVENT_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
			AC_DEFINE([PHP_HTTP_HAVE_EVENT], [1], [Have libevent support for cURL])
		fi
	fi

dnl ----
dnl RAPHF
dnl ----
	HTTP_HAVE_PHP_EXT([raphf], [
		AC_MSG_CHECKING([for php_raphf.h])
		HTTP_EXT_RAPHF_INCDIR=
		for i in `echo $INCLUDES | $SED -e's/-I//g'` $abs_srcdir ../raphf; do
			if test -d $i; then
				if test -f $i/php_raphf.h; then
					HTTP_EXT_RAPHF_INCDIR=$i
					break
				elif test -f $i/ext/raphf/php_raphf.h; then
					HTTP_EXT_RAPHF_INCDIR=$i/ext/raphf
					break
				fi
			fi
		done
		if test "x$HTTP_EXT_RAPHF_INCDIR" = "x"; then
			AC_MSG_ERROR([not found])
		else
			AC_MSG_RESULT([$HTTP_EXT_RAPHF_INCDIR])
			AC_DEFINE([PHP_HTTP_HAVE_PHP_RAPHF_H], [1], [Have ext/raphf support])
			PHP_ADD_INCLUDE([$HTTP_EXT_RAPHF_INCDIR])
		fi
	], [
		AC_MSG_ERROR([Please install pecl/raphf and activate extension=raphf.$SHLIB_DL_SUFFIX_NAME in your php.ini])
	])

dnl ----
dnl PROPRO
dnl ----
	HTTP_HAVE_PHP_EXT([propro], [
		AC_MSG_CHECKING([for php_propro.h])
		HTTP_EXT_PROPRO_INCDIR=
		for i in `echo $INCLUDES | $SED -e's/-I//g'` $abs_srcdir ../propro; do
			if test -d $i; then
				if test -f $i/php_propro.h; then
					HTTP_EXT_PROPRO_INCDIR=$i
					break
				elif test -f $i/ext/propro/php_propro.h; then
					HTTP_EXT_PROPRO_INCDIR=$i/ext/propro
					break
				fi
			fi
		done
		if test "x$HTTP_EXT_PROPRO_INCDIR" = "x"; then
			AC_MSG_ERROR([not found])
		else
			AC_MSG_RESULT([$HTTP_EXT_PROPRO_INCDIR])
			AC_DEFINE([PHP_HTTP_HAVE_PHP_PROPRO_H], [1], [Have ext/propro support])
			PHP_ADD_INCLUDE([$HTTP_EXT_PROPRO_INCDIR])
		fi
	], [
		AC_MSG_ERROR([Please install pecl/propro and activate extension=propro.$SHLIB_DL_SUFFIX_NAME in your php.ini])
	])

PHP_ARG_WITH([http-shared-deps], [whether to depend on extensions which have been built shared],
[  --without-http-shared-deps   HTTP: do not depend on extensions like hash
                                     and iconv (when they are built shared)], $PHP_HTTP, $PHP_HTTP)
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

	PHP_SUBST([HTTP_SHARED_LIBADD])

	PHP_HTTP_SRCDIR=PHP_EXT_SRCDIR(http)
	PHP_HTTP_BUILDDIR=PHP_EXT_BUILDDIR(http)

	PHP_ADD_INCLUDE($PHP_HTTP_SRCDIR/src)
	PHP_ADD_BUILD_DIR($PHP_HTTP_BUILDDIR/src)

	PHP_HTTP_HEADERS=`(cd $PHP_HTTP_SRCDIR/src && echo *.h)`
	PHP_HTTP_SOURCES=`(cd $PHP_HTTP_SRCDIR && echo src/*.c)`

	PHP_NEW_EXTENSION(http, $PHP_HTTP_SOURCES, $ext_shared)
	PHP_INSTALL_HEADERS(ext/http, php_http.h $PHP_HTTP_HEADERS)

	HTTP_SHARED_DEP([hash])
	HTTP_SHARED_DEP([iconv])
	PHP_ADD_EXTENSION_DEP([http], [raphf], true)
	PHP_ADD_EXTENSION_DEP([http], [propro], true)

	PHP_SUBST(PHP_HTTP_HEADERS)
	PHP_SUBST(PHP_HTTP_SOURCES)

	PHP_SUBST(PHP_HTTP_SRCDIR)
	PHP_SUBST(PHP_HTTP_BUILDDIR)

	PHP_ADD_MAKEFILE_FRAGMENT

	AC_DEFINE([HAVE_HTTP], [1], [Have extended HTTP support])
fi
