dnl
dnl PECL_HAVE_LIBCURL_FEATURE(feature[, code-if-yes[, code-if-no]])
dnl
dnl Checks $CURL_CONFIG --feature.
dnl Defines PHP_<PECL_NAME>_HAVE_LIBCURL_<FEATURE>
dnl
AC_DEFUN([PECL_HAVE_LIBCURL_FEATURE], [dnl
	AC_REQUIRE([PECL_PROG_EGREP])dnl
	AC_CACHE_CHECK([for $1 feature in libcurl], PECL_CACHE_VAR([HAVE_LIBCURL_FEATURE_$1]), [
		if $CURL_CONFIG --feature | $EGREP -qi $1; then
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

		PECL_HAVE_CONST([curl/curl.h], [CURL_VERSION_TLSAUTH_SRP], int, [
			AC_CACHE_CHECK([for CURLOPT_TLSAUTH_TYPE SRP support], PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP]), [
				PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP])=no
				AC_TRY_RUN([
					#include <curl/curl.h>
					int main(int argc, char *argv[]) {
						int has_feature = curl_version_info(CURLVERSION_NOW)->features & CURL_VERSION_TLSAUTH_SRP;
						int set_failure = curl_easy_setopt(curl_easy_init(), CURLOPT_TLSAUTH_TYPE, "SRP"");
						return !has_feature || set_failure;
					}
				], [
					PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP])=yes
				])
			])
			if test "$PECL_CACHE_VAR([LIBCURL_TLSAUTH_SRP])" = "yes"; then
				PECL_DEFINE([HAVE_LIBCURL_TLSAUTH_TYPE])
			fi
		])

		PECL_HAVE_CONST([curl/curl.h], [CURL_LOCK_DATA_SSL_SESSION], int, [
			AC_CACHE_CHECK([whether curl_share accepts CURL_LOCK_DATA_SSL_SESSION], PECL_CACHE_VAR([LIBCURL_SHARE_SSL]), [
				PECL_CACHE_VAR([LIBCURL_SHARE_SSL])=
				AC_TRY_RUN([
					#include <curl/curl.h>
					int main(int argc, char *argv[]) {
						CURLSH *ch = curl_share_init();
						return curl_share_setopt(ch, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
					}
				], [
					PECL_CACHE_VAR([LIBCURL_SHARE_SSL])=yes
				], [
					PECL_CACHE_VAR([LIBCURL_SHARE_SSL])=no
				])
			])
			if test "$PECL_CACHE_VAR([LIBCURL_SHARE_SSL])" = yes; then
				PECL_DEFINE([HAVE_LIBCURL_SHARE_SSL], [1])
			fi
		])

		if test "$PECL_VAR([LIBCURL_SSLLIB])" == "OpenSSL"; then
			PECL_HAVE_CONST([curl/curl.h], [CURLOPT_TLS13_CIPHERS], int, [
				AC_CACHE_CHECK([whether curl_easy_setopt accepts CURLOPT_TLS13_CIPHERS], PECL_CACHE_VAR([LIBCURL_TLS13_CIPHERS]), [
					PECL_CACHE_VAR([LIBCURL_TLS13_CIPHERS])=
					AC_TRY_RUN([
						#include <curl/curl.h>
						int main(int argc, char *argv[]) {
							CURL *ch = curl_easy_init();
							return curl_easy_setopt(ch, CURLSHOPT_TLS13_CIPHERS, "");
						}
					], [
						PECL_CACHE_VAR([LIBCURL_TLS13_CIPHERS])=yes
					], [
						PECL_CACHE_VAR([LIBCURL_TLS13_CIPHERS])=no
					])
				])
				if test "$PECL_CACHE_VAR([LIBCURL_TLS13_CIPHERS])" = yes; then
					PECL_DEFINE([HAVE_LIBCURL_TLS13_CIPHERS], [1])
				fi
			])
		fi
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
