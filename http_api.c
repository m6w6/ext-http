/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#define _WINSOCKAPI_
#define ZEND_INCLUDE_FULL_WINDOWS_HEADERS

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <ctype.h>

#ifdef PHP_WIN32
#	include <winsock2.h>
#elif defined(HAVE_NETDB_H)
#	include <netdb.h>
#endif

#include "php.h"
#include "php_version.h"
#include "php_streams.h"
#include "snprintf.h"
#include "ext/standard/md5.h"
#include "ext/standard/url.h"
#include "ext/standard/base64.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_lcg.h"

#include "SAPI.h"

#ifdef ZEND_ENGINE_2
#	include "ext/standard/php_http.h"
#endif

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_std_defs.h"

ZEND_DECLARE_MODULE_GLOBALS(http)

/* {{{ day/month names */
static const char *days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *wkdays[] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};
static const char *weekdays[] = {
	"Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday", "Sunday"
};
static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Okt", "Nov", "Dec"
};
enum assume_next {
	DATE_MDAY,
	DATE_YEAR,
	DATE_TIME
};
static const struct time_zone {
	const char *name;
	const int offset;
} time_zones[] = {
    {"GMT", 0},     /* Greenwich Mean */
    {"UTC", 0},     /* Universal (Coordinated) */
    {"WET", 0},     /* Western European */
    {"BST", 0},     /* British Summer */
    {"WAT", 60},    /* West Africa */
    {"AST", 240},   /* Atlantic Standard */
    {"ADT", 240},   /* Atlantic Daylight */
    {"EST", 300},   /* Eastern Standard */
    {"EDT", 300},   /* Eastern Daylight */
    {"CST", 360},   /* Central Standard */
    {"CDT", 360},   /* Central Daylight */
    {"MST", 420},   /* Mountain Standard */
    {"MDT", 420},   /* Mountain Daylight */
    {"PST", 480},   /* Pacific Standard */
    {"PDT", 480},   /* Pacific Daylight */
    {"YST", 540},   /* Yukon Standard */
    {"YDT", 540},   /* Yukon Daylight */
    {"HST", 600},   /* Hawaii Standard */
    {"HDT", 600},   /* Hawaii Daylight */
    {"CAT", 600},   /* Central Alaska */
    {"AHST", 600},  /* Alaska-Hawaii Standard */
    {"NT",  660},   /* Nome */
    {"IDLW", 720},  /* International Date Line West */
    {"CET", -60},   /* Central European */
    {"MET", -60},   /* Middle European */
    {"MEWT", -60},  /* Middle European Winter */
    {"MEST", -120}, /* Middle European Summer */
    {"CEST", -120}, /* Central European Summer */
    {"MESZ", -60},  /* Middle European Summer */
    {"FWT", -60},   /* French Winter */
    {"FST", -60},   /* French Summer */
    {"EET", -120},  /* Eastern Europe, USSR Zone 1 */
    {"WAST", -420}, /* West Australian Standard */
    {"WADT", -420}, /* West Australian Daylight */
    {"CCT", -480},  /* China Coast, USSR Zone 7 */
    {"JST", -540},  /* Japan Standard, USSR Zone 8 */
    {"EAST", -600}, /* Eastern Australian Standard */
    {"EADT", -600}, /* Eastern Australian Daylight */
    {"GST", -600},  /* Guam Standard, USSR Zone 9 */
    {"NZT", -720},  /* New Zealand */
    {"NZST", -720}, /* New Zealand Standard */
    {"NZDT", -720}, /* New Zealand Daylight */
    {"IDLE", -720}, /* International Date Line East */
};
/* }}} */

/* {{{ internals */

static int http_sort_q(const void *a, const void *b TSRMLS_DC);
#define http_send_chunk(d, b, e, m) _http_send_chunk((d), (b), (e), (m) TSRMLS_CC)
static STATUS _http_send_chunk(const void *data, const size_t begin, const size_t end, const http_send_mode mode TSRMLS_DC);

static int check_day(char *day, size_t len);
static int check_month(char *month);
static int check_tzone(char *tzone);

static int http_ob_stack_get(php_ob_buffer *, php_ob_buffer **);

/* {{{ static int http_sort_q(const void *, const void *) */
static int http_sort_q(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f, *s;
	zval result, *first, *second;

	f = *((Bucket **) a);
	s = *((Bucket **) b);

	first = *((zval **) f->pData);
	second= *((zval **) s->pData);

	if (numeric_compare_function(&result, first, second TSRMLS_CC) != SUCCESS) {
		return 0;
	}
	return (Z_LVAL(result) > 0 ? -1 : (Z_LVAL(result) < 0 ? 1 : 0));
}
/* }}} */

/* {{{ static STATUS http_send_chunk(const void *, size_t, size_t,
	http_send_mode) */
static STATUS _http_send_chunk(const void *data, const size_t begin,
	const size_t end, const http_send_mode mode TSRMLS_DC)
{
	char *buf;
	size_t read = 0;
	long len = end - begin;
	php_stream *s;

	switch (mode)
	{
		case SEND_RSRC:
			s = (php_stream *) data;
			if (php_stream_seek(s, begin, SEEK_SET)) {
				return FAILURE;
			}
			buf = (char *) ecalloc(1, HTTP_SENDBUF_SIZE);
			/* read into buf and write out */
			while ((len -= HTTP_SENDBUF_SIZE) >= 0) {
				if (!(read = php_stream_read(s, buf, HTTP_SENDBUF_SIZE))) {
					efree(buf);
					return FAILURE;
				}
				if (read - php_body_write(buf, read TSRMLS_CC)) {
					efree(buf);
					return FAILURE;
				}
			}

			/* read & write left over */
			if (len) {
				if (read = php_stream_read(s, buf, HTTP_SENDBUF_SIZE + len)) {
					if (read - php_body_write(buf, read TSRMLS_CC)) {
						efree(buf);
						return FAILURE;
					}
				} else {
					efree(buf);
					return FAILURE;
				}
			}
			efree(buf);
			return SUCCESS;
		break;

		case SEND_DATA:
			return len == php_body_write(((char *)data) + begin, len TSRMLS_CC)
				? SUCCESS : FAILURE;
		break;

		default:
			return FAILURE;
		break;
	}
}
/* }}} */

/* {{{ Day/Month/TZ checks for http_parse_date()
	Originally by libcurl, Copyright (C) 1998 - 2004, Daniel Stenberg, <daniel@haxx.se>, et al. */
static int check_day(char *day, size_t len)
{
	int i;
	const char * const *check = (len > 3) ? &weekdays[0] : &wkdays[0];
	for (i = 0; i < 7; i++) {
	    if (!strcmp(day, check[0])) {
	    	return i;
		}
		check++;
	}
	return -1;
}

static int check_month(char *month)
{
	int i;
	const char * const *check = &months[0];
	for (i = 0; i < 12; i++) {
		if (!strcmp(month, check[0])) {
			return i;
		}
		check++;
	}
	return -1;
}

/* return the time zone offset between GMT and the input one, in number
   of seconds or -1 if the timezone wasn't found/legal */

static int check_tzone(char *tzone)
{
	int i;
	const struct time_zone *check = time_zones;
	for (i = 0; i < sizeof(time_zones) / sizeof(time_zones[0]); i++) {
		if (!strcmp(tzone, check->name)) {
			return check->offset * 60;
		}
		check++;
	}
	return -1;
}
/* }}} */

/* char *pretty_key(char *, int, int, int) */
char *pretty_key(char *key, int key_len, int uctitle, int xhyphen)
{
	if (key && key_len) {
		int i, wasalpha;
		if (wasalpha = isalpha(key[0])) {
			key[0] = uctitle ? toupper(key[0]) : tolower(key[0]);
		}
		for (i = 1; i < key_len; i++) {
			if (isalpha(key[i])) {
				key[i] = ((!wasalpha) && uctitle) ? toupper(key[i]) : tolower(key[i]);
				wasalpha = 1;
			} else {
				if (xhyphen && (key[i] == '_')) {
					key[i] = '-';
				}
				wasalpha = 0;
			}
		}
	}
	return key;
}
/* }}} */

/* {{{ static STATUS http_ob_stack_get(php_ob_buffer *, php_ob_buffer **) */
static STATUS http_ob_stack_get(php_ob_buffer *o, php_ob_buffer **s)
{
	static int i = 0;
	php_ob_buffer *b = emalloc(sizeof(php_ob_buffer));
	b->handler_name = estrdup(o->handler_name);
	b->buffer = estrndup(o->buffer, o->text_length);
	b->text_length = o->text_length;
	b->chunk_size = o->chunk_size;
	b->erase = o->erase;
	s[i++] = b;
	return SUCCESS;
}
/* }}} */

/* }}} internals */

/* {{{ public API */

/* {{{ char *http_date(time_t) */
PHP_HTTP_API char *_http_date(time_t t TSRMLS_DC)
{
	struct tm *gmtime, tmbuf;

	if (gmtime = php_gmtime_r(&t, &tmbuf)) {
		char *date = ecalloc(1, 31);
		snprintf(date, 30,
			"%s, %02d %s %04d %02d:%02d:%02d GMT",
			days[gmtime->tm_wday], gmtime->tm_mday,
			months[gmtime->tm_mon], gmtime->tm_year + 1900,
			gmtime->tm_hour, gmtime->tm_min, gmtime->tm_sec
		);
		return date;
	}

	return NULL;
}
/* }}} */

/* {{{ time_t http_parse_date(char *)
	Originally by libcurl, Copyright (C) 1998 - 2004, Daniel Stenberg, <daniel@haxx.se>, et al. */
PHP_HTTP_API time_t _http_parse_date(const char *date)
{
	time_t t = 0;
	int tz_offset = -1, year = -1, month = -1, monthday = -1, weekday = -1,
		hours = -1, minutes = -1, seconds = -1;
	struct tm tm;
	enum assume_next dignext = DATE_MDAY;
	const char *indate = date;

	int found = 0, part = 0; /* max 6 parts */

	while (*date && (part < 6)) {
		int found = 0;

		while (*date && !isalnum(*date)) {
			date++;
		}

		if (isalpha(*date)) {
			/* a name coming up */
			char buf[32] = "";
			size_t len;
			sscanf(date, "%31[A-Za-z]", buf);
			len = strlen(buf);

			if (weekday == -1) {
				weekday = check_day(buf, len);
				if (weekday != -1) {
					found = 1;
				}
			}

			if (!found && (month == -1)) {
				month = check_month(buf);
				if (month != -1) {
					found = 1;
				}
			}

			if (!found && (tz_offset == -1)) {
				/* this just must be a time zone string */
				tz_offset = check_tzone(buf);
				if (tz_offset != -1) {
					found = 1;
				}
			}

			if (!found) {
				return -1; /* bad string */
			}
			date += len;
		}
		else if (isdigit(*date)) {
			/* a digit */
			int val;
			char *end;
			if ((seconds == -1) &&
				(3 == sscanf(date, "%02d:%02d:%02d", &hours, &minutes, &seconds))) {
				/* time stamp! */
				date += 8;
				found = 1;
			}
			else {
				val = (int) strtol(date, &end, 10);

				if ((tz_offset == -1) && ((end - date) == 4) && (val < 1300) &&
					(indate < date) && ((date[-1] == '+' || date[-1] == '-'))) {
					/* four digits and a value less than 1300 and it is preceeded with
					a plus or minus. This is a time zone indication. */
					found = 1;
					tz_offset = (val / 100 * 60 + val % 100) * 60;

					/* the + and - prefix indicates the local time compared to GMT,
					this we need ther reversed math to get what we want */
					tz_offset = date[-1] == '+' ? -tz_offset : tz_offset;
				}

				if (((end - date) == 8) && (year == -1) && (month == -1) && (monthday == -1)) {
					/* 8 digits, no year, month or day yet. This is YYYYMMDD */
					found = 1;
					year = val / 10000;
					month = (val % 10000) / 100 - 1; /* month is 0 - 11 */
					monthday = val % 100;
				}

				if (!found && (dignext == DATE_MDAY) && (monthday == -1)) {
					if ((val > 0) && (val < 32)) {
						monthday = val;
						found = 1;
					}
					dignext = DATE_YEAR;
				}

				if (!found && (dignext == DATE_YEAR) && (year == -1)) {
					year = val;
					found = 1;
					if (year < 1900) {
						year += year > 70 ? 1900 : 2000;
					}
					if(monthday == -1) {
						dignext = DATE_MDAY;
					}
				}

				if (!found) {
					return -1;
				}

				date = end;
			}
		}

		part++;
	}

	if (-1 == seconds) {
		seconds = minutes = hours = 0; /* no time, make it zero */
	}

	if ((-1 == monthday) || (-1 == month) || (-1 == year)) {
		/* lacks vital info, fail */
		return -1;
	}

	if (sizeof(time_t) < 5) {
		/* 32 bit time_t can only hold dates to the beginning of 2038 */
		if (year > 2037) {
			return 0x7fffffff;
		}
	}

	tm.tm_sec = seconds;
	tm.tm_min = minutes;
	tm.tm_hour = hours;
	tm.tm_mday = monthday;
	tm.tm_mon = month;
	tm.tm_year = year - 1900;
	tm.tm_wday = 0;
	tm.tm_yday = 0;
	tm.tm_isdst = 0;

	t = mktime(&tm);

	/* time zone adjust */
	{
		struct tm *gmt, keeptime2;
		long delta;
		time_t t2;

		if(!(gmt = php_gmtime_r(&t, &keeptime2))) {
			return -1; /* illegal date/time */
		}

		t2 = mktime(gmt);

		/* Add the time zone diff (between the given timezone and GMT) and the
		diff between the local time zone and GMT. */
		delta = (tz_offset != -1 ? tz_offset : 0) + (t - t2);

		if((delta > 0) && (t + delta < t)) {
			return -1; /* time_t overflow */
		}

		t += delta;
	}

	return t;
}
/* }}} */

/* {{{ char *http_etag(void *, size_t, http_send_mode) */
PHP_HTTP_API char *_http_etag(const void *data_ptr, const size_t data_len,
	const http_send_mode data_mode TSRMLS_DC)
{
	char ssb_buf[128] = {0};
	unsigned char digest[16];
	PHP_MD5_CTX ctx;
	char *new_etag = ecalloc(1, 33);

	PHP_MD5Init(&ctx);

	switch (data_mode)
	{
		case SEND_DATA:
			PHP_MD5Update(&ctx, data_ptr, data_len);
		break;

		case SEND_RSRC:
			if (!HTTP_G(ssb).sb.st_ino) {
				if (php_stream_stat((php_stream *) data_ptr, &HTTP_G(ssb))) {
					return NULL;
				}
			}
			snprintf(ssb_buf, 127, "%ld=%ld=%ld",
				HTTP_G(ssb).sb.st_mtime,
				HTTP_G(ssb).sb.st_ino,
				HTTP_G(ssb).sb.st_size
			);
			PHP_MD5Update(&ctx, ssb_buf, strlen(ssb_buf));
		break;

		default:
			efree(new_etag);
			return NULL;
		break;
	}

	PHP_MD5Final(digest, &ctx);
	make_digest(new_etag, digest);

	return new_etag;
}
/* }}} */

/* {{{ time_t http_lmod(void *, http_send_mode) */
PHP_HTTP_API time_t _http_lmod(const void *data_ptr, const http_send_mode data_mode TSRMLS_DC)
{
	switch (data_mode)
	{
		case SEND_DATA:
		{
			return time(NULL);
		}

		case SEND_RSRC:
		{
			if (!HTTP_G(ssb).sb.st_mtime) {
				if (php_stream_stat((php_stream *) data_ptr, &HTTP_G(ssb))) {
					return 0;
				}
			}
			return HTTP_G(ssb).sb.st_mtime;
		}

		default:
		{
			if (!HTTP_G(ssb).sb.st_mtime) {
				if(php_stream_stat_path(Z_STRVAL_P((zval *) data_ptr), &HTTP_G(ssb))) {
					return 0;
				}
			}
			return HTTP_G(ssb).sb.st_mtime;
		}
	}
}
/* }}} */

/* {{{ STATUS http_send_status_header(int, char *) */
PHP_HTTP_API STATUS _http_send_status_header(const int status, const char *header TSRMLS_DC)
{
	sapi_header_line h = {(char *) header, strlen(header), status};
	return sapi_header_op(SAPI_HEADER_REPLACE, &h TSRMLS_CC);
}
/* }}} */

/* {{{ zval *http_get_server_var(char *) */
PHP_HTTP_API zval *_http_get_server_var(const char *key TSRMLS_DC)
{
	zval **var;
	if (SUCCESS == zend_hash_find(
			HTTP_SERVER_VARS,
			(char *) key, strlen(key) + 1, (void **) &var)) {
		return *var;
	}
	return NULL;
}
/* }}} */

/* {{{ void http_ob_etaghandler(char *, uint, char **, uint *, int) */
PHP_HTTP_API void _http_ob_etaghandler(char *output, uint output_len,
	char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	char etag[33] = { 0 };
	unsigned char digest[16];

	if (mode & PHP_OUTPUT_HANDLER_START) {
		PHP_MD5Init(&HTTP_G(etag_md5));
	}

	PHP_MD5Update(&HTTP_G(etag_md5), output, output_len);

	if (mode & PHP_OUTPUT_HANDLER_END) {
		PHP_MD5Final(digest, &HTTP_G(etag_md5));

		/* just do that if desired */
		if (HTTP_G(etag_started)) {
			make_digest(etag, digest);

			if (http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
				http_send_status(304);
			} else {
				http_send_etag(etag, 32);
			}
		}
	}

	*handled_output_len = output_len;
	*handled_output = estrndup(output, output_len);
}
/* }}} */

/* {{{ STATUS http_start_ob_handler(php_output_handler_func_t, char *, uint, zend_bool) */
PHP_HTTP_API STATUS _http_start_ob_handler(php_output_handler_func_t handler_func,
	char *handler_name, uint chunk_size, zend_bool erase TSRMLS_DC)
{
	php_ob_buffer **stack;
	int count, i;

	if (count = OG(ob_nesting_level)) {
		stack = ecalloc(count, sizeof(php_ob_buffer *));

		if (count > 1) {
			zend_stack_apply_with_argument(&OG(ob_buffers), ZEND_STACK_APPLY_BOTTOMUP,
				(int (*)(void *elem, void *)) http_ob_stack_get, stack);
		}

		if (count > 0) {
			http_ob_stack_get(&OG(active_ob_buffer), stack);
		}

		while (OG(ob_nesting_level)) {
			php_end_ob_buffer(0, 0 TSRMLS_CC);
		}
	}

	php_ob_set_internal_handler(handler_func, chunk_size, handler_name, erase TSRMLS_CC);

	for (i = 0; i < count; i++) {
		php_ob_buffer *s = stack[i];
		if (strcmp(s->handler_name, "default output handler")) {
			php_start_ob_buffer_named(s->handler_name, s->chunk_size, s->erase TSRMLS_CC);
		}
		php_body_write(s->buffer, s->text_length TSRMLS_CC);
		efree(s->handler_name);
		efree(s->buffer);
		efree(s);
	}
	if (count) {
		efree(stack);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ int http_modified_match(char *, int) */
PHP_HTTP_API int _http_modified_match(const char *entry, const time_t t TSRMLS_DC)
{
	int retval;
	zval *zmodified;
	char *modified, *chr_ptr;

	HTTP_GSC(zmodified, entry, 0);

	modified = estrndup(Z_STRVAL_P(zmodified), Z_STRLEN_P(zmodified));
	if (chr_ptr = strrchr(modified, ';')) {
		chr_ptr = 0;
	}
	retval = (t <= http_parse_date(modified));
	efree(modified);
	return retval;
}
/* }}} */

/* {{{ int http_etag_match(char *, char *) */
PHP_HTTP_API int _http_etag_match(const char *entry, const char *etag TSRMLS_DC)
{
	zval *zetag;
	char *quoted_etag;
	STATUS result;

	HTTP_GSC(zetag, entry, 0);

	if (NULL != strchr(Z_STRVAL_P(zetag), '*')) {
		return 1;
	}

	quoted_etag = (char *) emalloc(strlen(etag) + 3);
	sprintf(quoted_etag, "\"%s\"", etag);

	if (!strchr(Z_STRVAL_P(zetag), ',')) {
		result = !strcmp(Z_STRVAL_P(zetag), quoted_etag);
	} else {
		result = (NULL != strstr(Z_STRVAL_P(zetag), quoted_etag));
	}
	efree(quoted_etag);
	return result;
}
/* }}} */

/* {{{ STATUS http_send_last_modified(int) */
PHP_HTTP_API STATUS _http_send_last_modified(const time_t t TSRMLS_DC)
{
	char *date = NULL;
	if (date = http_date(t)) {
		char modified[96] = "Last-Modified: ";
		strcat(modified, date);
		efree(date);

		/* remember */
		HTTP_G(lmod) = t;

		return http_send_header(modified);
	}
	return FAILURE;
}
/* }}} */

/* {{{ static STATUS http_send_etag(char *, int) */
PHP_HTTP_API STATUS _http_send_etag(const char *etag,
	const int etag_len TSRMLS_DC)
{
	STATUS status;
	char *etag_header;

	if (!etag_len){
		php_error_docref(NULL TSRMLS_CC,E_ERROR,
			"Attempt to send empty ETag (previous: %s)\n", HTTP_G(etag));
		return FAILURE;
	}

	/* remember */
	if (HTTP_G(etag)) {
		efree(HTTP_G(etag));
	}
	HTTP_G(etag) = estrdup(etag);

	etag_header = ecalloc(1, sizeof("ETag: \"\"") + etag_len);
	sprintf(etag_header, "ETag: \"%s\"", etag);
	if (SUCCESS != (status = http_send_header(etag_header))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't send '%s' header", etag_header);
	}
	efree(etag_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_send_cache_control(char *, size_t) */
PHP_HTTP_API STATUS _http_send_cache_control(const char *cache_control,
	const size_t cc_len TSRMLS_DC)
{
	STATUS status;
	char *cc_header = ecalloc(1, sizeof("Cache-Control: ") + cc_len);

	sprintf(cc_header, "Cache-Control: %s", cache_control);
	if (SUCCESS != (status = http_send_header(cc_header))) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE,
			"Could not send '%s' header", cc_header);
	}
	efree(cc_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_send_content_type(char *, size_t) */
PHP_HTTP_API STATUS _http_send_content_type(const char *content_type,
	const size_t ct_len TSRMLS_DC)
{
	STATUS status;
	char *ct_header;

	if (!strchr(content_type, '/')) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Content-Type '%s' doesn't seem to consist of a primary and a secondary part",
			content_type);
		return FAILURE;
	}

	/* remember for multiple ranges */
	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
	}
	HTTP_G(ctype) = estrndup(content_type, ct_len);

	ct_header = ecalloc(1, sizeof("Content-Type: ") + ct_len);
	sprintf(ct_header, "Content-Type: %s", content_type);

	if (SUCCESS != (status = http_send_header(ct_header))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Couldn't send '%s' header", ct_header);
	}
	efree(ct_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_send_content_disposition(char *, size_t, zend_bool) */
PHP_HTTP_API STATUS _http_send_content_disposition(const char *filename,
	const size_t f_len, const int send_inline TSRMLS_DC)
{
	STATUS status;
	char *cd_header;

	if (send_inline) {
		cd_header = ecalloc(1, sizeof("Content-Disposition: inline; filename=\"\"") + f_len);
		sprintf(cd_header, "Content-Disposition: inline; filename=\"%s\"", filename);
	} else {
		cd_header = ecalloc(1, sizeof("Content-Disposition: attachment; filename=\"\"") + f_len);
		sprintf(cd_header, "Content-Disposition: attachment; filename=\"%s\"", filename);
	}

	if (SUCCESS != (status = http_send_header(cd_header))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't send '%s' header", cd_header);
	}
	efree(cd_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_cache_last_modified(time_t, time_t, char *, size_t) */
PHP_HTTP_API STATUS _http_cache_last_modified(const time_t last_modified,
	const time_t send_modified, const char *cache_control, const size_t cc_len TSRMLS_DC)
{
	if (cc_len) {
		http_send_cache_control(cache_control, cc_len);
	}

	if (http_modified_match("HTTP_IF_MODIFIED_SINCE", last_modified)) {
		if (SUCCESS == http_send_status(304)) {
			zend_bailout();
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not send 304 Not Modified");
			return FAILURE;
		}
	}
	return http_send_last_modified(send_modified);
}
/* }}} */

/* {{{ STATUS http_cache_etag(char *, size_t, char *, size_t) */
PHP_HTTP_API STATUS _http_cache_etag(const char *etag, const size_t etag_len,
	const char *cache_control, const size_t cc_len TSRMLS_DC)
{
	if (cc_len) {
		http_send_cache_control(cache_control, cc_len);
	}

	if (etag_len) {
		http_send_etag(etag, etag_len);
		if (http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
			if (SUCCESS == http_send_status(304)) {
				zend_bailout();
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not send 304 Not Modified");
				return FAILURE;
			}
		}
	}

	/* if no etag is given and we didn't already start ob_etaghandler -- start it */
	if (!HTTP_G(etag_started)) {
		if (SUCCESS == http_start_ob_handler(_http_ob_etaghandler, "ob_etaghandler", 4096, 1)) {
			HTTP_G(etag_started) = 1;
			return SUCCESS;
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not start ob_etaghandler");
			return FAILURE;
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ char *http_absolute_uri(char *) */
PHP_HTTP_API char *_http_absolute_uri_ex(
	const char *url,	size_t url_len,
	const char *proto,	size_t proto_len,
	const char *host,	size_t host_len,
	unsigned port TSRMLS_DC)
{
#if defined(PHP_WIN32) || defined(HAVE_NETDB_H)
	struct servent *se;
#endif
	php_url *purl, furl = {NULL};
	size_t full_len = 0;
	zval *zhost = NULL;
	char *scheme = NULL, *URL = ecalloc(1, HTTP_URI_MAXLEN + 1);

	if ((!url || !url_len) && (
			(!(url = SG(request_info).request_uri)) ||
			(!(url_len = strlen(SG(request_info).request_uri))))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Cannot build an absolute URI if supplied URL and REQUEST_URI is empty");
		return NULL;
	}

	if (!(purl = php_url_parse(url))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not parse supplied URL");
		return NULL;
	}

	furl.user		= purl->user;
	furl.pass		= purl->pass;
	furl.path		= purl->path;
	furl.query		= purl->query;
	furl.fragment	= purl->fragment;

	if (proto) {
		furl.scheme = scheme = estrdup(proto);
	} else if (purl->scheme) {
		furl.scheme = purl->scheme;
#if defined(PHP_WIN32) || defined(HAVE_NETDB_H)
	} else if (port && (se = getservbyport(port, "tcp"))) {
		furl.scheme = (scheme = estrdup(se->s_name));
#endif
	} else {
		furl.scheme = "http";
	}

	if (port) {
		furl.port = port;
	} else if (purl->port) {
		furl.port = purl->port;
	} else if (strncmp(furl.scheme, "http", 4)) {
#if defined(PHP_WIN32) || defined(HAVE_NETDB_H)
		if (se = getservbyname(furl.scheme, "tcp")) {
			furl.port = se->s_port;
		} else
#endif
		furl.port = 80;
	} else {
		furl.port = (furl.scheme[5] == 's') ? 443 : 80;
	}

	if (host) {
		furl.host = (char *) host;
	} else if (purl->host) {
		furl.host = purl->host;
	} else if (	(zhost = http_get_server_var("HTTP_HOST")) ||
				(zhost = http_get_server_var("SERVER_NAME"))) {
		furl.host = Z_STRVAL_P(zhost);
	} else {
		furl.host = "localhost";
	}

#define HTTP_URI_STRLCATS(URL, full_len, add_string) HTTP_URI_STRLCAT(URL, full_len, add_string, sizeof(add_string)-1)
#define HTTP_URI_STRLCATL(URL, full_len, add_string) HTTP_URI_STRLCAT(URL, full_len, add_string, strlen(add_string))
#define HTTP_URI_STRLCAT(URL, full_len, add_string, add_len) \
	if ((full_len += add_len) > HTTP_URI_MAXLEN) { \
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, \
			"Absolute URI would have exceeded max URI length (%d bytes) - " \
			"tried to add %d bytes ('%s')", \
			HTTP_URI_MAXLEN, add_len, add_string); \
		if (scheme) { \
			efree(scheme); \
		} \
		php_url_free(purl); \
		return URL; \
	} else { \
		strcat(URL, add_string); \
	}

	HTTP_URI_STRLCATL(URL, full_len, furl.scheme);
	HTTP_URI_STRLCATS(URL, full_len, "://");

	if (furl.user) {
		HTTP_URI_STRLCATL(URL, full_len, furl.user);
		if (furl.pass) {
			HTTP_URI_STRLCATS(URL, full_len, ":");
			HTTP_URI_STRLCATL(URL, full_len, furl.pass);
		}
		HTTP_URI_STRLCATS(URL, full_len, "@");
	}

	HTTP_URI_STRLCATL(URL, full_len, furl.host);

	if (	(!strcmp(furl.scheme, "http") && (furl.port != 80)) ||
			(!strcmp(furl.scheme, "https") && (furl.port != 443))) {
		char port_string[8] = {0};
		snprintf(port_string, 7, ":%u", furl.port);
		HTTP_URI_STRLCATL(URL, full_len, port_string);
	}

	if (furl.path) {
		if (furl.path[0] != '/') {
			HTTP_URI_STRLCATS(URL, full_len, "/");
		}
		HTTP_URI_STRLCATL(URL, full_len, furl.path);
	} else {
		HTTP_URI_STRLCATS(URL, full_len, "/");
	}

	if (furl.query) {
		HTTP_URI_STRLCATS(URL, full_len, "?");
		HTTP_URI_STRLCATL(URL, full_len, furl.query);
	}

	if (furl.fragment) {
		HTTP_URI_STRLCATS(URL, full_len, "#");
		HTTP_URI_STRLCATL(URL, full_len, furl.fragment);
	}

	if (scheme) {
		efree(scheme);
	}
	php_url_free(purl);

	return URL;
}
/* }}} */

/* {{{ char *http_negotiate_q(char *, zval *, char *, hash_entry_type) */
PHP_HTTP_API char *_http_negotiate_q(const char *entry, const zval *supported,
	const char *def TSRMLS_DC)
{
	zval *zaccept, *zarray, *zdelim, **zentry, *zentries, **zsupp;
	char *q_ptr, *result;
	int i, c;
	double qual;

	HTTP_GSC(zaccept, entry, estrdup(def));

	MAKE_STD_ZVAL(zarray);
	array_init(zarray);

	MAKE_STD_ZVAL(zdelim);
	ZVAL_STRING(zdelim, ",", 0);
	php_explode(zdelim, zaccept, zarray, -1);
	efree(zdelim);

	MAKE_STD_ZVAL(zentries);
	array_init(zentries);

	c = zend_hash_num_elements(Z_ARRVAL_P(zarray));
	for (i = 0; i < c; i++, zend_hash_move_forward(Z_ARRVAL_P(zarray))) {

		if (SUCCESS != zend_hash_get_current_data(
				Z_ARRVAL_P(zarray), (void **) &zentry)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Cannot parse %s header: %s", entry, Z_STRVAL_P(zaccept));
			break;
		}

		/* check for qualifier */
		if (NULL != (q_ptr = strrchr(Z_STRVAL_PP(zentry), ';'))) {
			qual = strtod(q_ptr + 3, NULL);
		} else {
			qual = 1000.0 - i;
		}

		/* walk through the supported array */
		FOREACH_VAL(supported, zsupp) {
			if (!strcasecmp(Z_STRVAL_PP(zsupp), Z_STRVAL_PP(zentry))) {
				add_assoc_double(zentries, Z_STRVAL_PP(zsupp), qual);
				break;
			}
		}
	}

	zval_dtor(zarray);
	efree(zarray);

	zend_hash_internal_pointer_reset(Z_ARRVAL_P(zentries));

	if (	(SUCCESS != zend_hash_sort(Z_ARRVAL_P(zentries), zend_qsort,
					http_sort_q, 0 TSRMLS_CC)) ||
			(HASH_KEY_NON_EXISTANT == zend_hash_get_current_key(
					Z_ARRVAL_P(zentries), &result, 0, 1))) {
		result = estrdup(def);
	}

	zval_dtor(zentries);
	efree(zentries);

	return result;
}
/* }}} */

/* {{{ http_range_status http_get_request_ranges(HashTable *ranges, size_t) */
PHP_HTTP_API http_range_status _http_get_request_ranges(HashTable *ranges,
	const size_t length TSRMLS_DC)
{
	zval *zrange;
	char *range, c;
	long begin = -1, end = -1, *ptr;

	HTTP_GSC(zrange, "HTTP_RANGE", RANGE_NO);
	range = Z_STRVAL_P(zrange);

	if (strncmp(range, "bytes=", sizeof("bytes=") - 1)) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Range header misses bytes=");
		return RANGE_NO;
	}

	ptr = &begin;
	range += sizeof("bytes=") - 1;

	do {
		switch (c = *(range++))
		{
			case '0':
				*ptr *= 10;
			break;

			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				/*
				 * If the value of the pointer is already set (non-negative)
				 * then multiply its value by ten and add the current value,
				 * else initialise the pointers value with the current value
				 * --
				 * This let us recognize empty fields when validating the
				 * ranges, i.e. a "-10" for begin and "12345" for the end
				 * was the following range request: "Range: bytes=0-12345";
				 * While a "-1" for begin and "12345" for the end would
				 * have been: "Range: bytes=-12345".
				 */
				if (*ptr > 0) {
					*ptr *= 10;
					*ptr += c - '0';
				} else {
					*ptr = c - '0';
				}
			break;

			case '-':
				ptr = &end;
			break;

			case ' ':
				/* IE - ignore for now */
			break;

			case 0:
			case ',':

				if (length) {
					/* validate ranges */
					switch (begin)
					{
						/* "0-12345" */
						case -10:
							if ((length - end) < 1) {
								return RANGE_ERR;
							}
							begin = 0;
						break;

						/* "-12345" */
						case -1:
							if ((length - end) < 1) {
								return RANGE_ERR;
							}
							begin = length - end;
							end = length;
						break;

						/* "12345-(xxx)" */
						default:
							switch (end)
							{
								/* "12345-" */
								case -1:
									if ((length - begin) < 1) {
										return RANGE_ERR;
									}
									end = length - 1;
								break;

								/* "12345-67890" */
								default:
									if (	((length - begin) < 1) ||
											((length - end) < 1) ||
											((begin - end) >= 0)) {
										return RANGE_ERR;
									}
								break;
							}
						break;
					}
				}
				{
					zval *zentry;
					MAKE_STD_ZVAL(zentry);
					array_init(zentry);
					add_index_long(zentry, 0, begin);
					add_index_long(zentry, 1, end);
					zend_hash_next_index_insert(ranges, &zentry, sizeof(zval *), NULL);

					begin = -1;
					end = -1;
					ptr = &begin;
				}
			break;

			default:
				return RANGE_NO;
			break;
		}
	} while (c != 0);

	return RANGE_OK;
}
/* }}} */

/* {{{ STATUS http_send_ranges(HashTable *, void *, size_t, http_send_mode) */
PHP_HTTP_API STATUS _http_send_ranges(HashTable *ranges, const void *data, const size_t size, const http_send_mode mode TSRMLS_DC)
{
	long **begin, **end;
	zval **zrange;

	/* single range */
	if (zend_hash_num_elements(ranges) == 1) {
		char range_header[256] = {0};

		if (SUCCESS != zend_hash_index_find(ranges, 0, (void **) &zrange) ||
			SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 0, (void **) &begin) ||
			SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 1, (void **) &end)) {
			return FAILURE;
		}

		/* Send HTTP 206 Partial Content */
		http_send_status(206);

		/* send content range header */
		snprintf(range_header, 255, "Content-Range: bytes %d-%d/%d", **begin, **end, size);
		http_send_header(range_header);

		/* send requested chunk */
		return http_send_chunk(data, **begin, **end + 1, mode);
	}

	/* multi range */
	else {
		char bound[23] = {0}, preface[1024] = {0},
			multi_header[68] = "Content-Type: multipart/byteranges; boundary=";

		/* Send HTTP 206 Partial Content */
		http_send_status(206);

		/* send multipart/byteranges header */
		snprintf(bound, 22, "--%d%0.9f", time(NULL), php_combined_lcg(TSRMLS_C));
		strncat(multi_header, bound + 2, 21);
		http_send_header(multi_header);

		/* send each requested chunk */
		FOREACH_HASH_VAL(ranges, zrange) {
			if (SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 0, (void **) &begin) ||
				SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 1, (void **) &end)) {
				break;
			}

			snprintf(preface, 1023,
				HTTP_CRLF "%s"
				HTTP_CRLF "Content-Type: %s"
				HTTP_CRLF "Content-Range: bytes %ld-%ld/%ld"
				HTTP_CRLF
				HTTP_CRLF,

				bound,
				HTTP_G(ctype) ? HTTP_G(ctype) : "application/x-octetstream",
				**begin,
				**end,
				size
			);

			php_body_write(preface, strlen(preface) TSRMLS_CC);
			http_send_chunk(data, **begin, **end + 1, mode);
		}

		/* write boundary once more */
		php_body_write(HTTP_CRLF, sizeof(HTTP_CRLF) - 1 TSRMLS_CC);
		php_body_write(bound, strlen(bound) TSRMLS_CC);
		php_body_write("--", 2 TSRMLS_CC);

		return SUCCESS;
	}
}
/* }}} */

/* {{{ STATUS http_send(void *, size_t, http_send_mode) */
PHP_HTTP_API STATUS _http_send(const void *data_ptr, const size_t data_size,
	const http_send_mode data_mode TSRMLS_DC)
{
	int is_range_request = http_is_range_request();

	if (!data_ptr) {
		return FAILURE;
	}
	if (!data_size) {
		return SUCCESS;
	}

	/* etag handling */
	if (HTTP_G(etag_started)) {
		char *etag;
		/* interrupt */
		HTTP_G(etag_started) = 0;
		/* never ever use the output to compute the ETag if http_send() is used */
		php_end_ob_buffer(0, 0 TSRMLS_CC);
		if (!(etag = http_etag(data_ptr, data_size, data_mode))) {
			return FAILURE;
		}

		/* send 304 Not Modified if etag matches */
		if ((!is_range_request) && http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
			efree(etag);
			return http_send_status(304);
		}

		http_send_etag(etag, 32);
		efree(etag);
	}

	/* send 304 Not Modified if last-modified matches*/
    if ((!is_range_request) && http_modified_match("HTTP_IF_MODIFIED_SINCE", HTTP_G(lmod))) {
        return http_send_status(304);
    }

	if (is_range_request) {

		/* only send ranges if entity hasn't changed */
		if (
			((!zend_hash_exists(HTTP_SERVER_VARS, "HTTP_IF_MATCH", 13)) ||
			http_etag_match("HTTP_IF_MATCH", HTTP_G(etag)))
			&&
			((!zend_hash_exists(HTTP_SERVER_VARS, "HTTP_IF_UNMODIFIED_SINCE", 25)) ||
			http_modified_match("HTTP_IF_UNMODIFIED_SINCE", HTTP_G(lmod)))
		) {

			STATUS result = FAILURE;
			HashTable ranges;
			zend_hash_init(&ranges, 0, NULL, ZVAL_PTR_DTOR, 0);

			switch (http_get_request_ranges(&ranges, data_size))
			{
				case RANGE_NO:
					zend_hash_destroy(&ranges);
					/* go ahead and send all */
				break;

				case RANGE_OK:
					result = http_send_ranges(&ranges, data_ptr, data_size, data_mode);
					zend_hash_destroy(&ranges);
					return result;
				break;

				case RANGE_ERR:
					zend_hash_destroy(&ranges);
					http_send_status(416);
					return FAILURE;
				break;

				default:
					return FAILURE;
				break;
			}
		}
	}
	/* send all */
	return http_send_chunk(data_ptr, 0, data_size, data_mode);
}
/* }}} */

/* {{{ STATUS http_send_stream(php_stream *) */
PHP_HTTP_API STATUS _http_send_stream_ex(php_stream *file,
	zend_bool close_stream TSRMLS_DC)
{
	STATUS status;

	if ((!file) || php_stream_stat(file, &HTTP_G(ssb))) {
		return FAILURE;
	}

	status = http_send(file, HTTP_G(ssb).sb.st_size, SEND_RSRC);

	if (close_stream) {
		php_stream_close(file);
	}

	return status;
}
/* }}} */

/* {{{ STATUS http_chunked_decode(char *, size_t, char **, size_t *) */
PHP_HTTP_API STATUS _http_chunked_decode(const char *encoded,
	const size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	const char *e_ptr;
	char *d_ptr;

	*decoded_len = 0;
	*decoded = ecalloc(1, encoded_len);
	d_ptr = *decoded;
	e_ptr = encoded;

	while (((e_ptr - encoded) - encoded_len) > 0) {
		char hex_len[9] = {0};
		size_t chunk_len = 0;
		int i = 0;

		/* read in chunk size */
		while (isxdigit(*e_ptr)) {
			if (i == 9) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Chunk size is too long: 0x%s...", hex_len);
				efree(*decoded);
				return FAILURE;
			}
			hex_len[i++] = *e_ptr++;
		}

		/* reached the end */
		if (!strcmp(hex_len, "0")) {
			break;
		}

		/* new line */
		if (strncmp(e_ptr, HTTP_CRLF, 2)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Invalid character (expected 0x0D 0x0A; got: %x %x)",
				*e_ptr, *(e_ptr + 1));
			efree(*decoded);
			return FAILURE;
		}

		/* hex to long */
		{
			char *error = NULL;
			chunk_len = strtol(hex_len, &error, 16);
			if (error == hex_len) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Invalid chunk size string: '%s'", hex_len);
				efree(*decoded);
				return FAILURE;
			}
		}

		memcpy(d_ptr, e_ptr += 2, chunk_len);
		d_ptr += chunk_len;
		e_ptr += chunk_len + 2;
		*decoded_len += chunk_len;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_split_response(zval *, zval *, zval *) */
PHP_HTTP_API STATUS _http_split_response(zval *response, zval *headers, zval *body TSRMLS_DC)
{
	char *b = NULL;
	size_t l = 0;
	STATUS status = http_split_response_ex(Z_STRVAL_P(response), Z_STRLEN_P(response), Z_ARRVAL_P(headers), &b, &l);
	ZVAL_STRINGL(body, b, l, 0);
	return status;
}
/* }}} */

/* {{{ STATUS http_split_response(char *, size_t, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_split_response_ex(char *response,
	size_t response_len, HashTable *headers, char **body, size_t *body_len TSRMLS_DC)
{
	char *header = response, *real_body = NULL;

	while (0 < (response_len - (response - header + 4))) {
		if (	(*response++ == '\r') &&
				(*response++ == '\n') &&
				(*response++ == '\r') &&
				(*response++ == '\n')) {
			real_body = response;
			break;
		}
	}

	if (real_body && (*body_len = (response_len - (real_body - header)))) {
		*body = ecalloc(1, *body_len + 1);
		memcpy(*body, real_body, *body_len);
	}

	return http_parse_headers_ex(header, real_body ? response_len - *body_len : response_len, headers, 1);
}
/* }}} */

/* {{{ STATUS http_parse_headers(char *, long, zval *) */
PHP_HTTP_API STATUS _http_parse_headers_ex(char *header, int header_len,
	HashTable *headers, zend_bool prettify TSRMLS_DC)
{
	char *colon = NULL, *line = NULL, *begin = header;
	zval array;

	Z_ARRVAL(array) = headers;

	if (header_len < 2) {
		return FAILURE;
	}

	/* status code */
	if (!strncmp(header, "HTTP/1.", 7)) {
		char *end = strstr(header, HTTP_CRLF);
		add_assoc_stringl(&array, "Status",
			header + sizeof("HTTP/1.x ") - 1,
			end - (header + sizeof("HTTP/1.x ") - 1), 1);
		header = end + 2;
	}

	line = header;

	while (header_len >= (line - begin)) {
		int value_len = 0;

		switch (*line++)
		{
			case 0:
				--value_len; /* we don't have CR so value length is one char less */
			case '\n':
				if (colon && ((!(*line - 1)) || ((*line != ' ') && (*line != '\t')))) {

					/* skip empty key */
					if (header != colon) {
						char *key = estrndup(header, colon - header);

						if (prettify) {
							key = pretty_key(key, colon - header, 1, 1);
						}

						value_len += line - colon - 1;

						/* skip leading ws */
						while (isspace(*(++colon))) --value_len;
						/* skip trailing ws */
						while (isspace(colon[value_len - 1])) --value_len;

						if (value_len < 1) {
							/* hm, empty header? */
							add_assoc_stringl(&array, key, "", 0, 1);
						} else {
							add_assoc_stringl(&array, key, colon, value_len, 1);
						}
						efree(key);
					}

					colon = NULL;
					value_len = 0;
					header += line - header;
				}
			break;

			case ':':
				if (!colon) {
					colon = line - 1;
				}
			break;
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ void http_get_request_headers_ex(HashTable *, zend_bool) */
PHP_HTTP_API void _http_get_request_headers_ex(HashTable *headers, zend_bool prettify TSRMLS_DC)
{
    char *key = NULL;
    long idx = 0;
    zval array;

    Z_ARRVAL(array) = headers;

    FOREACH_HASH_KEY(HTTP_SERVER_VARS, key, idx) {
        if (key && !strncmp(key, "HTTP_", 5)) {
            zval **header;

            if (prettify) {
            	key = pretty_key(key + 5, strlen(key) - 5, 1, 1);
            }

            zend_hash_get_current_data(HTTP_SERVER_VARS, (void **) &header);
            add_assoc_stringl(&array, key, Z_STRVAL_PP(header), Z_STRLEN_PP(header), 1);
            key = NULL;
        }
    }
}
/* }}} */

/* {{{ STATUS http_urlencode_hash_ex(HashTable *, int, char **, size_t *) */
PHP_HTTP_API STATUS _http_urlencode_hash_ex(HashTable *hash, int override_argsep,
	char *pre_encoded_data, size_t pre_encoded_len,
	char **encoded_data, size_t *encoded_len TSRMLS_DC)
{
	smart_str qstr = {0};

	if (override_argsep) {
		HTTP_URL_ARGSEP_OVERRIDE;
	}

	if (pre_encoded_len && pre_encoded_data) {
		smart_str_appendl(&qstr, pre_encoded_data, pre_encoded_len);
	}

	if (SUCCESS != php_url_encode_hash_ex(hash, &qstr, NULL, 0, NULL, 0, NULL, 0, NULL TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't encode query data");
		if (qstr.c) {
			efree(qstr.c);
		}
		if (override_argsep) {
			HTTP_URL_ARGSEP_RESTORE;
		}
		return FAILURE;
	}

	if (override_argsep) {
		HTTP_URL_ARGSEP_RESTORE;
	}

	smart_str_0(&qstr);

	*encoded_data = qstr.c;
	if (encoded_len) {
		*encoded_len  = qstr.len;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_auth_header(char *, char*) */
PHP_HTTP_API STATUS _http_auth_header(const char *type, const char *realm TSRMLS_DC)
{
	char realm_header[1024] = {0};
	snprintf(realm_header, 1023, "WWW-Authenticate: %s realm=\"%s\"", type, realm);
	return http_send_status_header(401, realm_header);
}
/* }}} */

/* {{{ STATUS http_auth_credentials(char **, char **) */
PHP_HTTP_API STATUS _http_auth_credentials(char **user, char **pass TSRMLS_DC)
{
	if (strncmp(sapi_module.name, "isapi", 5)) {
		zval *zuser, *zpass;

		HTTP_GSC(zuser, "PHP_AUTH_USER", FAILURE);
		HTTP_GSC(zpass, "PHP_AUTH_PW", FAILURE);

		*user = estrndup(Z_STRVAL_P(zuser), Z_STRLEN_P(zuser));
		*pass = estrndup(Z_STRVAL_P(zpass), Z_STRLEN_P(zpass));

		return SUCCESS;
	} else {
		zval *zauth = NULL;
		HTTP_GSC(zauth, "HTTP_AUTHORIZATION", FAILURE);
		{
			char *decoded, *colon;
			int decoded_len;
			decoded = php_base64_decode(Z_STRVAL_P(zauth), Z_STRLEN_P(zauth),
				&decoded_len);

			if (colon = strchr(decoded + 6, ':')) {
				*user = estrndup(decoded + 6, colon - decoded - 6);
				*pass = estrndup(colon + 1, decoded + decoded_len - colon - 6 - 1);

				return SUCCESS;
			} else {
				return FAILURE;
			}
		}
	}
}
/* }}} */

#ifndef ZEND_ENGINE_2
/* {{{ php_url_encode_hash
	Author: Sara Golemon <pollita@php.net> */
PHP_HTTP_API STATUS php_url_encode_hash_ex(HashTable *ht, smart_str *formstr,
				const char *num_prefix, int num_prefix_len,
				const char *key_prefix, int key_prefix_len,
				const char *key_suffix, int key_suffix_len,
				zval *type TSRMLS_DC)
{
	char *arg_sep = NULL, *key = NULL, *ekey, *newprefix, *p;
	int arg_sep_len, key_len, ekey_len, key_type, newprefix_len;
	ulong idx;
	zval **zdata = NULL, *copyzval;

	if (!ht) {
		return FAILURE;
	}

	if (ht->nApplyCount > 0) {
		/* Prevent recursion */
		return SUCCESS;
	}

	arg_sep = INI_STR("arg_separator.output");
	if (!arg_sep || !strlen(arg_sep)) {
		arg_sep = HTTP_URL_ARGSEP_DEFAULT;
	}
	arg_sep_len = strlen(arg_sep);

	for (zend_hash_internal_pointer_reset(ht);
		(key_type = zend_hash_get_current_key_ex(ht, &key, &key_len, &idx, 0, NULL)) != HASH_KEY_NON_EXISTANT;
		zend_hash_move_forward(ht)
	) {
		if (key_type == HASH_KEY_IS_STRING && key_len && key[key_len-1] == '\0') {
			/* We don't want that trailing NULL */
			key_len -= 1;
		}

#ifdef ZEND_ENGINE_2
		/* handling for private & protected object properties */
		if (key && *key == '\0' && type != NULL) {
			char *tmp;

			zend_object *zobj = zend_objects_get_address(type TSRMLS_CC);
			if (zend_check_property_access(zobj, key TSRMLS_CC) != SUCCESS) {
				/* private or protected property access outside of the class */
				continue;
			}
			zend_unmangle_property_name(key, &tmp, &key);
			key_len = strlen(key);
		}
#endif

		if (zend_hash_get_current_data_ex(ht, (void **)&zdata, NULL) == FAILURE || !zdata || !(*zdata)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error traversing form data array.");
			return FAILURE;
		}
		if (Z_TYPE_PP(zdata) == IS_ARRAY || Z_TYPE_PP(zdata) == IS_OBJECT) {
			if (key_type == HASH_KEY_IS_STRING) {
				ekey = php_url_encode(key, key_len, &ekey_len);
				newprefix_len = key_suffix_len + ekey_len + key_prefix_len + 1;
				newprefix = emalloc(newprefix_len + 1);
				p = newprefix;

				if (key_prefix) {
					memcpy(p, key_prefix, key_prefix_len);
					p += key_prefix_len;
				}

				memcpy(p, ekey, ekey_len);
				p += ekey_len;
				efree(ekey);

				if (key_suffix) {
					memcpy(p, key_suffix, key_suffix_len);
					p += key_suffix_len;
				}

				*(p++) = '[';
				*p = '\0';
			} else {
				/* Is an integer key */
				ekey_len = spprintf(&ekey, 12, "%ld", idx);
				newprefix_len = key_prefix_len + num_prefix_len + ekey_len + key_suffix_len + 1;
				newprefix = emalloc(newprefix_len + 1);
				p = newprefix;

				if (key_prefix) {
					memcpy(p, key_prefix, key_prefix_len);
					p += key_prefix_len;
				}

				memcpy(p, num_prefix, num_prefix_len);
				p += num_prefix_len;

				memcpy(p, ekey, ekey_len);
				p += ekey_len;
				efree(ekey);

				if (key_suffix) {
					memcpy(p, key_suffix, key_suffix_len);
					p += key_suffix_len;
				}
				*(p++) = '[';
				*p = '\0';
			}
			ht->nApplyCount++;
			php_url_encode_hash_ex(HASH_OF(*zdata), formstr, NULL, 0, newprefix, newprefix_len, "]", 1, (Z_TYPE_PP(zdata) == IS_OBJECT ? *zdata : NULL) TSRMLS_CC);
			ht->nApplyCount--;
			efree(newprefix);
		} else if (Z_TYPE_PP(zdata) == IS_NULL || Z_TYPE_PP(zdata) == IS_RESOURCE) {
			/* Skip these types */
			continue;
		} else {
			if (formstr->len) {
				smart_str_appendl(formstr, arg_sep, arg_sep_len);
			}
			/* Simple key=value */
			smart_str_appendl(formstr, key_prefix, key_prefix_len);
			if (key_type == HASH_KEY_IS_STRING) {
				ekey = php_url_encode(key, key_len, &ekey_len);
				smart_str_appendl(formstr, ekey, ekey_len);
				efree(ekey);
			} else {
				/* Numeric key */
				if (num_prefix) {
					smart_str_appendl(formstr, num_prefix, num_prefix_len);
				}
				ekey_len = spprintf(&ekey, 12, "%ld", idx);
				smart_str_appendl(formstr, ekey, ekey_len);
				efree(ekey);
			}
			smart_str_appendl(formstr, key_suffix, key_suffix_len);
			smart_str_appendl(formstr, "=", 1);
			switch (Z_TYPE_PP(zdata)) {
				case IS_STRING:
					ekey = php_url_encode(Z_STRVAL_PP(zdata), Z_STRLEN_PP(zdata), &ekey_len);
					break;
				case IS_LONG:
				case IS_BOOL:
					ekey_len = spprintf(&ekey, 12, "%ld", Z_LVAL_PP(zdata));
					break;
				case IS_DOUBLE:
					ekey_len = spprintf(&ekey, 48, "%.*G", (int) EG(precision), Z_DVAL_PP(zdata));
					break;
				default:
					/* fall back on convert to string */
					MAKE_STD_ZVAL(copyzval);
					*copyzval = **zdata;
					zval_copy_ctor(copyzval);
					convert_to_string_ex(&copyzval);
					ekey = php_url_encode(Z_STRVAL_P(copyzval), Z_STRLEN_P(copyzval), &ekey_len);
					zval_ptr_dtor(&copyzval);
			}
			smart_str_appendl(formstr, ekey, ekey_len);
			efree(ekey);
		}
	}

	return SUCCESS;
}
/* }}} */
#endif /* !ZEND_ENDGINE_2 */

/* }}} public API */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

