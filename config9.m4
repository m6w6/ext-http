
m4_foreach(dir, [., ext/http], [
	sinclude(dir/autoconf/pecl/pecl.m4)
	sinclude(dir/autoconf/pecl/zlib.m4)
	sinclude(dir/autoconf/pecl/libbrotli.m4)
	sinclude(dir/autoconf/pecl/libcurl.m4)
	sinclude(dir/autoconf/pecl/libevent.m4)
])

PECL_INIT([http])

PHP_ARG_WITH([http], [whether to enable extended HTTP support],
[  --with-http             Enable extended HTTP support])
if test "$PHP_HTTP" != "no"; then
	dnl STDC
	AC_TYPE_OFF_T
	AC_TYPE_MBSTATE_T
	dnl getdomainname() is declared in netdb.h on some platforms: AIX, OSF
	AC_CHECK_HEADERS([netdb.h unistd.h wchar.h wctype.h arpa/inet.h])
	AC_CHECK_FUNC(gethostname,,[
		AC_CHECK_LIB(nsl, gethostname)
	])
	AC_CHECK_FUNC(getdomainname,,[
		AC_CHECK_LIB(nsl, getdomainname)
	])
	AC_CHECK_FUNCS(mbrtowc mbtowc iswalnum inet_pton)

	dnl ZLIB
	PHP_ARG_WITH([http-zlib-dir], [whether/where to check for zlib],
	[  --with-http-zlib-dir[=DIR]         HTTP: where to find zlib], $PHP_HTTP, no)
	PECL_CHECK_ZLIB([$PHP_HTTP_ZLIB_DIR], [1.2.0.4])
	PECL_CHECK_DONE(zlib, $PECL_VAR([HAVE_ZLIB]))
	
	dnl BROTLI
	PHP_ARG_WITH([http-libbrotli-dir], [whether/where to check for libbrotli],
	[  --with-http-libbrotli-dir[=DIR]    HTTP: where to find libbrotli], $PHP_HTTP, no)
	PECL_CHECK_LIBBROTLI([$PHP_HTTP_LIBBROTLI_DIR], [1.0])
	PECL_CHECK_DONE(libbrotli, $PECL_VAR([HAVE_LIBBROTLI]))

	dnl CURL
	PHP_ARG_WITH([http-libcurl-dir], [whether/where to check for libcurl],
	[  --with-http-libcurl-dir[=DIR]      HTTP: where to find libcurl], $PHP_HTTP, no)
	if test "$PHP_HTTP_LIBCURL_DIR" != "no"; then
		PECL_CHECK_LIBCURL([$PHP_HTTP_LIBCURL_DIR], [7.18.2])
		PECL_HAVE_LIBCURL_PROTOCOL([HTTP], [
			PECL_HAVE_LIBCURL_FEATURE([HTTP2])
			PECL_HAVE_LIBCURL_ARES
			PECL_HAVE_LIBCURL_SSL
			PECL_HAVE_LIBCURL_CA
			PECL_DEFINE([HAVE_CLIENT])
		])
		PECL_CHECK_DONE(libcurl, [$PECL_VAR([HAVE_LIBCURL_PROTOCOL_HTTP])])
	fi

	dnl EVENT
	PHP_ARG_WITH([http-libevent-dir], [whether/where to check for libevent],
	[  --with-http-libevent-dir[=DIR]     HTTP: where to find libevent], $PHP_HTTP_LIBCURL_DIR, no)
	if test "$PHP_HTTP_LIBEVENT_DIR" != "no"; then
		PECL_CHECK_LIBEVENT([$PHP_HTTP_LIBEVENT_DIR])
		PECL_CHECK_DONE(libevent, [$PECL_VAR([HAVE_LIBEVENT])])
	fi

	dnl GNU IDNA
	PHP_ARG_WITH([http-libidn2-dir], [whether/where to check for libidn2],
	[  --with-http-libidn2-dir[=DIR]      HTTP: where to find libidn2], $PHP_HTTP_LIBCURL_DIR, no)
	if test "$PHP_HTTP_LIBIDN2_DIR" != "no"; then
		PECL_CHECK_CUSTOM(libidn2, "$PHP_HTTP_LIBIDN2_DIR", idn2.h, idn2,
			[$($EGREP "define IDN2_VERSION " "include/idn2.h" | $SED -e's/^.*VERSION //g' -e 's/@<:@^0-9\.@:>@//g')])
		if $PECL_VAR([HAVE_LIBIDN2]); then
			PECL_DEFINE([HAVE_IDNA2008])
		fi
		PECL_CHECK_DONE(libidn2, $PECL_VAR([HAVE_LIBIDN2]))
	fi
	PHP_ARG_WITH([http-libidn-dir], [whether/where to check for libidn],
	[  --with-http-libidn-dir[=DIR]       HTTP: where to find libidn], $PHP_HTTP_LIBCURL_DIR, no)
	if test "$PHP_HTTP_LIBIDN_DIR" != "no"; then
		PECL_CHECK_PKGCONFIG(libidn, [$PHP_HTTP_LIBIDN_DIR])
		if $PECL_VAR([HAVE_LIBIDN]); then
			PECL_DEFINE([HAVE_IDNA2003])
		fi
		PECL_CHECK_DONE(libidn, $PECL_VAR([HAVE_LIBIDN]))
	fi

	dnl ICU IDNA
	PHP_ARG_WITH([http-libicu-dir], [whether/where to check for libicu],
	[  --with-http-libicu-dir[=DIR]       HTTP: where to find libicu], $PHP_HTTP_LIBCURL_DIR, no)
	if test "$PHP_HTTP_LIBICU_DIR" != "no"; then
		AC_PATH_PROG(ICU_CONFIG, icu-config, false, [$PHP_HTTP_LIBICU_DIR/bin:$PATH:/usr/local/bin])

		PECL_CHECK_CONFIG(libicu, [$ICU_CONFIG], [--version], [--cppflags], [--ldflags-searchpath], [--ldflags-libsonly])
		AC_CACHE_CHECK([for uidna_IDNToASCII], PECL_CACHE_VAR([HAVE_UIDNA_IDNToASCII]), [
			if printf "%s" "$CFLAGS" | $EGREP -q "(^|\s)-Werror\b"; then
				if ! printf "%s" "$CFLAGS" | $EGREP -q "(^|\s)-Wno-error=deprecated-declarations\b"; then
					CFLAGS="$CFLAGS -Wno-error=deprecated-declarations"
				fi
			fi
			AC_TRY_LINK([
				#include <unicode/uidna.h>
			], [
				uidna_IDNToASCII(0, 0, 0, 0, 0, 0, 0);
			], [
				PECL_CACHE_VAR([HAVE_UIDNA_IDNToASCII])=yes
			], [
				PECL_CACHE_VAR([HAVE_UIDNA_IDNToASCII])=no
			])
		])
		if $PECL_CACHE_VAR([HAVE_UIDNA_IDNTOASCII]); then
			PECL_DEFINE([HAVE_IDNA2003])
			PECL_DEFINE_FN([UIDNA_IDNTOASCII])
		fi

		AC_CACHE_CHECK([for uidna_nameToASCII_UTF8], PECL_CACHE_VAR([HAVE_UIDNA_NAMETOASCII_UTF8]), [
			AC_TRY_LINK([
				#include <unicode/uidna.h>
			], [
				uidna_nameToASCII_UTF8(0, 0, 0, 0, 0, 0, 0);
			], [
				PECL_CACHE_VAR([HAVE_UIDNA_NAMETOASCII_UTF8])=yes
			], [
				PECL_CACHE_VAR([HAVE_UIDNA_NAMETOASCII_UTF8])=no
			])
		])
		if $PECL_CACHE_VAR([HAVE_UIDNA_NAMETOASCII_UTF8]); then
			PECL_DEFINE([HAVE_IDNA2008])
			PECL_DEFINE_FN([uidna_nameToASCII_UTF8])
		fi
		PECL_CHECK_DONE(libicu, [$PECL_VAR([HAVE_LIBICU])])
	fi

	dnl IDNKIT2
	PHP_ARG_WITH([http-libidnkit2-dir], [whether/where to check for libidnkit2],
	[  --with-http-libidnkit2-dir[=DIR]   HTTP: where to find libidnkit2], $PHP_HTTP_LIBCURL_DIR, no)
	if test "$PHP_HTTP_LIBIDNKIT2_DIR" != "no"; then
		PECL_CHECK_CUSTOM(libidnkit2, "$PHP_HTTP_LIBIDNKIT2_DIR", idn/api.h, idnkit,
			[$($EGREP "define IDNKIT_VERSION_LIBIDN\b" "include/idn/version.h" | $SED -e's/^.*VERSION_LIBIDN//g' -e 's/@<:@^0-9\.@:>@//g')])
		if $PECL_VAR([HAVE_LIBIDNKIT2]); then
			PECL_DEFINE([HAVE_IDNA2008])
		fi
		PECL_CHECK_DONE(libidnkit2, [$PECL_VAR([HAVE_LIBIDNKIT2])])
	fi
	dnl IDNKIT1
	PHP_ARG_WITH([http-libidnkit-dir], [whether/where to check for libidnkit],
	[  --with-http-libidnkit-dir[=DIR]    HTTP: where to find libidnkit], $PHP_HTTP_LIBCURL_DIR, no)
	if test "$PHP_HTTP_LIBIDNKIT_DIR" != "no"; then
		# libidnkit1 and libidnkit2 have the same API
		if test "$PHP_HTTP_LIBIDNKIT2_DIR" != no && $PECL_VAR([HAVE_LIBIDNKIT2]); then
			AC_MSG_WARN([libidnkit-$PECL_VAR([LIBIDNKIT2_VERSION]) already enabled, skipping libidnkit1])
		else
			PECL_CHECK_CUSTOM(libidnkit, "$PHP_HTTP_LIBIDNKIT_DIR", idn/api.h, idnkit,
				[$($EGREP "define IDNKIT_VERSION\b" "include/idn/version.h" | $SED -e's/^.*VERSION//g' -e 's/@<:@^0-9\.@:>@//g')])
			if $PECL_VAR([HAVE_LIBIDNKIT]); then
				PECL_DEFINE([HAVE_IDNA2003])
			fi
			PECL_CHECK_DONE(libidnkit, [$PECL_VAR([HAVE_LIBIDNKIT])])
		fi
	fi

	dnl EXTENSIONS
	PECL_HAVE_PHP_EXT([raphf], [
		PECL_HAVE_PHP_EXT_HEADER([raphf])
	], [
		AC_MSG_ERROR([please install and enable pecl/raphf])
	])
	PECL_HAVE_PHP_EXT([propro], [
		PECL_HAVE_PHP_EXT_HEADER([propro])
	], [
		AC_MSG_ERROR([please install and enable pecl/propro])
	])
	PECL_HAVE_PHP_EXT([hash])
	PECL_HAVE_PHP_EXT([iconv])

	dnl DONE
	PHP_SUBST([HTTP_SHARED_LIBADD])

	PECL_VAR([SRCDIR])=PHP_EXT_SRCDIR(http)
	PECL_VAR([BUILDDIR])=PHP_EXT_BUILDDIR(http)

	PHP_ADD_INCLUDE([$PECL_VAR([SRCDIR])/src])
	PHP_ADD_BUILD_DIR([$PECL_VAR([BUILDDIR])/src])

	PECL_VAR([HEADERS])=$(cd $PECL_VAR([SRCDIR])/src && echo *.h)
	PECL_VAR([SOURCES])=$(cd $PECL_VAR([SRCDIR]) && echo src/*.c)

	PHP_NEW_EXTENSION(http, [$PECL_VAR([SOURCES])], [$ext_shared])
	PHP_INSTALL_HEADERS(ext/http, [php_http.h $PECL_VAR([HEADERS])])

	PHP_ARG_WITH([http-shared-deps], [whether to depend on extensions which have been built shared],
	[  --without-http-shared-deps       HTTP: do not depend on extensions like hash
                                         and iconv (when they are built shared)], yes, no)
	if test "$PHP_HTTP_SHARED_DEPS" != "no"; then
		if $PECL_VAR([HAVE_EXT_HASH]); then
			PHP_ADD_EXTENSION_DEP([http], [hash], true)
		fi
		if $PECL_VAR([HAVE_EXT_ICONV]); then
			PHP_ADD_EXTENSION_DEP([http], [iconv], true)
		fi
	fi
	PHP_ADD_EXTENSION_DEP([http], [raphf], true)
	PHP_ADD_EXTENSION_DEP([http], [propro], true)

	PHP_SUBST(PECL_VAR([HEADERS]))
	PHP_SUBST(PECL_VAR([SOURCES]))

	PHP_SUBST(PECL_VAR([SRCDIR]))
	PHP_SUBST(PECL_VAR([BUILDDIR]))

	PHP_ADD_MAKEFILE_FRAGMENT

	AC_DEFINE([HAVE_HTTP], [1], [Have extended HTTP support])
fi
