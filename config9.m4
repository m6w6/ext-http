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
PHP_ARG_WITH([http-libidn2-dir], [],
[  --with-http-libidn-dir[=DIR]   HTTP: where to find libidn2], $PHP_HTTP_LIBCURL_DIR, "")
PHP_ARG_WITH([http-libicu-dir], [],
[  --with-http-libidn-dir[=DIR]   HTTP: where to find libicu], $PHP_HTTP_LIBCURL_DIR, "")

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

	if test -z "$PKG_CONFIG"; then
		AC_PATH_PROG(PKG_CONFIG, pkg-config, false)
	fi

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
	dnl HTTP_HAVE_VERSION(name, min-version[, code-if-yes[, code-if-not]])
	dnl
	AC_DEFUN([HTTP_HAVE_VERSION], [
		aversion=$(printf "%s" "$AS_TR_CPP([PHP_HTTP_]$1[_VERSION])" | $AWK '{print $[]1*1000000 + $[]2*10000 + $[]3*100 + $[]4}')
		mversion=$(printf "%s" "$2" | $AWK '{print $[]1*1000000 + $[]2*10000 + $[]3*100 + $[]4}')
		AC_MSG_CHECKING([whether $1 version $]AS_TR_CPP([PHP_HTTP_]$1[_VERSION])[ is at least $2])
		if test "$aversion" -lt "$mversion"; then
			ifelse($4,,AC_MSG_ERROR([$1 minimum version $mversion required; got $aversion]), [
				AC_MSG_RESULT([no])
				$4
			])
		else
			AC_MSG_RESULT([ok])
			$3
		fi
	])

	dnl
	dnl HTTP_CHECK_CUSTOM(name, path, header, lib, version)
	dnl
	AC_DEFUN([HTTP_CHECK_CUSTOM], [
		save_CPPFLAGS=$CPPFLAGS
		save_LDFLAGS=$LDFLAGS
		save_LIBS=$LIBS

		for path in $2 /usr/local /usr /opt; do
			if test "$path" = "" || test "$path" = "yes" || test "$path" = "no"; then
				continue
			fi
			AC_MSG_CHECKING([for $1 in $path])
			if test -f "$path/include/$3"; then
				CPPFLAGS="-I$path"
				LDFLAGS="-L$path"
				LIBS="-l$4"

				AS_TR_CPP([PHP_HTTP_][$1][_VERSION])=$5
				AC_MSG_RESULT([${AS_TR_CPP([PHP_HTTP_][$1][_VERSION]):-no}])
				AC_DEFINE_UNQUOTED(AS_TR_CPP([PHP_HTTP_][$1][_VERSION]), "$AS_TR_CPP([PHP_HTTP_][$1][_VERSION])", [ ])
				break
			fi
			AC_MSG_RESULT([no])
		done
	])

	dnl
	dnl HTTP_CHECK_CONFIG(name, prog-config, version-flag, cppflags-flag, ldflags-flag, libs-flag)
	dnl
	AC_DEFUN([HTTP_CHECK_CONFIG], [
		AC_MSG_CHECKING([for $1])

		save_CPPFLAGS=$CPPFLAGS
		save_LDFLAGS=$LDFLAGS
		save_LIBS=$LIBS
		AS_TR_CPP([PHP_HTTP_][$1][_VERSION])=$($2 $3)
		CPPFLAGS=$($2 $4)
		LDFLAGS=$($2 $5)
		LIBS=$($2 $6)

		AC_MSG_RESULT([${AS_TR_CPP([PHP_HTTP_][$1][_VERSION]):-no}])
		AC_DEFINE_UNQUOTED(AS_TR_CPP([PHP_HTTP_][$1][_VERSION]), "$AS_TR_CPP([PHP_HTTP_][$1][_VERSION])", [ ])
	])

	dnl
	dnl HTTP_CHECK_PKGCONFIG(pkg[, pkg_config_path])
	dnl
	AC_DEFUN([HTTP_CHECK_PKGCONFIG], [
		ifelse($2,,,PKG_CONFIG_PATH="$2/lib/pkgconfig:$PKG_CONFIG_PATH")
		if $($PKG_CONFIG $1 --exists); then
			AS_TR_CPP([PHP_HTTP_HAVE_$1])=true
			HTTP_CHECK_CONFIG([$1], [$PKG_CONFIG $1], [--modversion], [--cflags-only-I], [--libs-only-L], [--libs-only-l])
		else
			AS_TR_CPP([PHP_HTTP_HAVE_$1])=false
		fi
	])

	dnl
	dnl HTTP_CHECK_DONE(name, success[, incline, libline])
	AC_DEFUN([HTTP_CHECK_DONE], [
		if $2; then
			incline=$CPPFLAGS
			libline="$LDFLAGS $LIBS"
			AC_DEFINE(AS_TR_CPP([PHP_HTTP_HAVE_$1]), [1], [ ])
		else
			incline=$3
			libline=$4
		fi

		CPPFLAGS=$save_CPPFLAGS
		LDFLAGS=$save_LDFLAGS
		LIBS=$save_LIBS

		PHP_EVAL_INCLINE([$incline])
		PHP_EVAL_LIBLINE([$libline], HTTP_SHARED_LIBLINE)
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

	HTTP_CHECK_PKGCONFIG(libidn, [$PHP_HTTP_LIBIDN_DIR])
	if $PHP_HTTP_HAVE_LIBIDN; then
		AC_DEFINE([PHP_HTTP_HAVE_IDNA2003], [1], [ ])
	fi
	HTTP_CHECK_DONE(libidn, $PHP_HTTP_HAVE_LIBIDN)

	HTTP_CHECK_CUSTOM(libidn2, "$PHP_HTTP_LIBIDN2_DIR", idn2.h, idn2,
		[$($EGREP "define IDN2_VERSION " $path/include/idn2.h | $SED -e's/^.*VERSION //g' -e 's/@<:@^0-9\.@:>@//g')])
	if test -n "$PHP_HTTP_LIBIDN2_VERSION"; then
		AC_DEFINE([PHP_HTTP_HAVE_IDNA2008], [1], [ ])
	fi
	HTTP_CHECK_DONE(libidn2, test -n "$PHP_HTTP_LIBIDN2_VERSION")

	case $host_os in
	darwin*)
		PHP_CHECK_FUNC(uidna_IDNToASCII, icucore)
		;;
	*)
		AC_PATH_PROG(ICU_CONFIG, icu-config, false, [$PHP_HTTP_LIBICU_DIR/bin:$PATH:/usr/local/bin])

		HTTP_CHECK_CONFIG(libicu, [$ICU_CONFIG], [--version], [--cppflags], [--ldflags-searchpath], [--ldflags-libsonly])
		AC_MSG_CHECKING([for uidna_IDNToASCII])
		AC_TRY_LINK([
			#include <unicode/uidna.h>
		], [
			uidna_IDNToASCII(0, 0, 0, 0, 0, 0, 0);
		], [
			AC_MSG_RESULT([yes])
			PHP_HTTP_HAVE_ICU=true
			AC_DEFINE([PHP_HTTP_HAVE_IDNA2003], [1], [ ])
			AC_DEFINE([HAVE_UIDNA_IDNTOASCII], [1], [ ])
		], [
			AC_MSG_RESULT([no])
		])
		AC_MSG_CHECKING([for uidna_nameToASCII_UTF8])
		AC_TRY_LINK([
			#include <unicode/uidna.h>
		], [
			uidna_nameToASCII_UTF8(0, 0, 0, 0, 0, 0, 0);
		], [
			AC_MSG_RESULT([yes])
			PHP_HTTP_HAVE_ICU=true
			AC_DEFINE([PHP_HTTP_HAVE_IDNA2008], [1], [ ])
			AC_DEFINE([HAVE_UIDNA_NAMETOASCII_UTF8], [1], [ ])
		], [
			AC_MSG_RESULT([no])
		])
		HTTP_CHECK_DONE(libicu, [$PHP_HTTP_HAVE_LIBICU])
		;;
	esac

dnl ----
dnl ZLIB
dnl ----
	HTTP_CHECK_CUSTOM(zlib, ["$PHP_HTTP_ZLIB_DIR" "$PHP_ZLIB_DIR" "$PHP_ZLIB"], zlib.h, z,
		[$($EGREP "define ZLIB_VERSION" "$path/include/zlib.h" | $SED -e 's/@<:@^0-9\.@:>@//g')])
	HTTP_HAVE_VERSION(zlib, 1.2.0.4)
	HTTP_CHECK_DONE(zlib, test -n "$PHP_HTTP_ZLIB_VERSION")

dnl ----
dnl CURL
dnl ----

	AC_PATH_PROG([CURL_CONFIG], [curl-config], false, [$PHP_HTTP_LIBCURL_DIR/bin:$PATH:/usr/local/bin])

	if $CURL_CONFIG --protocols | $EGREP -q HTTP; then
		HTTP_CHECK_CONFIG(libcurl, $CURL_CONFIG,
			[--version | $SED -e 's/@<:@^0-9\.@:>@//g'],
			[--cflags],
			[--libs | $EGREP -o -- '-L@<:@^ @:>@* ?'],
			[--libs | $EGREP -o -- '-l@<:@^ @:>@* ?']
		)
		HTTP_HAVE_VERSION(libcurl, 7.18.2)

		AC_MSG_CHECKING([for HTTP2 support in libcurl])
		if $CURL_CONFIG --feature | $EGREP -q HTTP2; then
			AC_MSG_RESULT([yes])
			AC_DEFINE([PHP_HTTP_HAVE_HTTP2], [1], [ ])
		else
			AC_MSG_RESULT([no])
		fi

		AC_MSG_CHECKING([for SSL support in libcurl])
		if $CURL_CONFIG --feature | $EGREP -q SSL; then
			AC_MSG_RESULT([yes])
			AC_DEFINE([PHP_HTTP_HAVE_SSL], [1], [ ])

			HTTP_CURL_SSL_LIB_CHECK(OpenSSL, [
				AC_CHECK_HEADER([openssl/ssl.h], [
					AC_CHECK_HEADER([openssl/crypto.h], [
						AC_DEFINE([PHP_HTTP_HAVE_OPENSSL], [1], [ ])
						LIBS="$LIBS -lssl -lcrypto"
					])
				])
			])
			HTTP_CURL_SSL_LIB_CHECK(GnuTLS, [
				AC_CHECK_HEADER([gnutls.h], [
					AC_CHECK_HEADER([gcrypt.h], [
						AC_DEFINE([PHP_HTTP_HAVE_GNUTLS], [1], [ ])
						LIBS="$LIBS -lgnutls -lgcrypt"
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

		HTTP_HAVE_A_REQUEST_LIB=true
	fi
	HTTP_CHECK_DONE(libcurl, test -n "$PHP_HTTP_LIBCURL_VERSION")

dnl ----
dnl EVENT
dnl ----

	HTTP_CHECK_PKGCONFIG(libevent, [$PHP_HTTP_LIBEVENT_DIR])
	HTTP_HAVE_VERSION(libevent, 2.0, [
		AC_DEFINE([PHP_HTTP_HAVE_LIBEVENT2], [1], [ ])
	])
	HTTP_CHECK_DONE(libevent, [$PHP_HTTP_HAVE_LIBEVENT])

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
	if $HTTP_HAVE_A_REQUEST_LIB; then
		AC_DEFINE([PHP_HTTP_HAVE_CLIENT], [1], [Have HTTP client support])
	fi
fi
