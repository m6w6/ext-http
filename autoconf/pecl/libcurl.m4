dnl
dnl PECL_HAVE_LIBCURL_FEATURE(feature[, code-if-yes[, code-if-no]])
dnl
dnl Checks $CURL_CONFIG --feature.
dnl Defines PHP_<PECL_NAME>_HAVE_LIBCURL_<FEATURE>
dnl
AC_DEFUN([PECL_HAVE_LIBCURL_FEATURE], [dnl
	AC_REQUIRE([PECL_PROG_EGREP])dnl
	AC_CACHE_CHECK([for $1 feature in libcurl], PECL_CACHE_VAR([HAVE_LIBCURL_FEATURE_$1]), [
		if $CURL_CONFIG --feature | $EGREP -q $1; then
			PECL_CACHE_VAR([HAVE_LIBCURL_FEATURE_$1])=yes
		else
			PECL_CACHE_VAR([HAVE_LIBCURL_FEATURE_$1])=no
		fi
	])
	PECL_VAR([HAVE_LIBCURL_FEATURE_$1])=$PECL_CACHE_VAR([HAVE_LIBCURL_FEATURE_$1])
	if $PECL_VAR([HAVE_LIBCURL_FEATURE_$1]); then
		PECL_DEFINE([HAVE_LIBCURL_$1])
		$2
	else
		ifelse([$3],,:,[$3])
	fi
])
dnl
dnl PECL_HAVE_LIBCURL_PROTOCOL(protocol[, code-if-yes[, code-if-no]])
dnl
AC_DEFUN([PECL_HAVE_LIBCURL_PROTOCOL], [
	AC_CACHE_CHECK([for $1 protocol in libcurl], PECL_CACHE_VAR([HAVE_LIBCURL_PROTOCOL_$1]), [
		if $CURL_CONFIG --protocols | $EGREP -q $1; then
			PECL_CACHE_VAR([HAVE_LIBCURL_PROTOCOL_$1])=yes
		else
			PECL_CACHE_VAR([HAVE_LIBCURL_PROTOCOL_$1])=no
		fi
	])
	PECL_VAR([HAVE_LIBCURL_PROTOCOL_$1])=$PECL_CACHE_VAR([HAVE_LIBCURL_PROTOCOL_$1])
	if $PECL_VAR([HAVE_LIBCURL_PROTOCOL_$1]); then
		PECL_DEFINE([HAVE_LIBCURL_$1])
		$2
	else
		ifelse([$3],,:,[$3])
	fi
])
dnl
dnl PECL_HAVE_LIBCURL_SSLLIB(ssllib-name, headers, libs)
dnl
AC_DEFUN([PECL_HAVE_LIBCURL_SSLLIB], [
	if test -z "$PECL_VAR([LIBCURL_SSLLIB])"; then
		AC_CACHE_CHECK([for $1 providing SSL in libcurl], PECL_CACHE_VAR([HAVE_LIBCURL_$1]), [
			AC_TRY_RUN([
				#include <strings.h>
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
				PECL_CACHE_VAR([HAVE_LIBCURL_$1])=yes
			], [
				PECL_CACHE_VAR([HAVE_LIBCURL_$1])=no
			])
		])
		PECL_VAR([HAVE_LIBCURL_$1])=$PECL_CACHE_VAR([HAVE_LIBCURL_$1])
		if $PECL_VAR([HAVE_LIBCURL_$1]); then
			PECL_VAR([LIBCURL_SSLLIB])="$1"
			PECL_DEFINE([HAVE_LIBCURL_$1])
			m4_foreach_w(header, $2, [AC_CHECK_HEADER(header,, [
				PECL_VAR([HAVE_LIBCURL_$1])=false
			])])
			ifelse([$3],,,[
				if $PECL_VAR([HAVE_LIBCURL_$1]); then
					m4_foreach_w(lib, $3, [
						PHP_ADD_LIBRARY(lib, true, [AS_TR_CPP(PECL_NAME[_SHARED_LIBADD])])
					])
				fi
			])
		fi
	fi
])
dnl
dnl PECL_HAVE_LIBCURL_SSL
dnl
AC_DEFUN([PECL_HAVE_LIBCURL_SSL], [dnl
	AC_REQUIRE([PECL_HAVE_LIBCURL_CA])dnl
	PECL_HAVE_LIBCURL_FEATURE([SSL], [
		PECL_HAVE_LIBCURL_SSLLIB([OpenSSL], [openssl/ssl.h openssl/crypto.h], [ssl crypto])
		PECL_HAVE_LIBCURL_SSLLIB([GnuTLS], [gnutls/gnutls.h gcrypt.h], [gnutls gcrypt])
		PECL_HAVE_LIBCURL_SSLLIB([NSS])
		PECL_HAVE_LIBCURL_SSLLIB([SecureTransport])
		PECL_HAVE_LIBCURL_SSLLIB([GSKit])
		PECL_HAVE_LIBCURL_SSLLIB([PolarSSL])
		PECL_HAVE_LIBCURL_SSLLIB([WolfSSL])
		PECL_HAVE_LIBCURL_SSLLIB([mbedTLS])
		PECL_HAVE_LIBCURL_SSLLIB([axTLS])

		case "$PECL_VAR([LIBCURL_SSLLIB])" in
		OpenSSL|GnuTLS|PolarSSL)
			PECL_DEFINE([HAVE_LIBCURL_CAPATH])
			PECL_DEFINE([HAVE_LIBCURL_CAINFO])
			;;
		NSS)
			AC_CACHE_CHECK([whether NSS PEM is available], [PECL_CACHE_VAR([HAVE_LIBCURL_NSSPEM])], [
				PECL_SAVE_ENV([LIBS], [NSSPEM])
				LIBS="$LIBS -lnsspem"
				AC_TRY_LINK([], [(void)0;], [
					PECL_CACHE_VAR([HAVE_LIBCURL_NSSPEM])=yes
				], [
					PECL_CACHE_VAR([HAVE_LIBCURL_NSSPEM])=no
				])
				PECL_RESTORE_ENV([LIBS], [NSSPEM])
			])
			if $PECL_CACHE_VAR([HAVE_LIBCURL_NSSPEM]); then
				PECL_DEFINE([HAVE_LIBCURL_CAINFO])
			else
				PECL_DEFINE([HAVE_LIBCURL_CAINFO], [0])
			fi
			PECL_DEFINE([HAVE_LIBCURL_CAPATH], [0])
			;;
		*)
			PECL_DEFINE([HAVE_LIBCURL_CAPATH], [0])
			PECL_DEFINE([HAVE_LIBCURL_CAINFO], [0])
			;;
		esac

		PECL_HAVE_CONST([curl/curl.h], [CURLOPT_TLSAUTH_TYPE], int, [
			AC_CACHE_CHECK([whether CURLOPT_TLSAUTH_TYPE expects CURL_TLSAUTH_SRP], PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP]), [
				PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP])=
				AC_TRY_RUN([
					#include <curl/curl.h>
					int main(int argc, char *argv[]) {
						CURL *ch = curl_easy_init();
						return curl_easy_setopt(ch, CURLOPT_TLSAUTH_TYPE, CURL_TLSAUTH_SRP);
					}
				], [
					PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP])=yes
				], [
					AC_TRY_RUN([
						#include <curl/curl.h>
						int main(int argc, char *argv[]) {
							CURL *ch = curl_easy_init();
							return curl_easy_setopt(ch, CURLOPT_TLSAUTH_TYPE, "SRP");
						}
					], [
						PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP])=no
					])
				])
			])
			if test -n "$PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP])"; then
				PECL_DEFINE([HAVE_LIBCURL_TLSAUTH_TYPE])
				if $PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP]); then
					PECL_DEFINE([LIBCURL_TLSAUTH_SRP], [CURL_TLSAUTH_SRP])
					PECL_DEFINE([LIBCURL_TLSAUTH_DEF], [CURL_TLSAUTH_NONE])
				else
					PECL_DEFINE([LIBCURL_TLSAUTH_SRP], ["SRP"])
					PECL_DEFINE([LIBCURL_TLSAUTH_DEF], [""])
				fi
			fi
		])
	])
])
dnl
dnl PECL_HAVE_LIBCURL_ARES
dnl
AC_DEFUN([PECL_HAVE_LIBCURL_ARES], [
	AC_CACHE_CHECK([for c-ares providing AsynchDNS in libcurl], PECL_CACHE_VAR([HAVE_LIBCURL_ARES]), [
		AC_TRY_RUN([
			#include <curl/curl.h>
			int main(int argc, char *argv[]) {
				curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
				if (data && data->ares && data->ares_num) {
					return 0;
				}
				return 1;
			}
		], [
			PECL_CACHE_VAR([HAVE_LIBCURL_ARES])=yes
		], [
			PECL_CACHE_VAR([HAVE_LIBCURL_ARES])=no
		])
	])
	PECL_VAR([HAVE_LIBCURL_ARES])=$PECL_CACHE_VAR([HAVE_LIBCURL_ARES])
	if $PECL_VAR([HAVE_LIBCURL_ARES]); then
		PECL_DEFINE([HAVE_LIBCURL_ARES])
	fi
])
dnl
dnl PECL_HAVE_LIBCURL_CA
dnl
dnl Checks for any installed default CA path/info with PECL_CHECK_CA providing curl's ca config.
dnl Defines shell vars PHP_<PECL_NAME>_LIBCURL_CAPATH and PHP_<PECL_NAME>_LIBCURL_CAINFO
dnl additionally to those defined in PECL_CHECK_CA.
dnl
AC_DEFUN([PECL_HAVE_LIBCURL_CA], [
	CURL_CONFIG_CA=$($CURL_CONFIG --ca)
	if test -z "$CURL_CONFIG_CA"; then
		CURL_CONFIG_CA=$($CURL_CONFIG --configure | $EGREP -o -- "--with-ca@<:@^\'@:>@*" | $SED 's/.*=//')
	fi
	PECL_CHECK_CA($CURL_CONFIG_CA, $CURL_CONFIG_CA)
	PECL_VAR([LIBCURL_CAPATH])=$PECL_VAR([CAPATH])
	PECL_VAR([LIBCURL_CAINFO])=$PECL_VAR([CAINFO])
])
dnl
dnl PECL_CHECK_LIBCURL(search-dir[, min-version])
dnl
dnl Defines shell var $CURL_CONFIG (false if curl-config could not be found).
dnl
dnl Calls PECL_CHECK_CONFIG(libcurl ...) for its tests.
dnl Call PECL_CHECK_DONE(libcurl[, success]) yourself when you're done
dnl with any subtests. This is IMPORTANT!
dnl
AC_DEFUN([PECL_CHECK_LIBCURL], [dnl
	AC_REQUIRE([PECL_PROG_SED])dnl
	AC_REQUIRE([PECL_PROG_EGREP])dnl
	AC_PATH_PROG([CURL_CONFIG], [curl-config], false, [$1/bin:$PATH:/usr/local/bin])
	PECL_CHECK_CONFIG(libcurl, $CURL_CONFIG,
		[--version | $SED -e 's/@<:@^0-9\.@:>@//g'],
		[--cflags],
		[--libs | $EGREP -o -- '(^|\s)-L@<:@^ @:>@* ?' | xargs],
		[--libs | $EGREP -o -- '(^|\s)-l@<:@^ @:>@* ?' | xargs]dnl
	)
	ifelse([$2],,,[
		PECL_HAVE_VERSION([libcurl], [$2])
	])
])