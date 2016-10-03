dnl
dnl PECL_CHECK_ZLIB(
dnl
dnl Calls PECL_CHECK_CUSTOM(zlib ...) for its tests.
dnl Call PECL_CHECK_DONE(zlib[, success]) yourself when you're done
dnl with any subtests. This is IMPORTANT!
dnl
AC_DEFUN([PECL_CHECK_ZLIB], [
	PECL_CHECK_CUSTOM(zlib, ["$1" "$PHP_ZLIB_DIR" "$PHP_ZLIB"], zlib.h, z,
		[$($EGREP "define ZLIB_VERSION" "$path/include/zlib.h" | $SED -e 's/@<:@^0-9\.@:>@//g')])
	ifelse([$2],,,[PECL_HAVE_VERSION(zlib, $2)])
])