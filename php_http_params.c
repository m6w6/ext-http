/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http.h"

PHP_HTTP_API void php_http_params_parse_default_func(void *arg, const char *key, int keylen, const char *val, int vallen TSRMLS_DC)
{
	zval tmp, *entry;
	HashTable *ht = (HashTable *) arg;

	if (ht) {
		INIT_PZVAL_ARRAY(&tmp, ht);

		if (vallen) {
			MAKE_STD_ZVAL(entry);
			array_init(entry);
			if (keylen) {
				add_assoc_stringl_ex(entry, key, keylen + 1, estrndup(val, vallen), vallen, 0);
			} else {
				add_next_index_stringl(entry, val, vallen, 1);
			}
			add_next_index_zval(&tmp, entry);
		} else {
			add_next_index_stringl(&tmp, key, keylen, 1);
		}
	}
}

PHP_HTTP_API STATUS	php_http_params_parse(const char *param, int flags, php_http_params_parse_func_t cb, void *cb_arg TSRMLS_DC)
{
#define ST_QUOTE	1
#define ST_VALUE	2
#define ST_KEY		3
#define ST_ASSIGN	4
#define ST_ADD		5

	int st = ST_KEY, keylen = 0, vallen = 0;
	char *s, *c, *key = NULL, *val = NULL;

	if (!cb) {
		cb = php_http_params_parse_default_func;
	}

	for(c = s = estrdup(param);;) {
	continued:
#if 0
	{
		char *tk = NULL, *tv = NULL;

		if (key) {
			if (keylen) {
				tk= estrndup(key, keylen);
			} else {
				tk = ecalloc(1, 7);
				memcpy(tk, key, 3);
				tk[3]='.'; tk[4]='.'; tk[5]='.';
			}
		}
		if (val) {
			if (vallen) {
				tv = estrndup(val, vallen);
			} else {
				tv = ecalloc(1, 7);
				memcpy(tv, val, 3);
				tv[3]='.'; tv[4]='.'; tv[5]='.';
			}
		}
		fprintf(stderr, "[%6s] %c \"%s=%s\"\n",
				(
						st == ST_QUOTE ? "QUOTE" :
						st == ST_VALUE ? "VALUE" :
						st == ST_KEY ? "KEY" :
						st == ST_ASSIGN ? "ASSIGN" :
						st == ST_ADD ? "ADD":
						"HUH?"
				), *c?*c:'0', tk, tv
		);
		STR_FREE(tk); STR_FREE(tv);
	}
#endif
		switch (st) {
			case ST_QUOTE:
			quote:
				if (*c == '"') {
					if (*(c-1) == '\\') {
						memmove(c-1, c, strlen(c)+1);
						goto quote;
					} else {
						goto add;
					}
				} else {
					if (!val) {
						val = c;
					}
					if (!*c) {
						--val;
						st = ST_ADD;
					}
				}
				break;

			case ST_VALUE:
				switch (*c) {
					case '"':
						if (!val) {
							st = ST_QUOTE;
						}
						break;

					case ' ':
						break;

					case ';':
					case '\0':
						goto add;
						break;
					case ',':
						if (flags & PHP_HTTP_PARAMS_ALLOW_COMMA) {
							goto add;
						}
						/* fallthrough */
					default:
						if (!val) {
							val = c;
						}
						break;
				}
				break;

			case ST_KEY:
				switch (*c) {
					case ',':
						if (flags & PHP_HTTP_PARAMS_ALLOW_COMMA) {
							goto allow_comma;
						}
						/* fallthrough */
					case '\r':
					case '\n':
					case '\t':
					case '\013':
					case '\014':
						goto failure;
						break;

					case ' ':
						if (key) {
							keylen = c - key;
							st = ST_ASSIGN;
						}
						break;

					case ';':
					case '\0':
					allow_comma:
						if (key) {
							keylen = c-- - key;
							st = ST_ADD;
						}
						break;

					case ':':
						if (!(flags & PHP_HTTP_PARAMS_COLON_SEPARATOR)) {
							goto not_separator;
						}
						if (key) {
							keylen = c - key;
							st = ST_VALUE;
						} else {
							goto failure;
						}
						break;

					case '=':
						if (flags & PHP_HTTP_PARAMS_COLON_SEPARATOR) {
							goto not_separator;
						}
						if (key) {
							keylen = c - key;
							st = ST_VALUE;
						} else {
							goto failure;
						}
						break;

					default:
					not_separator:
						if (!key) {
							key = c;
						}
						break;
				}
				break;

			case ST_ASSIGN:
				if (*c == '=') {
					st = ST_VALUE;
				} else if (!*c || *c == ';' || ((flags & PHP_HTTP_PARAMS_ALLOW_COMMA) && *c == ',')) {
					st = ST_ADD;
				} else if (*c != ' ') {
					goto failure;
				}
				break;

			case ST_ADD:
			add:
				if (val) {
					vallen = c - val;
					if (st != ST_QUOTE) {
						while (val[vallen-1] == ' ') --vallen;
					}
				} else {
					val = "";
					vallen = 0;
				}

				cb(cb_arg, key, keylen, val, vallen TSRMLS_CC);

				st = ST_KEY;
				key = val = NULL;
				keylen = vallen = 0;
				break;
		}
		if (*c) {
			++c;
		} else if (st == ST_ADD) {
			goto add;
		} else {
			break;
		}
	}

	efree(s);
	return SUCCESS;

failure:
	if (flags & PHP_HTTP_PARAMS_RAISE_ERROR) {
		php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Unexpected character (%c) at pos %tu of %zu", *c, c-s, strlen(s));
	}
	if (flags & PHP_HTTP_PARAMS_ALLOW_FAILURE) {
		if (st == ST_KEY) {
			if (key) {
				keylen = c - key;
			} else {
				key = c;
			}
		} else {
			--c;
		}
		st = ST_ADD;
		goto continued;
	}
	efree(s);
	return FAILURE;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

