
AC_DEFUN([PECL_CHECK_LIBEVENT], [
	PECL_CHECK_PKGCONFIG(libevent, [$1])
	if test -n "$PECL_CHECKED_VERSION(libevent)"; then
		PECL_HAVE_VERSION(libevent, 2.0, [
			PECL_DEFINE([HAVE_LIBEVENT2])
		], [ ])
		ifelse([$2],,,[PECL_HAVE_VERSION(libevent, [$2])])
		AC_CHECK_FUNC(event_base_new,,[
			AC_DEFINE([event_base_new], [event_init], [missing event_base_new() in libevent1])
		])
		AC_CHECK_FUNC(event_assign,,[
			AC_DEFINE([event_assign(e, b, s, a, cb, d)], [do {\
				event_set(e, s, a, cb, d); \
				event_base_set(b, e);\
			} while(0)], [missing event_assign() in libevent1])
		])
	fi
])