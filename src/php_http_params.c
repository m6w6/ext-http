/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

static php_http_params_token_t def_param_sep = {",", 1}, *def_param_sep_ptr[] = {&def_param_sep, NULL};
static php_http_params_token_t def_arg_sep = {";", 1}, *def_arg_sep_ptr[] = {&def_arg_sep, NULL};
static php_http_params_token_t def_val_sep = {"=", 1}, *def_val_sep_ptr[] = {&def_val_sep, NULL};
static php_http_params_opts_t def_opts = {
	{NULL, 0},
	def_param_sep_ptr,
	def_arg_sep_ptr,
	def_val_sep_ptr,
	{{0}},
	PHP_HTTP_PARAMS_DEFAULT
};

php_http_params_opts_t *php_http_params_opts_default_get(php_http_params_opts_t *opts)
{
	if (!opts) {
		opts = emalloc(sizeof(*opts));
	}

	memcpy(opts, &def_opts, sizeof(def_opts));

	return opts;
}

typedef struct php_http_params_state {
	php_http_params_token_t input;
	php_http_params_token_t param;
	php_http_params_token_t arg;
	php_http_params_token_t val;
	struct {
		zval *param;
		zval *args;
		zval *val;
	} current;
	unsigned quotes:1;
	unsigned escape:1;
	unsigned rfc5987:1;
} php_http_params_state_t;

static inline void sanitize_escaped(zval *zv)
{
	if (Z_STRVAL_P(zv)[0] == '"' && Z_STRVAL_P(zv)[Z_STRLEN_P(zv) - 1] == '"') {
		size_t deq_len = Z_STRLEN_P(zv) - 2;
		char *deq = estrndup(Z_STRVAL_P(zv) + 1, deq_len);

		zval_dtor(zv);
		ZVAL_STR(zv, php_http_cs2zs(deq, deq_len));
	}

	php_stripcslashes(Z_STR_P(zv));
}

static inline zend_string *quote_string(zend_string *zs, zend_bool force)
{
	size_t len = (zs)->len;

#if PHP_VERSION_ID < 70300
	zs = php_addcslashes(zs, 0, ZEND_STRL("\0..\37\173\\\""));
#else
	zs = php_addcslashes(zs, ZEND_STRL("\0..\37\173\\\""));
#endif

	if (force || len != (zs)->len || strpbrk((zs)->val, "()<>@,;:\"[]?={} ")) {
		int len = (zs)->len + 2;

		zs = zend_string_extend(zs, len, 0);

		memmove(&(zs)->val[1], (zs)->val, (zs)->len);
		(zs)->val[0] = '"';
		(zs)->val[len-1] = '"';
		(zs)->val[len] = '\0';

		zend_string_forget_hash_val(zs);
	}

	return zs;
}

/*	if (Z_TYPE_P(zv) == IS_STRING) {
		size_t len = Z_STRLEN_P(zv);
		zend_string *stripped = php_addcslashes(Z_STR_P(zv), 0,
				ZEND_STRL("\0..\37\173\\\""));

		if (len != stripped->len || strpbrk(stripped->val, "()<>@,;:\"[]?={} ")) {
			size_t len = stripped->len + 2;
			char *str = emalloc(len + 1);

			str[0] = '"';
			memcpy(&str[1], stripped->val, stripped->len);
			str[len-1] = '"';
			str[len] = '\0';

			zval_dtor(zv);
			zend_string_release(stripped);
			ZVAL_STR(zv, php_http_cs2zs(str, len));
		} else {
			zval_dtor(zv);
			ZVAL_STR(zv, stripped);
		}
*/

static inline void prepare_escaped(zval *zv)
{
	if (Z_TYPE_P(zv) == IS_STRING) {
		zend_string *str = quote_string(Z_STR_P(zv), 0);

		zval_dtor(zv);
		ZVAL_STR(zv, str);
	} else {
		zval_dtor(zv);
		ZVAL_EMPTY_STRING(zv);
	}
}

static inline void sanitize_urlencoded(zval *zv)
{
	Z_STRLEN_P(zv) = php_url_decode(Z_STRVAL_P(zv), Z_STRLEN_P(zv));
}

static inline void prepare_urlencoded(zval *zv)
{
	zend_string *str = php_raw_url_encode(Z_STRVAL_P(zv), Z_STRLEN_P(zv));

	zval_dtor(zv);
	ZVAL_STR(zv, str);
}

static void sanitize_dimension(zval *zv)
{
	zval arr, tmp, *cur = &arr;
	char *var = NULL, *ptr = Z_STRVAL_P(zv), *end = Z_STRVAL_P(zv) + Z_STRLEN_P(zv);
	long level = 0;

	array_init(&arr);

	while (ptr < end) {
		if (!var) {
			var = ptr;
		}

		switch (*ptr) {
			case '[':
				if (++level > PG(max_input_nesting_level)) {
					zval_ptr_dtor(&arr);
					php_error_docref(NULL, E_WARNING, "Max input nesting level of %ld exceeded", (long) PG(max_input_nesting_level));
					return;
				}
				if (ptr - var == 0) {
					++var;
					break;
				}
				/* no break */

			case ']':

				ZVAL_NULL(&tmp);
				convert_to_array(cur);

				if (ptr - var) {
					char chr = *ptr;
					*ptr = '\0';
					cur = zend_symtable_str_update(Z_ARRVAL_P(cur), var, ptr - var, &tmp);
					*ptr = chr;
				} else {
					cur = zend_hash_next_index_insert(Z_ARRVAL_P(cur), &tmp);
				}

				var = NULL;
				break;
		}

		++ptr;
	}

	if (zend_hash_num_elements(Z_ARRVAL(arr))) {
		zval_dtor(zv);
		ZVAL_COPY_VALUE(zv, &arr);
	} else {
		zval_ptr_dtor(&arr);
	}
}

static inline void shift_key(php_http_buffer_t *buf, char *key_str, size_t key_len, const char *ass, size_t asl, unsigned flags);
static inline void shift_val(php_http_buffer_t *buf, zval *zvalue, const char *vss, size_t vsl, unsigned flags);

static void prepare_dimension(php_http_buffer_t *buf, php_http_buffer_t *keybuf, zval *zvalue, const char *pss, size_t psl, const char *vss, size_t vsl, unsigned flags)
{
	HashTable *ht = HASH_OF(zvalue);
	php_http_arrkey_t key;
	zval *val;
	php_http_buffer_t prefix;

	if (!HT_IS_RECURSIVE(ht)) {
		HT_PROTECT_RECURSION(ht);
		php_http_buffer_init(&prefix);
		php_http_buffer_append(&prefix, keybuf->data, keybuf->used);

		ZEND_HASH_FOREACH_KEY_VAL_IND(ht, key.h, key.key, val)
		{
			if (key.key && !*key.key->val) {
				/* only public properties */
				continue;
			}

			php_http_buffer_appends(&prefix, "[");
			if (key.key) {
				php_http_buffer_append(&prefix, key.key->val, key.key->len);
			} else {
				php_http_buffer_appendf(&prefix, "%lu", key.h);
			}
			php_http_buffer_appends(&prefix, "]");

			if (Z_TYPE_P(val) == IS_ARRAY || Z_TYPE_P(val) == IS_OBJECT) {
				prepare_dimension(buf, &prefix, val, pss, psl, vss, vsl, flags);
			} else {
				zend_string *cpy = zval_get_string(val);
				zval tmp;

				ZVAL_STR(&tmp, cpy);
				shift_key(buf, prefix.data, prefix.used, pss, psl, flags);
				shift_val(buf, &tmp, vss, vsl, flags);
				zend_string_release(cpy);
			}

			php_http_buffer_cut(&prefix, keybuf->used, prefix.used - keybuf->used);
		}
		ZEND_HASH_FOREACH_END();
		HT_UNPROTECT_RECURSION(ht);

		php_http_buffer_dtor(&prefix);
	}
}

static inline void sanitize_key(unsigned flags, const char *str, size_t len, zval *zv, zend_bool *rfc5987)
{
	char *eos;
	zend_string *zs = zend_string_init(str, len, 0);

	zval_dtor(zv);
	ZVAL_STR(zv, php_trim(zs, NULL, 0, 3));
	zend_string_release(zs);

	if (flags & PHP_HTTP_PARAMS_ESCAPED) {
		sanitize_escaped(zv);
	}

	if (!Z_STRLEN_P(zv)) {
		return;
	}

	if (flags & PHP_HTTP_PARAMS_RFC5987) {
		eos = &Z_STRVAL_P(zv)[Z_STRLEN_P(zv)-1];
		if (*eos == '*') {
			*eos = '\0';
			*rfc5987 = 1;
			Z_STRLEN_P(zv) -= 1;
		}
	}

	if (flags & PHP_HTTP_PARAMS_URLENCODED) {
		sanitize_urlencoded(zv);
	}

	if (flags & PHP_HTTP_PARAMS_DIMENSION) {
		sanitize_dimension(zv);
	}
}

static inline void sanitize_rfc5987(zval *zv, char **language, zend_bool *latin1)
{
	char *ptr;

	/* examples:
	 * iso-8850-1'de'bl%f6der%20schei%df%21
	 * utf-8'de-DE'bl%c3%b6der%20schei%c3%9f%21
	 */

	switch (Z_STRVAL_P(zv)[0]) {
	case 'I':
	case 'i':
		if (!strncasecmp(Z_STRVAL_P(zv), "iso-8859-1", lenof("iso-8859-1"))) {
			*latin1 = 1;
			ptr = Z_STRVAL_P(zv) + lenof("iso-8859-1");
			break;
		}
		/* no break */
	case 'U':
	case 'u':
		if (!strncasecmp(Z_STRVAL_P(zv), "utf-8", lenof("utf-8"))) {
			*latin1 = 0;
			ptr = Z_STRVAL_P(zv) + lenof("utf-8");
			break;
		}
		/* no break */
	default:
		return;
	}

	/* extract language */
	if (*ptr == '\'') {
		for (*language = ++ptr; *ptr && *ptr != '\''; ++ptr);
		if (!*ptr) {
			*language = NULL;
			return;
		}
		*language = estrndup(*language, ptr - *language);

		/* remainder */
		ptr = estrdup(++ptr);
		zval_dtor(zv);
		ZVAL_STR(zv, php_http_cs2zs(ptr, strlen(ptr)));
	}
}

static inline void sanitize_rfc5988(char *str, size_t len, zval *zv)
{
	zend_string *zs = zend_string_init(str, len, 0);

	zval_dtor(zv);
	ZVAL_STR(zv, php_trim(zs, " ><", 3, 3));
	zend_string_release(zs);
}

static inline void prepare_rfc5988(zval *zv)
{
	if (Z_TYPE_P(zv) != IS_STRING) {
		zval_dtor(zv);
		ZVAL_EMPTY_STRING(zv);
	}
}

static void utf8encode(zval *zv)
{
	size_t pos, len = 0;
	unsigned char *ptr = (unsigned char *) Z_STRVAL_P(zv);

	while (*ptr) {
		if (*ptr++ >= 0x80) {
			++len;
		}
		++len;
	}

	ptr = safe_emalloc(1, len, 1);
	for (len = 0, pos = 0; len <= Z_STRLEN_P(zv); ++len, ++pos) {
		ptr[pos] = Z_STRVAL_P(zv)[len];
		if ((ptr[pos]) >= 0x80) {
			ptr[pos + 1] = 0x80 | (ptr[pos] & 0x3f);
			ptr[pos] = 0xc0 | ((ptr[pos] >> 6) & 0x1f);
			++pos;
		}
	}
	zval_dtor(zv);
	ZVAL_STR(zv, php_http_cs2zs((char *) ptr, pos-1));
}

static inline void sanitize_value(unsigned flags, const char *str, size_t len, zval *zv, zend_bool rfc5987)
{
	char *language = NULL;
	zend_bool latin1 = 0;
	zend_string *zs = zend_string_init(str, len, 0);

	zval_dtor(zv);
	ZVAL_STR(zv, php_trim(zs, NULL, 0, 3));
	zend_string_release(zs);

	if (rfc5987) {
		sanitize_rfc5987(zv, &language, &latin1);
	}

	if (flags & PHP_HTTP_PARAMS_ESCAPED) {
		sanitize_escaped(zv);
	}

	if ((flags & PHP_HTTP_PARAMS_URLENCODED) || (rfc5987 && language)) {
		sanitize_urlencoded(zv);
	}

	if (rfc5987 && language) {
		zval tmp;

		if (latin1) {
			utf8encode(zv);
		}

		ZVAL_COPY_VALUE(&tmp, zv);
		array_init(zv);
		add_assoc_zval(zv, language, &tmp);
		efree(language);
	}
}

static inline void prepare_key(unsigned flags, char *old_key, size_t old_len, char **new_key, size_t *new_len)
{
	zval zv;

	ZVAL_STRINGL(&zv, old_key, old_len);

	if (flags & PHP_HTTP_PARAMS_URLENCODED) {
		prepare_urlencoded(&zv);
	}

	if (flags & PHP_HTTP_PARAMS_ESCAPED) {
		if (flags & PHP_HTTP_PARAMS_RFC5988) {
			prepare_rfc5988(&zv);
		} else {
			prepare_escaped(&zv);
		}
	}

	*new_key = estrndup(Z_STRVAL(zv), Z_STRLEN(zv));
	*new_len = Z_STRLEN(zv);
	zval_ptr_dtor(&zv);
}

static inline void prepare_value(unsigned flags, zval *zv)
{
	if (flags & PHP_HTTP_PARAMS_URLENCODED) {
		prepare_urlencoded(zv);
	}

	if (flags & PHP_HTTP_PARAMS_ESCAPED) {
		prepare_escaped(zv);
	}
}

static void merge_param(HashTable *params, zval *zdata, zval **current_param, zval **current_args)
{
	zval *ptr, *zdata_ptr;
	php_http_arrkey_t hkey = {0};

#if 0
	{
		zval tmp;
		INIT_PZVAL_ARRAY(&tmp, params);
		fprintf(stderr, "params = ");
		zend_print_zval_r(&tmp, 1);
		fprintf(stderr, "\n");
	}
#endif

	zend_hash_get_current_key(Z_ARRVAL_P(zdata), &hkey.key, &hkey.h);

	if ((hkey.key && !zend_hash_exists(params, hkey.key))
	||	(!hkey.key && !zend_hash_index_exists(params, hkey.h))
	) {
		zval tmp, arg, *args;

		/* create the entry if it doesn't exist */
		ptr = zend_hash_get_current_data(Z_ARRVAL_P(zdata));
		Z_TRY_ADDREF_P(ptr);
		array_init(&tmp);
		add_assoc_zval_ex(&tmp, ZEND_STRL("value"), ptr);

		array_init(&arg);
		args = zend_hash_str_update(Z_ARRVAL(tmp), "arguments", lenof("arguments"), &arg);
		*current_args = args;

		if (hkey.key) {
			ptr = zend_hash_update(params, hkey.key, &tmp);
		} else {
			ptr = zend_hash_index_update(params, hkey.h, &tmp);
		}
	} else {
		/* merge */
		if (hkey.key) {
			ptr = zend_hash_find(params, hkey.key);
		} else {
			ptr = zend_hash_index_find(params, hkey.h);
		}

		zdata_ptr = zdata;

		if (Z_TYPE_P(ptr) == IS_ARRAY
		&&	(ptr = zend_hash_str_find(Z_ARRVAL_P(ptr), "value", lenof("value")))
		&&	(zdata_ptr = zend_hash_get_current_data(Z_ARRVAL_P(zdata_ptr)))
		) {
			/*
			 * params = [arr => [value => [0 => 1]]]
			 *                            ^- ptr
			 * zdata  = [arr => [0 => NULL]]
			 *                  ^- zdata_ptr
			 */
			zval *test_ptr;

			while (Z_TYPE_P(zdata_ptr) == IS_ARRAY && (test_ptr = zend_hash_get_current_data(Z_ARRVAL_P(zdata_ptr)))) {
				if (Z_TYPE_P(test_ptr) == IS_ARRAY && Z_TYPE_P(ptr) == IS_ARRAY) {
					zval *tmp_ptr = ptr;

					/* now find key in ptr */
					if (HASH_KEY_IS_STRING == zend_hash_get_current_key(Z_ARRVAL_P(zdata_ptr), &hkey.key, &hkey.h)) {
						if ((ptr = zend_hash_find(Z_ARRVAL_P(ptr), hkey.key))) {
							zdata_ptr = test_ptr;
						} else {
							ptr = tmp_ptr;
							Z_TRY_ADDREF_P(test_ptr);
							ptr = zend_hash_update(Z_ARRVAL_P(ptr), hkey.key, test_ptr);
							break;
						}
					} else {
						if ((ptr = zend_hash_index_find(Z_ARRVAL_P(ptr), hkey.h))) {
							zdata_ptr = test_ptr;
						} else if (hkey.h) {
							ptr = tmp_ptr;
							Z_TRY_ADDREF_P(test_ptr);
							ptr = zend_hash_index_update(Z_ARRVAL_P(ptr), hkey.h, test_ptr);
							break;
						} else {
							ptr = tmp_ptr;
							Z_TRY_ADDREF_P(test_ptr);
							ptr = zend_hash_next_index_insert(Z_ARRVAL_P(ptr), test_ptr);
							break;
						}
					}
				} else {
					/* this is the leaf */
					Z_TRY_ADDREF_P(test_ptr);
					if (Z_TYPE_P(ptr) != IS_ARRAY) {
						zval_dtor(ptr);
						array_init(ptr);
					}
					if (HASH_KEY_IS_STRING == zend_hash_get_current_key(Z_ARRVAL_P(zdata_ptr), &hkey.key, &hkey.h)) {
						ptr = zend_hash_update(Z_ARRVAL_P(ptr), hkey.key, test_ptr);
					} else if (hkey.h) {
						ptr = zend_hash_index_update(Z_ARRVAL_P(ptr), hkey.h, test_ptr);
					} else {
						ptr = zend_hash_next_index_insert(Z_ARRVAL_P(ptr), test_ptr);
					}
					break;
				}
			}

		}
	}

	/* bubble up */
	while (Z_TYPE_P(ptr) == IS_ARRAY) {
		zval *tmp = zend_hash_get_current_data(Z_ARRVAL_P(ptr));

		if (tmp) {
			ptr = tmp;
		} else {
			break;
		}
	}
	*current_param = ptr;
}

static void push_param(HashTable *params, php_http_params_state_t *state, const php_http_params_opts_t *opts)
{
	if (state->val.str) {
		if (!state->current.val) {
			return;
		} else if (0 < (state->val.len = state->input.str - state->val.str)) {
			sanitize_value(opts->flags, state->val.str, state->val.len, state->current.val, state->rfc5987);
		} else {
			ZVAL_EMPTY_STRING(state->current.val);
		}
		state->rfc5987 = 0;
	} else if (state->arg.str) {
		if (0 < (state->arg.len = state->input.str - state->arg.str)) {
			zval val, key;
			zend_bool rfc5987 = 0;

			ZVAL_NULL(&key);
			sanitize_key(opts->flags, state->arg.str, state->arg.len, &key, &rfc5987);
			state->rfc5987 = rfc5987;
			if (Z_TYPE(key) == IS_STRING && Z_STRLEN(key)) {
				ZVAL_TRUE(&val);

				if (rfc5987) {
					zval *rfc;

					if ((rfc = zend_hash_str_find(Z_ARRVAL_P(state->current.args), ZEND_STRL("*rfc5987*")))) {
						state->current.val = zend_symtable_str_update(Z_ARRVAL_P(rfc), Z_STRVAL(key), Z_STRLEN(key), &val);
					} else {
						zval tmp;

						array_init_size(&tmp, 1);
						state->current.val = zend_symtable_str_update(Z_ARRVAL(tmp), Z_STRVAL(key), Z_STRLEN(key), &val);
						zend_symtable_str_update(Z_ARRVAL_P(state->current.args), ZEND_STRL("*rfc5987*"), &tmp);
					}
				} else {
					state->current.val = zend_symtable_str_update(Z_ARRVAL_P(state->current.args), Z_STRVAL(key), Z_STRLEN(key), &val);
				}
			}
			zval_dtor(&key);
		}
	} else if (state->param.str) {
		if (0 < (state->param.len = state->input.str - state->param.str)) {
			zval prm, arg, val, key;
			zend_bool rfc5987 = 0;

			ZVAL_NULL(&key);
			if (opts->flags & PHP_HTTP_PARAMS_RFC5988) {
				sanitize_rfc5988(state->param.str, state->param.len, &key);
			} else {
				sanitize_key(opts->flags, state->param.str, state->param.len, &key, &rfc5987);
				state->rfc5987 = rfc5987;
			}
			if (Z_TYPE(key) == IS_ARRAY) {
				merge_param(params, &key, &state->current.val, &state->current.args);
			} else if (Z_TYPE(key) == IS_STRING && Z_STRLEN(key)) {
				// FIXME: array_init_size(&prm, 2);
				array_init(&prm);

				if (!Z_ISUNDEF(opts->defval)) {
					ZVAL_COPY_VALUE(&val, &opts->defval);
					zval_copy_ctor(&val);
				} else {
					ZVAL_TRUE(&val);
				}
				if (rfc5987 && (opts->flags & PHP_HTTP_PARAMS_RFC5987)) {
					state->current.val = zend_hash_str_update(Z_ARRVAL(prm), "*rfc5987*", lenof("*rfc5987*"), &val);
				} else {
					state->current.val = zend_hash_str_update(Z_ARRVAL(prm), "value", lenof("value"), &val);
				}
				// FIXME: array_init_size(&arg, 3);
				array_init(&arg);
				state->current.args = zend_hash_str_update(Z_ARRVAL(prm), "arguments", lenof("arguments"), &arg);
				state->current.param = zend_symtable_str_update(params, Z_STRVAL(key), Z_STRLEN(key), &prm);
			}
			zval_ptr_dtor(&key);
		}
	}
}

static inline zend_bool check_str(const char *chk_str, size_t chk_len, const char *sep_str, size_t sep_len) {
	return 0 < sep_len && chk_len >= sep_len && *chk_str == *sep_str && !memcmp(chk_str + 1, sep_str + 1, sep_len - 1);
}

static size_t check_sep(php_http_params_state_t *state, php_http_params_token_t **separators)
{
	php_http_params_token_t **sep = separators;

	if (state->quotes || state->escape) {
		return 0;
	}

	if (sep) while (*sep) {
		if (check_str(state->input.str, state->input.len, (*sep)->str, (*sep)->len)) {
			return (*sep)->len;
		}
		++sep;
	}
	return 0;
}

static void skip_sep(size_t skip, php_http_params_state_t *state, php_http_params_token_t **param, php_http_params_token_t **arg, php_http_params_token_t **val)
{
	size_t sep_len;

	state->input.str += skip;
	state->input.len -= skip;

	while (	(param && (sep_len = check_sep(state, param)))
	||		(arg && (sep_len = check_sep(state, arg)))
	||		(val && (sep_len = check_sep(state, val)))
	) {
		state->input.str += sep_len;
		state->input.len -= sep_len;
	}
}

HashTable *php_http_params_parse(HashTable *params, const php_http_params_opts_t *opts)
{
	php_http_params_state_t state = {{NULL,0}, {NULL,0}, {NULL,0}, {NULL,0}, {NULL,NULL,NULL}, 0, 0};

	state.input.str = opts->input.str;
	state.input.len = opts->input.len;

	if (!params) {
		ALLOC_HASHTABLE(params);
		ZEND_INIT_SYMTABLE(params);
	}

	while (state.input.len) {
		if ((opts->flags & PHP_HTTP_PARAMS_RFC5988) && !state.arg.str) {
			if (*state.input.str == '<') {
				state.quotes = 1;
			} else if (*state.input.str == '>') {
				state.quotes = 0;
			}
		} else if (*state.input.str == '"' && !state.escape) {
			state.quotes = !state.quotes;
		} else {
			state.escape = (*state.input.str == '\\');
		}

		if (!state.param.str) {
			/* initialize */
			skip_sep(0, &state, opts->param, opts->arg, opts->val);
			state.param.str = state.input.str;
		} else {
			size_t sep_len;
			/* are we at a param separator? */
			if (0 < (sep_len = check_sep(&state, opts->param))) {
				push_param(params, &state, opts);

				skip_sep(sep_len, &state, opts->param, opts->arg, opts->val);

				/* start off with a new param */
				state.param.str = state.input.str;
				state.param.len = 0;
				state.arg.str = NULL;
				state.arg.len = 0;
				state.val.str = NULL;
				state.val.len = 0;

				continue;

			} else
			/* are we at an arg separator? */
			if (0 < (sep_len = check_sep(&state, opts->arg))) {
				push_param(params, &state, opts);

				skip_sep(sep_len, &state, NULL, opts->arg, opts->val);

				/* continue with a new arg */
				state.arg.str = state.input.str;
				state.arg.len = 0;
				state.val.str = NULL;
				state.val.len = 0;

				continue;

			} else
			/* are we at a val separator? */
			if (0 < (sep_len = check_sep(&state, opts->val))) {
				/* only handle separator if we're not already reading in a val */
				if (!state.val.str) {
					push_param(params, &state, opts);

					skip_sep(sep_len, &state, NULL, NULL, opts->val);

					state.val.str = state.input.str;
					state.val.len = 0;

					continue;
				}
			}
		}

		if (state.input.len) {
			++state.input.str;
			--state.input.len;
		}
	}
	/* finalize */
	push_param(params, &state, opts);

	return params;
}

static inline void shift_key(php_http_buffer_t *buf, char *key_str, size_t key_len, const char *ass, size_t asl, unsigned flags)
{
	char *str;
	size_t len;

	if (buf->used) {
		php_http_buffer_append(buf, ass, asl);
	}

	prepare_key(flags, key_str, key_len, &str, &len);
	php_http_buffer_append(buf, str, len);
	efree(str);
}

static inline void shift_rfc5987(php_http_buffer_t *buf, zval *zvalue, const char *vss, size_t vsl, unsigned flags)
{
	HashTable *ht = HASH_OF(zvalue);
	zval *zdata, tmp;
	zend_string *zs;
	php_http_arrkey_t key = {0};

	if ((zdata = zend_hash_get_current_data(ht))
	&&	HASH_KEY_NON_EXISTENT != zend_hash_get_current_key(ht, &key.key, &key.h)
	) {
		php_http_arrkey_stringify(&key, NULL);
		php_http_buffer_appendf(buf, "*%.*sutf-8'%.*s'",
				(int) (vsl > INT_MAX ? INT_MAX : vsl), vss,
				(int) (key.key->len > INT_MAX ? INT_MAX : key.key->len), key.key->val);
		php_http_arrkey_dtor(&key);

		if (Z_TYPE_P(zdata) == IS_INDIRECT) {
			zdata = Z_INDIRECT_P(zdata);
		}
		zs = zval_get_string(zdata);
		ZVAL_STR(&tmp, zs);
		prepare_value(flags | PHP_HTTP_PARAMS_URLENCODED, &tmp);
		php_http_buffer_append(buf, Z_STRVAL(tmp), Z_STRLEN(tmp));
		zval_ptr_dtor(&tmp);
	}
}

static inline void shift_rfc5988(php_http_buffer_t *buf, char *key_str, size_t key_len, const char *ass, size_t asl, unsigned flags)
{
	char *str;
	size_t len;

	if (buf->used) {
		php_http_buffer_append(buf, ass, asl);
	}

	prepare_key(flags, key_str, key_len, &str, &len);
	php_http_buffer_appends(buf, "<");
	php_http_buffer_append(buf, str, len);
	php_http_buffer_appends(buf, ">");
	efree(str);
}

static inline void shift_rfc5988_val(php_http_buffer_t *buf, zval *zv, const char *vss, size_t vsl, unsigned flags)
{
	zend_string *str, *zs = zval_get_string(zv);

	str = quote_string(zs, 1);
	zend_string_release(zs);

	php_http_buffer_append(buf, vss, vsl);
	php_http_buffer_append(buf, str->val, str->len);
	zend_string_release(str);
}

static inline void shift_val(php_http_buffer_t *buf, zval *zvalue, const char *vss, size_t vsl, unsigned flags)
{
	zval tmp;
	zend_string *zs;

	switch (Z_TYPE_P(zvalue)) {
	case IS_TRUE:
		break;

	case IS_FALSE:
		php_http_buffer_append(buf, vss, vsl);
		php_http_buffer_appends(buf, "0");
		break;

	default:
		zs = zval_get_string(zvalue);

		ZVAL_STR(&tmp, zs);
		prepare_value(flags, &tmp);
		php_http_buffer_append(buf, vss, vsl);
		php_http_buffer_append(buf, Z_STRVAL(tmp), Z_STRLEN(tmp));

		zval_ptr_dtor(&tmp);
		break;
	}
}

static void shift_arg(php_http_buffer_t *buf, char *key_str, size_t key_len, zval *zvalue, const char *ass, size_t asl, const char *vss, size_t vsl, unsigned flags)
{
	if (Z_TYPE_P(zvalue) == IS_ARRAY || Z_TYPE_P(zvalue) == IS_OBJECT) {
		php_http_arrkey_t key;
		HashTable *ht = HASH_OF(zvalue);
		zval *val;
		zend_bool rfc5987 = !strcmp(key_str, "*rfc5987*");

		if (!rfc5987) {
			shift_key(buf, key_str, key_len, ass, asl, flags);
		}
		ZEND_HASH_FOREACH_KEY_VAL_IND(ht, key.h, key.key, val)
		{
			/* did you mean recursion? */
			php_http_arrkey_stringify(&key, NULL);
			if (rfc5987 && (Z_TYPE_P(val) == IS_ARRAY || Z_TYPE_P(val) == IS_OBJECT)) {
				shift_key(buf, key.key->val, key.key->len, ass, asl, flags);
				shift_rfc5987(buf, val, vss, vsl, flags);
			} else {
				shift_arg(buf, key.key->val, key.key->len, val, ass, asl, vss, vsl, flags);
			}
			php_http_arrkey_dtor(&key);
		}
		ZEND_HASH_FOREACH_END();
	} else {
		shift_key(buf, key_str, key_len, ass, asl, flags);

		if (flags & PHP_HTTP_PARAMS_RFC5988) {
			switch (key_len) {
			case lenof("rel"):
			case lenof("title"):
			case lenof("anchor"):
				/* some args must be quoted */
				if (0 <= php_http_select_str(key_str, 3, "rel", "title", "anchor")) {
					shift_rfc5988_val(buf, zvalue, vss, vsl, flags);
					return;
				}
				break;
			}
		}

		shift_val(buf, zvalue, vss, vsl, flags);
	}
}

static void shift_param(php_http_buffer_t *buf, char *key_str, size_t key_len, zval *zvalue, const char *pss, size_t psl, const char *ass, size_t asl, const char *vss, size_t vsl, unsigned flags, zend_bool rfc5987)
{
	if (Z_TYPE_P(zvalue) == IS_ARRAY || Z_TYPE_P(zvalue) == IS_OBJECT) {
		/* treat as arguments, unless we care for dimensions or rfc5987 */
		if (flags & PHP_HTTP_PARAMS_DIMENSION) {
			php_http_buffer_t *keybuf = php_http_buffer_from_string(key_str, key_len);
			prepare_dimension(buf, keybuf, zvalue, pss, psl, vss, vsl, flags);
			php_http_buffer_free(&keybuf);
		} else if (rfc5987) {
			shift_key(buf, key_str, key_len, pss, psl, flags);
			shift_rfc5987(buf, zvalue, vss, vsl, flags);
		} else {
			shift_arg(buf, key_str, key_len, zvalue, ass, asl, vss, vsl, flags);
		}
	} else {
		if (flags & PHP_HTTP_PARAMS_RFC5988) {
			shift_rfc5988(buf, key_str, key_len, pss, psl, flags);
		} else {
			shift_key(buf, key_str, key_len, pss, psl, flags);
		}
		shift_val(buf, zvalue, vss, vsl, flags);
	}
}

php_http_buffer_t *php_http_params_to_string(php_http_buffer_t *buf, HashTable *params, const char *pss, size_t psl, const char *ass, size_t asl, const char *vss, size_t vsl, unsigned flags)
{
	zval *zparam;
	php_http_arrkey_t key;
	zend_bool rfc5987 = 0;

	if (!buf) {
		buf = php_http_buffer_init(NULL);
	}

	ZEND_HASH_FOREACH_KEY_VAL(params, key.h, key.key, zparam)
	{
		zval *zvalue, *zargs;

		if (Z_TYPE_P(zparam) != IS_ARRAY) {
			zvalue = zparam;
		} else {
			if (!(zvalue = zend_hash_str_find(Z_ARRVAL_P(zparam), ZEND_STRL("value")))) {
				if (!(zvalue = zend_hash_str_find(Z_ARRVAL_P(zparam), ZEND_STRL("*rfc5987*")))) {
					zvalue = zparam;
				} else {
					rfc5987 = 1;
				}
			}
		}

		php_http_arrkey_stringify(&key, NULL);
		shift_param(buf, key.key->val, key.key->len, zvalue, pss, psl, ass, asl, vss, vsl, flags, rfc5987);
		php_http_arrkey_dtor(&key);

		if (Z_TYPE_P(zparam) == IS_ARRAY) {
			zval *tmp = zend_hash_str_find(Z_ARRVAL_P(zparam), ZEND_STRL("arguments"));

			if (tmp) {
				zvalue = tmp;
			} else if (zvalue == zparam) {
				continue;
			} else {
				zvalue = zparam;
			}
		}

		if (Z_TYPE_P(zvalue) == IS_ARRAY) {
			ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(zvalue), key.h, key.key, zargs)
			{
				if (zvalue == zparam && key.key && zend_string_equals_literal(key.key, "value")) {
					continue;
				}

				php_http_arrkey_stringify(&key, NULL);
				shift_arg(buf, key.key->val, key.key->len, zargs, ass, asl, vss, vsl, flags);
				php_http_arrkey_dtor(&key);
			}
			ZEND_HASH_FOREACH_END();
		}
	}
	ZEND_HASH_FOREACH_END();

	php_http_buffer_shrink(buf);
	php_http_buffer_fix(buf);

	return buf;
}

php_http_params_token_t **php_http_params_separator_init(zval *zv)
{
	zval *sep, ztmp;
	php_http_params_token_t **ret, **tmp;

	if (!zv) {
		return NULL;
	}

	ZVAL_DUP(&ztmp, zv);
	zv = &ztmp;
	convert_to_array(zv);

	ret = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(zv)) + 1, sizeof(*ret));

	tmp = ret;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(zv), sep)
	{
		zend_string *zs = zval_get_string(sep);

		if (zs->len) {
			*tmp = emalloc(sizeof(**tmp));
			(*tmp)->str = estrndup(zs->val, (*tmp)->len = zs->len);
			++tmp;
		}
		zend_string_release(zs);
	}
	ZEND_HASH_FOREACH_END();

	zval_ptr_dtor(&ztmp);

	*tmp = NULL;
	return ret;
}

void php_http_params_separator_free(php_http_params_token_t **separator)
{
	php_http_params_token_t **sep = separator;
	if (sep) {
		while (*sep) {
			PTR_FREE((*sep)->str);
			efree(*sep);
			++sep;
		}
		efree(separator);
	}
}

static zend_class_entry *php_http_params_class_entry;
zend_class_entry *php_http_params_get_class_entry(void)
{
	return php_http_params_class_entry;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpParams___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, params)
	ZEND_ARG_INFO(0, param_sep)
	ZEND_ARG_INFO(0, arg_sep)
	ZEND_ARG_INFO(0, val_sep)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpParams, __construct)
{
	zval *zparams = NULL, *param_sep = NULL, *arg_sep = NULL, *val_sep = NULL;
	zend_long flags = PHP_HTTP_PARAMS_DEFAULT;
	zend_error_handling zeh;
	zend_string *zs;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|z!/z/z/z/l", &zparams, &param_sep, &arg_sep, &val_sep, &flags), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_get_exception_runtime_class_entry(), &zeh);
	{
		switch (ZEND_NUM_ARGS()) {
			case 5:
				zend_update_property_long(php_http_params_class_entry, getThis(), ZEND_STRL("flags"), flags);
				/* no break */
			case 4:
				zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), val_sep);
				/* no break */
			case 3:
				zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), arg_sep);
				/* no break */
			case 2:
				zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), param_sep);
				/* no break */
		}

		if (zparams) {
			switch (Z_TYPE_P(zparams)) {
				case IS_OBJECT:
				case IS_ARRAY:
					convert_to_array(zparams);
					zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), zparams);
					break;
				default:
					zs = zval_get_string(zparams);
					if (zs->len) {
						zval tmp;

						php_http_params_opts_t opts = {
							{zs->val, zs->len},
							php_http_params_separator_init(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), 0, &tmp)),
							php_http_params_separator_init(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), 0, &tmp)),
							php_http_params_separator_init(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), 0, &tmp)),
							{{0}}, flags
						};

						array_init(&tmp);
						php_http_params_parse(Z_ARRVAL(tmp), &opts);
						zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), &tmp);
						zval_ptr_dtor(&tmp);

						php_http_params_separator_free(opts.param);
						php_http_params_separator_free(opts.arg);
						php_http_params_separator_free(opts.val);
					}
					zend_string_release(zs);
					break;
			}
		} else {
			zval tmp;

			array_init(&tmp);
			zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), &tmp);
			zval_ptr_dtor(&tmp);
		}
	}
	zend_restore_error_handling(&zeh);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpParams_toArray, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpParams, toArray)
{
	zval zparams_tmp, *zparams;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}
	zparams = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0, &zparams_tmp);
	RETURN_ZVAL(zparams, 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpParams_toString, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpParams, toString)
{
	zval *tmp, *zparams, *zpsep, *zasep, *zvsep;
	zval zparams_tmp, flags_tmp, psep_tmp, asep_tmp, vsep_tmp;
	zend_string *psep, *asep, *vsep;
	long flags;
	php_http_buffer_t buf;

	zparams = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0, &zparams_tmp);
	convert_to_array_ex(zparams);
	flags = zval_get_long(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("flags"), 0, &flags_tmp));

	zpsep = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), 0, &psep_tmp);
	if (Z_TYPE_P(zpsep) == IS_ARRAY && (tmp = zend_hash_get_current_data(Z_ARRVAL_P(zpsep)))) {
		psep = zval_get_string(tmp);
	} else {
		psep = zval_get_string(zpsep);
	}
	zasep = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), 0, &asep_tmp);
	if (Z_TYPE_P(zasep) == IS_ARRAY && (tmp = zend_hash_get_current_data(Z_ARRVAL_P(zasep)))) {
		asep = zval_get_string(tmp);
	} else {
		asep = zval_get_string(zasep);
	}
	zvsep = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), 0, &vsep_tmp);
	if (Z_TYPE_P(zvsep) == IS_ARRAY && (tmp = zend_hash_get_current_data(Z_ARRVAL_P(zvsep)))) {
		vsep = zval_get_string(tmp);
	} else {
		vsep = zval_get_string(zvsep);
	}

	php_http_buffer_init(&buf);
	php_http_params_to_string(&buf, Z_ARRVAL_P(zparams), psep->val, psep->len, asep->val, asep->len, vsep->val, vsep->len, flags);

	zend_string_release(psep);
	zend_string_release(asep);
	zend_string_release(vsep);

	RETVAL_STR(php_http_cs2zs(buf.data, buf.used));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpParams_offsetExists, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpParams, offsetExists)
{
	zend_string *name;
	zval zparams_tmp, *zparam, *zparams;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name)) {
		return;
	}

	zparams = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0, &zparams_tmp);

	if (Z_TYPE_P(zparams) == IS_ARRAY && (zparam = zend_symtable_find(Z_ARRVAL_P(zparams), name))) {
		RETVAL_BOOL(Z_TYPE_P(zparam) != IS_NULL);
	} else {
		RETVAL_FALSE;
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpParams_offsetGet, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpParams, offsetGet)
{
	zend_string *name;
	zval zparams_tmp, *zparam, *zparams;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name)) {
		return;
	}

	zparams = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0, &zparams_tmp);

	if (Z_TYPE_P(zparams) == IS_ARRAY && (zparam = zend_symtable_find(Z_ARRVAL_P(zparams), name))) {
		RETVAL_ZVAL(zparam, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpParams_offsetUnset, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpParams, offsetUnset)
{
	zend_string *name;
	zval zparams_tmp, *zparams;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name)) {
		return;
	}

	zparams = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0, &zparams_tmp);

	if (Z_TYPE_P(zparams) == IS_ARRAY) {
		zend_symtable_del(Z_ARRVAL_P(zparams), name);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpParams_offsetSet, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpParams, offsetSet)
{
	zend_string *name;
	zval zparams_tmp, *zparam, *zparams, *nvalue;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &nvalue)) {
		return;
	}

	zparams = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0, &zparams_tmp);
	convert_to_array(zparams);

	if (name->len) {
		if (Z_TYPE_P(nvalue) == IS_ARRAY) {
			if ((zparam = zend_symtable_find(Z_ARRVAL_P(zparams), name))) {
				convert_to_array(zparam);
				array_join(Z_ARRVAL_P(nvalue), Z_ARRVAL_P(zparam), 0, 0);
			} else {
				Z_TRY_ADDREF_P(nvalue);
				add_assoc_zval_ex(zparams, name->val, name->len, nvalue);
			}
		} else {
			zval tmp;

			if ((zparam = zend_symtable_find(Z_ARRVAL_P(zparams), name))) {
				ZVAL_DUP(&tmp, zparam);
				convert_to_array(&tmp);
			} else {
				array_init(&tmp);
			}

			Z_TRY_ADDREF_P(nvalue);
			add_assoc_zval_ex(&tmp, ZEND_STRL("value"), nvalue);
			add_assoc_zval_ex(zparams, name->val, name->len, &tmp);
		}
	} else {
		zval arr;
		zend_string *zs = zval_get_string(nvalue);

		array_init(&arr);
		add_assoc_bool_ex(&arr, ZEND_STRL("value"), 1);
		add_assoc_zval_ex(zparams, zs->val, zs->len, &arr);
		zend_string_release(zs);
	}
}

static zend_function_entry php_http_params_methods[] = {
	PHP_ME(HttpParams, __construct,   ai_HttpParams___construct,   ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)

	PHP_ME(HttpParams, toArray,       ai_HttpParams_toArray,       ZEND_ACC_PUBLIC)
	PHP_ME(HttpParams, toString,      ai_HttpParams_toString,      ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpParams, __toString, toString, ai_HttpParams_toString, ZEND_ACC_PUBLIC)

	PHP_ME(HttpParams, offsetExists,  ai_HttpParams_offsetExists,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpParams, offsetUnset,   ai_HttpParams_offsetUnset,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpParams, offsetSet,     ai_HttpParams_offsetSet,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpParams, offsetGet,     ai_HttpParams_offsetGet,     ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_params)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Params", php_http_params_methods);
	php_http_params_class_entry = zend_register_internal_class(&ce);
	php_http_params_class_entry->create_object = php_http_params_object_new;
	zend_class_implements(php_http_params_class_entry, 1, zend_ce_arrayaccess);

	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_PARAM_SEP"), ZEND_STRL(","));
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_ARG_SEP"), ZEND_STRL(";"));
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_VAL_SEP"), ZEND_STRL("="));
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("COOKIE_PARAM_SEP"), ZEND_STRL(""));

	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_RAW"), PHP_HTTP_PARAMS_RAW);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_ESCAPED"), PHP_HTTP_PARAMS_ESCAPED);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_URLENCODED"), PHP_HTTP_PARAMS_URLENCODED);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_DIMENSION"), PHP_HTTP_PARAMS_DIMENSION);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_RFC5987"), PHP_HTTP_PARAMS_RFC5987);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_RFC5988"), PHP_HTTP_PARAMS_RFC5988);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_DEFAULT"), PHP_HTTP_PARAMS_DEFAULT);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_QUERY"), PHP_HTTP_PARAMS_QUERY);

	zend_declare_property_null(php_http_params_class_entry, ZEND_STRL("params"), ZEND_ACC_PUBLIC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("param_sep"), ZEND_STRL(","), ZEND_ACC_PUBLIC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("arg_sep"), ZEND_STRL(";"), ZEND_ACC_PUBLIC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("val_sep"), ZEND_STRL("="), ZEND_ACC_PUBLIC);
	zend_declare_property_long(php_http_params_class_entry, ZEND_STRL("flags"), PHP_HTTP_PARAMS_DEFAULT, ZEND_ACC_PUBLIC);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

