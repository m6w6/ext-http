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

#include "php_http_api.h"

static php_http_params_token_t def_param_sep = {",", 1}, *def_param_sep_ptr[] = {&def_param_sep, NULL};
static php_http_params_token_t def_arg_sep = {";", 1}, *def_arg_sep_ptr[] = {&def_arg_sep, NULL};
static php_http_params_token_t def_val_sep = {"=", 1}, *def_val_sep_ptr[] = {&def_val_sep, NULL};
static php_http_params_opts_t def_opts = {
	{NULL, 0},
	def_param_sep_ptr,
	def_arg_sep_ptr,
	def_val_sep_ptr,
	NULL,
	PHP_HTTP_PARAMS_DEFAULT
};

PHP_HTTP_API php_http_params_opts_t *php_http_params_opts_default_get(php_http_params_opts_t *opts)
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
		zval **param;
		zval **args;
		zval **val;
	} current;
} php_http_params_state_t;

static inline void sanitize_default(zval *zv TSRMLS_DC)
{
	if (Z_STRVAL_P(zv)[0] == '"' && Z_STRVAL_P(zv)[Z_STRLEN_P(zv) - 1] == '"') {
		size_t deq_len = Z_STRLEN_P(zv) - 2;
		char *deq = estrndup(Z_STRVAL_P(zv) + 1, deq_len);

		zval_dtor(zv);
		ZVAL_STRINGL(zv, deq, deq_len, 0);
	}

	php_stripslashes(Z_STRVAL_P(zv), &Z_STRLEN_P(zv) TSRMLS_CC);
}

static inline void prepare_default(zval *zv TSRMLS_DC)
{
	if (Z_TYPE_P(zv) == IS_STRING) {
		int len = Z_STRLEN_P(zv);

		Z_STRVAL_P(zv) = php_addslashes(Z_STRVAL_P(zv), Z_STRLEN_P(zv), &Z_STRLEN_P(zv), 1 TSRMLS_CC);

		if (len != Z_STRLEN_P(zv)) {
			zval tmp = *zv;
			int len = Z_STRLEN_P(zv) + 2;
			char *str = emalloc(len + 1);

			str[0] = '"';
			memcpy(&str[1], Z_STRVAL_P(zv), Z_STRLEN_P(zv));
			str[len-1] = '"';
			str[len] = '\0';

			zval_dtor(&tmp);
			ZVAL_STRINGL(zv, str, len, 0);
		}
	} else {
		zval_dtor(zv);
		ZVAL_EMPTY_STRING(zv);
	}
}

static inline void sanitize_urlencoded(zval *zv TSRMLS_DC)
{
	Z_STRLEN_P(zv) = php_raw_url_decode(Z_STRVAL_P(zv), Z_STRLEN_P(zv));
}

static inline void prepare_urlencoded(zval *zv TSRMLS_DC)
{
	int len;
	char *str =	php_url_encode(Z_STRVAL_P(zv), Z_STRLEN_P(zv), &len);

	zval_dtor(zv);
	ZVAL_STRINGL(zv, str, len, 0);
}

static void sanitize_dimension(zval *zv TSRMLS_DC)
{
	zval *arr = NULL, *tmp = NULL, **cur = NULL;
	char *var = NULL, *ptr = Z_STRVAL_P(zv), *end = Z_STRVAL_P(zv) + Z_STRLEN_P(zv);
	long level = 0;

	MAKE_STD_ZVAL(arr);
	array_init(arr);
	cur = &arr;

	while (ptr < end) {
		if (!var) {
			var = ptr;
		}

		switch (*ptr) {
			case '[':
				if (++level > PG(max_input_nesting_level)) {
					zval_ptr_dtor(&arr);
					php_http_error(HE_WARNING, PHP_HTTP_E_QUERYSTRING, "Max input nesting level of %ld exceeded", PG(max_input_nesting_level));
					return;
				}
				if (ptr - var == 0) {
					++var;
					break;
				}
				/* no break */

			case ']':

				MAKE_STD_ZVAL(tmp);
				ZVAL_NULL(tmp);
				convert_to_array(*cur);

				if (ptr - var) {
					char chr = *ptr;
					*ptr = '\0';
					zend_symtable_update(Z_ARRVAL_PP(cur), var, ptr - var + 1, (void *) &tmp, sizeof(zval *), (void *) &cur);
					*ptr = chr;
				} else {
					zend_hash_next_index_insert(Z_ARRVAL_PP(cur), (void *) &tmp, sizeof(zval *), (void *) &cur);
				}

				var = NULL;
				break;
		}

		++ptr;
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(arr))) {
		zval_dtor(zv);
#if PHP_VERSION_ID >= 50400
		ZVAL_COPY_VALUE(zv, arr);
#else
		zv->value = arr->value;
		Z_TYPE_P(zv) = Z_TYPE_P(arr);
#endif
		FREE_ZVAL(arr);
	} else {
		zval_ptr_dtor(&arr);
	}
}

static inline void shift_key(php_http_buffer_t *buf, char *key_str, size_t key_len, const char *ass, size_t asl, unsigned flags TSRMLS_DC);
static inline void shift_val(php_http_buffer_t *buf, zval *zvalue, const char *vss, size_t vsl, unsigned flags TSRMLS_DC);

static void prepare_dimension(php_http_buffer_t *buf, php_http_buffer_t *keybuf, zval *zvalue, const char *pss, size_t psl, const char *vss, size_t vsl, unsigned flags TSRMLS_DC)
{
	HashTable *ht = HASH_OF(zvalue);
	HashPosition pos;
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **val;
	php_http_buffer_t prefix;

	if (!ht->nApplyCount++) {
		php_http_buffer_init(&prefix);
		php_http_buffer_append(&prefix, keybuf->data, keybuf->used);

		FOREACH_HASH_KEYVAL(pos, ht, key, val) {
			if (key.type == HASH_KEY_IS_STRING && !*key.str) {
				/* only public properties */
				continue;
			}

			php_http_buffer_appends(&prefix, "[");
			if (key.type == HASH_KEY_IS_STRING) {
				php_http_buffer_append(&prefix, key.str, key.len - 1);
			} else {
				php_http_buffer_appendf(&prefix, "%lu", key.num);
			}
			php_http_buffer_appends(&prefix, "]");

			if (Z_TYPE_PP(val) == IS_ARRAY || Z_TYPE_PP(val) == IS_OBJECT) {
				prepare_dimension(buf, &prefix, *val, pss, psl, vss, vsl, flags TSRMLS_CC);
			} else {
				zval *cpy = php_http_ztyp(IS_STRING, *val);

				shift_key(buf, prefix.data, prefix.used, pss, psl, flags TSRMLS_CC);
				shift_val(buf, cpy, vss, vsl, flags TSRMLS_CC);
				zval_ptr_dtor(&cpy);
			}

			php_http_buffer_cut(&prefix, keybuf->used, prefix.used - keybuf->used);
		}
		php_http_buffer_dtor(&prefix);
	}
	--ht->nApplyCount;
}

static inline void sanitize_key(unsigned flags, char *str, size_t len, zval *zv TSRMLS_DC)
{
	zval_dtor(zv);
	php_trim(str, len, NULL, 0, zv, 3 TSRMLS_CC);

	if (flags & PHP_HTTP_PARAMS_DEFAULT) {
		sanitize_default(zv TSRMLS_CC);
	}

	if (flags & PHP_HTTP_PARAMS_URLENCODED) {
		sanitize_urlencoded(zv TSRMLS_CC);
	}

	if (flags & PHP_HTTP_PARAMS_DIMENSION) {
		sanitize_dimension(zv TSRMLS_CC);
	}
}

static inline void sanitize_value(unsigned flags, char *str, size_t len, zval *zv TSRMLS_DC)
{
	zval_dtor(zv);
	php_trim(str, len, NULL, 0, zv, 3 TSRMLS_CC);

	if (flags & PHP_HTTP_PARAMS_DEFAULT) {
		sanitize_default(zv TSRMLS_CC);
	}

	if (flags & PHP_HTTP_PARAMS_URLENCODED) {
		sanitize_urlencoded(zv TSRMLS_CC);
	}
}

static inline void prepare_key(unsigned flags, char *old_key, size_t old_len, char **new_key, size_t *new_len TSRMLS_DC)
{
	zval zv;

	INIT_PZVAL(&zv);
	ZVAL_STRINGL(&zv, old_key, old_len, 1);

	if (flags & PHP_HTTP_PARAMS_URLENCODED) {
		prepare_urlencoded(&zv TSRMLS_CC);
	}

	if (flags & PHP_HTTP_PARAMS_DEFAULT) {
		prepare_default(&zv TSRMLS_CC);
	}

	*new_key = Z_STRVAL(zv);
	*new_len = Z_STRLEN(zv);
}

static inline void prepare_value(unsigned flags, zval *zv TSRMLS_DC)
{
	if (flags & PHP_HTTP_PARAMS_URLENCODED) {
		prepare_urlencoded(zv TSRMLS_CC);
	}

	if (flags & PHP_HTTP_PARAMS_DEFAULT) {
		prepare_default(zv TSRMLS_CC);
	}
}

static void merge_param(HashTable *params, zval *zdata, zval ***current_param, zval ***current_args TSRMLS_DC)
{
	zval **ptr, **zdata_ptr;
	php_http_array_hashkey_t hkey = php_http_array_hashkey_init(0);

#if 0
	{
		zval tmp;
		INIT_PZVAL_ARRAY(&tmp, params);
		fprintf(stderr, "params = ");
		zend_print_zval_r(&tmp, 1 TSRMLS_CC);
		fprintf(stderr, "\n");
	}
#endif

	hkey.type = zend_hash_get_current_key_ex(Z_ARRVAL_P(zdata), &hkey.str, &hkey.len, &hkey.num, hkey.dup, NULL);

	if ((hkey.type == HASH_KEY_IS_STRING && !zend_hash_exists(params, hkey.str, hkey.len))
	||	(hkey.type == HASH_KEY_IS_LONG && !zend_hash_index_exists(params, hkey.num))
	) {
		zval *tmp, *arg, **args;

		/* create the entry if it doesn't exist */
		zend_hash_get_current_data(Z_ARRVAL_P(zdata), (void *) &ptr);
		Z_ADDREF_PP(ptr);
		MAKE_STD_ZVAL(tmp);
		array_init(tmp);
		add_assoc_zval_ex(tmp, ZEND_STRS("value"), *ptr);

		MAKE_STD_ZVAL(arg);
		array_init(arg);
		zend_hash_update(Z_ARRVAL_P(tmp), "arguments", sizeof("arguments"), (void *) &arg, sizeof(zval *), (void *) &args);
		*current_args = args;

		if (hkey.type == HASH_KEY_IS_STRING) {
			zend_hash_update(params, hkey.str, hkey.len, (void *) &tmp, sizeof(zval *), (void *) &ptr);
		} else {
			zend_hash_index_update(params, hkey.num, (void *) &tmp, sizeof(zval *), (void *) &ptr);
		}
	} else {
		/* merge */
		if (hkey.type == HASH_KEY_IS_STRING) {
			zend_hash_find(params, hkey.str, hkey.len, (void *) &ptr);
		} else {
			zend_hash_index_find(params, hkey.num, (void *) &ptr);
		}

		zdata_ptr = &zdata;

		if (Z_TYPE_PP(ptr) == IS_ARRAY
		&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(ptr), "value", sizeof("value"), (void *) &ptr)
		&&	SUCCESS == zend_hash_get_current_data(Z_ARRVAL_PP(zdata_ptr), (void *) &zdata_ptr)
		) {
			/*
			 * params = [arr => [value => [0 => 1]]]
			 *                            ^- ptr
			 * zdata  = [arr => [0 => NULL]]
			 *                  ^- zdata_ptr
			 */
			zval **test_ptr;

			while (Z_TYPE_PP(zdata_ptr) == IS_ARRAY
			&&	SUCCESS == zend_hash_get_current_data(Z_ARRVAL_PP(zdata_ptr), (void *) &test_ptr)
			) {
				if (Z_TYPE_PP(test_ptr) == IS_ARRAY) {

					/* now find key in ptr */
					if (HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(Z_ARRVAL_PP(zdata_ptr), &hkey.str, &hkey.len, &hkey.num, hkey.dup, NULL)) {
						if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(ptr), hkey.str, hkey.len, (void *) &ptr)) {
							zdata_ptr = test_ptr;
						} else {
							Z_ADDREF_PP(test_ptr);
							zend_hash_update(Z_ARRVAL_PP(ptr), hkey.str, hkey.len, (void *) test_ptr, sizeof(zval *), (void *) &ptr);
							break;
						}
					} else {
						if (SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(ptr), hkey.num, (void *) &ptr)) {
							zdata_ptr = test_ptr;
						} else if (hkey.num) {
							Z_ADDREF_PP(test_ptr);
							zend_hash_index_update(Z_ARRVAL_PP(ptr), hkey.num, (void *) test_ptr, sizeof(zval *), (void *) &ptr);
							break;
						} else {
							Z_ADDREF_PP(test_ptr);
							zend_hash_next_index_insert(Z_ARRVAL_PP(ptr), (void *) test_ptr, sizeof(zval *), (void *) &ptr);
							break;
						}
					}
				} else {
					/* this is the leaf */
					Z_ADDREF_PP(test_ptr);
					if (Z_TYPE_PP(ptr) != IS_ARRAY) {
						zval_dtor(*ptr);
						array_init(*ptr);
					}
					if (HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(Z_ARRVAL_PP(zdata_ptr), &hkey.str, &hkey.len, &hkey.num, hkey.dup, NULL)) {
						zend_hash_update(Z_ARRVAL_PP(ptr), hkey.str, hkey.len, (void *) test_ptr, sizeof(zval *), (void *) &ptr);
					} else if (hkey.num) {
						zend_hash_index_update(Z_ARRVAL_PP(ptr), hkey.num, (void *) test_ptr, sizeof(zval *), (void *) &ptr);
					} else {
						zend_hash_next_index_insert(Z_ARRVAL_PP(ptr), (void *) test_ptr, sizeof(zval *), (void *) &ptr);
					}
					break;
				}
			}

		}
	}

	/* bubble up */
	while (Z_TYPE_PP(ptr) == IS_ARRAY && SUCCESS == zend_hash_get_current_data(Z_ARRVAL_PP(ptr), (void *) &ptr));
	*current_param = ptr;
}

static void push_param(HashTable *params, php_http_params_state_t *state, const php_http_params_opts_t *opts TSRMLS_DC)
{
	if (state->val.str) {
		if (0 < (state->val.len = state->input.str - state->val.str)) {
			sanitize_value(opts->flags, state->val.str, state->val.len, *(state->current.val) TSRMLS_CC);
		}
	} else if (state->arg.str) {
		if (0 < (state->arg.len = state->input.str - state->arg.str)) {
			zval *val, key;

			INIT_PZVAL(&key);
			ZVAL_NULL(&key);
			sanitize_key(opts->flags, state->arg.str, state->arg.len, &key TSRMLS_CC);
			if (Z_TYPE(key) == IS_STRING && Z_STRLEN(key)) {
				MAKE_STD_ZVAL(val);
				ZVAL_TRUE(val);
				zend_symtable_update(Z_ARRVAL_PP(state->current.args), Z_STRVAL(key), Z_STRLEN(key) + 1, (void *) &val, sizeof(zval *), (void *) &state->current.val);
			}
			zval_dtor(&key);
		}
	} else if (state->param.str) {
		if (0 < (state->param.len = state->input.str - state->param.str)) {
			zval *prm, *arg, *val, *key;

			MAKE_STD_ZVAL(key);
			ZVAL_NULL(key);
			sanitize_key(opts->flags, state->param.str, state->param.len, key TSRMLS_CC);
			if (Z_TYPE_P(key) != IS_STRING) {
				merge_param(params, key, &state->current.val, &state->current.args TSRMLS_CC);
			} else if (Z_STRLEN_P(key)) {
				MAKE_STD_ZVAL(prm);
				array_init(prm);

				MAKE_STD_ZVAL(val);
				if (opts->defval) {
#if PHP_VERSION_ID >= 50400
					ZVAL_COPY_VALUE(val, opts->defval);
#else
					val->value = opts->defval->value;
					Z_TYPE_P(val) = Z_TYPE_P(opts->defval);
#endif
					zval_copy_ctor(val);
				} else {
					ZVAL_TRUE(val);
				}
				zend_hash_update(Z_ARRVAL_P(prm), "value", sizeof("value"), (void *) &val, sizeof(zval *), (void *) &state->current.val);

				MAKE_STD_ZVAL(arg);
				array_init(arg);
				zend_hash_update(Z_ARRVAL_P(prm), "arguments", sizeof("arguments"), (void *) &arg, sizeof(zval *), (void *) &state->current.args);

				zend_symtable_update(params, Z_STRVAL_P(key), Z_STRLEN_P(key) + 1, (void *) &prm, sizeof(zval *), (void *) &state->current.param);
			}
			zval_ptr_dtor(&key);
		}
	}
}

static inline zend_bool check_str(const char *chk_str, size_t chk_len, const char *sep_str, size_t sep_len) {
	return 0 < sep_len && chk_len >= sep_len && !memcmp(chk_str, sep_str, sep_len);
}

static size_t check_sep(php_http_params_state_t *state, php_http_params_token_t **separators)
{
	php_http_params_token_t **sep = separators;

	if (sep) while (*sep) {
		if (check_str(state->input.str, state->input.len, (*sep)->str, (*sep)->len)) {
			return (*sep)->len;
		}
		++sep;
	}
	return 0;
}

static void skip_sep(size_t skip, php_http_params_state_t *state, php_http_params_token_t **param, php_http_params_token_t **arg, php_http_params_token_t **val TSRMLS_DC)
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

PHP_HTTP_API HashTable *php_http_params_parse(HashTable *params, const php_http_params_opts_t *opts TSRMLS_DC)
{
	php_http_params_state_t state = {{NULL,0}, {NULL,0}, {NULL,0}, {NULL,0}, {NULL,NULL,NULL}};

	state.input.str = opts->input.str;
	state.input.len = opts->input.len;

	if (!params) {
		ALLOC_HASHTABLE(params);
		ZEND_INIT_SYMTABLE(params);
	}

	while (state.input.len) {
		if (*state.input.str == '\\') {
			++state.input.str;
			--state.input.len;
		} else if (!state.param.str) {
			/* initialize */
			skip_sep(0, &state, opts->param, opts->arg, opts->val TSRMLS_CC);
			state.param.str = state.input.str;
		} else {
			size_t sep_len;
			/* are we at a param separator? */
			if (0 < (sep_len = check_sep(&state, opts->param))) {
				push_param(params, &state, opts TSRMLS_CC);

				skip_sep(sep_len, &state, opts->param, opts->arg, opts->val TSRMLS_CC);

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
				push_param(params, &state, opts TSRMLS_CC);

				skip_sep(sep_len, &state, NULL, opts->arg, opts->val TSRMLS_CC);

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
					push_param(params, &state, opts TSRMLS_CC);

					skip_sep(sep_len, &state, NULL, NULL, opts->val TSRMLS_CC);

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
	push_param(params, &state, opts TSRMLS_CC);

	return params;
}

static inline void shift_key(php_http_buffer_t *buf, char *key_str, size_t key_len, const char *ass, size_t asl, unsigned flags TSRMLS_DC)
{
	char *str;
	size_t len;

	if (buf->used) {
		php_http_buffer_append(buf, ass, asl);
	}

	prepare_key(flags, key_str, key_len, &str, &len TSRMLS_CC);
	php_http_buffer_append(buf, str, len);
	efree(str);
}

static inline void shift_val(php_http_buffer_t *buf, zval *zvalue, const char *vss, size_t vsl, unsigned flags TSRMLS_DC)
{
	if (Z_TYPE_P(zvalue) != IS_BOOL) {
		zval *tmp = php_http_ztyp(IS_STRING, zvalue);

		prepare_value(flags, tmp TSRMLS_CC);
		php_http_buffer_append(buf, vss, vsl);
		php_http_buffer_append(buf, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));

		zval_ptr_dtor(&tmp);
	} else if (!Z_BVAL_P(zvalue)) {
		php_http_buffer_append(buf, vss, vsl);
		php_http_buffer_appends(buf, "0");
	}
}

static void shift_arg(php_http_buffer_t *buf, char *key_str, size_t key_len, zval *zvalue, const char *ass, size_t asl, const char *vss, size_t vsl, unsigned flags TSRMLS_DC)
{
	if (Z_TYPE_P(zvalue) == IS_ARRAY || Z_TYPE_P(zvalue) == IS_OBJECT) {
		HashPosition pos;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		zval **val;

		FOREACH_KEYVAL(pos, zvalue, key, val) {
			/* did you mean recursion? */
			php_http_array_hashkey_stringify(&key);
			shift_arg(buf, key.str, key.len-1, *val, ass, asl, vss, vsl, flags TSRMLS_CC);
			php_http_array_hashkey_stringfree(&key);
		}
	} else {
		shift_key(buf, key_str, key_len, ass, asl, flags TSRMLS_CC);
		shift_val(buf, zvalue, vss, vsl, flags TSRMLS_CC);
	}
}

static void shift_param(php_http_buffer_t *buf, char *key_str, size_t key_len, zval *zvalue, const char *pss, size_t psl, const char *ass, size_t asl, const char *vss, size_t vsl, unsigned flags TSRMLS_DC)
{
	if (Z_TYPE_P(zvalue) == IS_ARRAY || Z_TYPE_P(zvalue) == IS_OBJECT) {
		/* treat as arguments, unless we care for dimensions */
		if (flags & PHP_HTTP_PARAMS_DIMENSION) {
			php_http_buffer_t *keybuf = php_http_buffer_from_string(key_str, key_len);
			prepare_dimension(buf, keybuf, zvalue, pss, psl, vss, vsl, flags TSRMLS_CC);
			php_http_buffer_free(&keybuf);
		} else {
			shift_arg(buf, key_str, key_len, zvalue, ass, asl, vss, vsl, flags TSRMLS_CC);
		}
	} else {
		shift_key(buf, key_str, key_len, pss, psl, flags TSRMLS_CC);
		shift_val(buf, zvalue, vss, vsl, flags TSRMLS_CC);
	}
}

PHP_HTTP_API php_http_buffer_t *php_http_params_to_string(php_http_buffer_t *buf, HashTable *params, const char *pss, size_t psl, const char *ass, size_t asl, const char *vss, size_t vsl, unsigned flags TSRMLS_DC)
{
	zval **zparam;
	HashPosition pos, pos1;
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0), key1 = php_http_array_hashkey_init(0);

	if (!buf) {
		buf = php_http_buffer_init(NULL);
	}

	FOREACH_HASH_KEYVAL(pos, params, key, zparam) {
		zval **zvalue, **zargs;

		if (Z_TYPE_PP(zparam) != IS_ARRAY || SUCCESS != zend_hash_find(Z_ARRVAL_PP(zparam), ZEND_STRS("value"), (void *) &zvalue)) {
			zvalue = zparam;
		}

		php_http_array_hashkey_stringify(&key);
		shift_param(buf, key.str, key.len - 1, *zvalue, pss, psl, ass, asl, vss, vsl, flags TSRMLS_CC);
		php_http_array_hashkey_stringfree(&key);

		if (Z_TYPE_PP(zparam) == IS_ARRAY && SUCCESS != zend_hash_find(Z_ARRVAL_PP(zparam), ZEND_STRS("arguments"), (void *) &zvalue)) {
			if (zvalue == zparam) {
				continue;
			}
			zvalue = zparam;
		}

		if (Z_TYPE_PP(zvalue) == IS_ARRAY) {
			FOREACH_KEYVAL(pos1, *zvalue, key1, zargs) {
				if (zvalue == zparam && key1.type == HASH_KEY_IS_STRING && !strcmp(key1.str, "value")) {
					continue;
				}

				php_http_array_hashkey_stringify(&key1);
				shift_arg(buf, key1.str, key1.len - 1, *zargs, ass, asl, vss, vsl, flags TSRMLS_CC);
				php_http_array_hashkey_stringfree(&key1);
			}
		}
	}

	php_http_buffer_shrink(buf);
	php_http_buffer_fix(buf);

	return buf;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpParams, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpParams, method, 0)
#define PHP_HTTP_PARAMS_ME(method, visibility)	PHP_ME(HttpParams, method, PHP_HTTP_ARGS(HttpParams, method), visibility)
#define PHP_HTTP_PARAMS_GME(method, visibility)	PHP_ME(HttpParams, method, PHP_HTTP_ARGS(HttpParams, __getter), visibility)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(params, 0)
	PHP_HTTP_ARG_VAL(param_sep, 0)
	PHP_HTTP_ARG_VAL(arg_sep, 0)
	PHP_HTTP_ARG_VAL(val_sep, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(toArray);
PHP_HTTP_EMPTY_ARGS(toString);

PHP_HTTP_BEGIN_ARGS(offsetExists, 1)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(offsetUnset, 1)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(offsetGet, 1)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(offsetSet, 2)
	PHP_HTTP_ARG_VAL(name, 0)
	PHP_HTTP_ARG_VAL(value, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_params_class_entry;

zend_class_entry *php_http_params_get_class_entry(void)
{
	return php_http_params_class_entry;
}

static zend_function_entry php_http_params_method_entry[] = {
	PHP_HTTP_PARAMS_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)

	PHP_HTTP_PARAMS_ME(toArray, ZEND_ACC_PUBLIC)
	PHP_HTTP_PARAMS_ME(toString, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpParams, __toString, toString, PHP_HTTP_ARGS(HttpParams, toString), ZEND_ACC_PUBLIC)

	PHP_HTTP_PARAMS_ME(offsetExists, ZEND_ACC_PUBLIC)
	PHP_HTTP_PARAMS_ME(offsetUnset, ZEND_ACC_PUBLIC)
	PHP_HTTP_PARAMS_ME(offsetSet, ZEND_ACC_PUBLIC)
	PHP_HTTP_PARAMS_ME(offsetGet, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_params)
{
	PHP_HTTP_REGISTER_CLASS(http, Params, http_params, php_http_object_get_class_entry(), 0);

	zend_class_implements(php_http_params_class_entry TSRMLS_CC, 1, zend_ce_arrayaccess);

	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_PARAM_SEP"), ZEND_STRL(",") TSRMLS_CC);
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_ARG_SEP"), ZEND_STRL(";") TSRMLS_CC);
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_VAL_SEP"), ZEND_STRL("=") TSRMLS_CC);
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("COOKIE_PARAM_SEP"), ZEND_STRL("") TSRMLS_CC);

	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_RAW"), PHP_HTTP_PARAMS_RAW TSRMLS_CC);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_DEFAULT"), PHP_HTTP_PARAMS_DEFAULT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_URLENCODED"), PHP_HTTP_PARAMS_URLENCODED TSRMLS_CC);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_DIMENSION"), PHP_HTTP_PARAMS_DIMENSION TSRMLS_CC);
	zend_declare_class_constant_long(php_http_params_class_entry, ZEND_STRL("PARSE_QUERY"), PHP_HTTP_PARAMS_QUERY TSRMLS_CC);

	zend_declare_property_null(php_http_params_class_entry, ZEND_STRL("params"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("param_sep"), ZEND_STRL(","), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("arg_sep"), ZEND_STRL(";"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("val_sep"), ZEND_STRL("="), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(php_http_params_class_entry, ZEND_STRL("flags"), PHP_HTTP_PARAMS_DEFAULT, ZEND_ACC_PUBLIC TSRMLS_CC);

	return SUCCESS;
}

PHP_HTTP_API php_http_params_token_t **php_http_params_separator_init(zval *zv TSRMLS_DC)
{
	zval **sep;
	HashPosition pos;
	php_http_params_token_t **ret, **tmp;

	if (!zv) {
		return NULL;
	}

	zv = php_http_ztyp(IS_ARRAY, zv);
	ret = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(zv)) + 1, sizeof(*ret));

	tmp = ret;
	FOREACH_VAL(pos, zv, sep) {
		zval *zt = php_http_ztyp(IS_STRING, *sep);

		if (Z_STRLEN_P(zt)) {
			*tmp = emalloc(sizeof(**tmp));
			(*tmp)->str = estrndup(Z_STRVAL_P(zt), (*tmp)->len = Z_STRLEN_P(zt));
			++tmp;
		}
		zval_ptr_dtor(&zt);
	}
	zval_ptr_dtor(&zv);

	*tmp = NULL;
	return ret;
}

PHP_HTTP_API void php_http_params_separator_free(php_http_params_token_t **separator)
{
	php_http_params_token_t **sep = separator;
	if (sep) {
		while (*sep) {
			STR_FREE((*sep)->str);
			efree(*sep);
			++sep;
		}
		efree(separator);
	}
}

PHP_METHOD(HttpParams, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		zval *zcopy, *zparams = NULL, *param_sep = NULL, *arg_sep = NULL, *val_sep = NULL;
		long flags = PHP_HTTP_PARAMS_DEFAULT;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!/z/z/z/l", &zparams, &param_sep, &arg_sep, &val_sep, &flags)) {
			switch (ZEND_NUM_ARGS()) {
				case 5:
					zend_update_property_long(php_http_params_class_entry, getThis(), ZEND_STRL("flags"), flags TSRMLS_CC);
					/* no break */
				case 4:
					zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), val_sep TSRMLS_CC);
					/* no break */
				case 3:
					zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), arg_sep TSRMLS_CC);
					/* no break */
				case 2:
					zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), param_sep TSRMLS_CC);
					/* no break */
			}

			if (zparams) {
				switch (Z_TYPE_P(zparams)) {
					case IS_OBJECT:
					case IS_ARRAY:
						zcopy = php_http_zsep(1, IS_ARRAY, zparams);
						zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), zcopy TSRMLS_CC);
						zval_ptr_dtor(&zcopy);
						break;
					default:
						zcopy = php_http_ztyp(IS_STRING, zparams);
						if (Z_STRLEN_P(zcopy)) {
							php_http_params_opts_t opts = {
								{Z_STRVAL_P(zcopy), Z_STRLEN_P(zcopy)},
								php_http_params_separator_init(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), 0 TSRMLS_CC) TSRMLS_CC),
								php_http_params_separator_init(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), 0 TSRMLS_CC) TSRMLS_CC),
								php_http_params_separator_init(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), 0 TSRMLS_CC) TSRMLS_CC),
								NULL, flags
							};

							MAKE_STD_ZVAL(zparams);
							array_init(zparams);
							php_http_params_parse(Z_ARRVAL_P(zparams), &opts TSRMLS_CC);
							zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), zparams TSRMLS_CC);
							zval_ptr_dtor(&zparams);

							php_http_params_separator_free(opts.param);
							php_http_params_separator_free(opts.arg);
							php_http_params_separator_free(opts.val);
						}
						zval_ptr_dtor(&zcopy);
						break;
				}
			} else {
				MAKE_STD_ZVAL(zparams);
				array_init(zparams);
				zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), zparams TSRMLS_CC);
				zval_ptr_dtor(&zparams);
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpParams, toArray)
{
	if (SUCCESS != zend_parse_parameters_none()) {
		RETURN_FALSE;
	}
	RETURN_PROP(php_http_params_class_entry, "params");
}

PHP_METHOD(HttpParams, toString)
{
	zval **tmp, *zparams, *zpsep, *zasep, *zvsep, *zflags;
	php_http_buffer_t buf;

	zparams = php_http_zsep(1, IS_ARRAY, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0 TSRMLS_CC));
	zflags = php_http_ztyp(IS_LONG, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("flags"), 0 TSRMLS_CC));

	zpsep = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), 0 TSRMLS_CC);
	if (Z_TYPE_P(zpsep) == IS_ARRAY && SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zpsep), (void *) &tmp)) {
		zpsep = php_http_ztyp(IS_STRING, *tmp);
	} else {
		zpsep = php_http_ztyp(IS_STRING, zpsep);
	}
	zasep = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), 0 TSRMLS_CC);
	if (Z_TYPE_P(zasep) == IS_ARRAY && SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zasep), (void *) &tmp)) {
		zasep = php_http_ztyp(IS_STRING, *tmp);
	} else {
		zasep = php_http_ztyp(IS_STRING, zasep);
	}
	zvsep = zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), 0 TSRMLS_CC);
	if (Z_TYPE_P(zvsep) == IS_ARRAY && SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zvsep), (void *) &tmp)) {
		zvsep = php_http_ztyp(IS_STRING, *tmp);
	} else {
		zvsep = php_http_ztyp(IS_STRING, zvsep);
	}

	php_http_buffer_init(&buf);
	php_http_params_to_string(&buf, Z_ARRVAL_P(zparams), Z_STRVAL_P(zpsep), Z_STRLEN_P(zpsep), Z_STRVAL_P(zasep), Z_STRLEN_P(zasep), Z_STRVAL_P(zvsep), Z_STRLEN_P(zvsep), Z_LVAL_P(zflags) TSRMLS_CC);

	zval_ptr_dtor(&zparams);
	zval_ptr_dtor(&zpsep);
	zval_ptr_dtor(&zasep);
	zval_ptr_dtor(&zvsep);
	zval_ptr_dtor(&zflags);

	RETVAL_PHP_HTTP_BUFFER_VAL(&buf);
}

PHP_METHOD(HttpParams, offsetExists)
{
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name_str, &name_len)) {
		zval **zparam, *zparams = php_http_ztyp(IS_ARRAY, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0 TSRMLS_CC));

		if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(zparams), name_str, name_len + 1, (void *) &zparam)) {
			RETVAL_BOOL(Z_TYPE_PP(zparam) != IS_NULL);
		} else {
			RETVAL_FALSE;
		}
		zval_ptr_dtor(&zparams);
	}
}

PHP_METHOD(HttpParams, offsetGet)
{
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name_str, &name_len)) {
		zval **zparam, *zparams = php_http_ztyp(IS_ARRAY, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0 TSRMLS_CC));

		if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(zparams), name_str, name_len + 1, (void *) &zparam)) {
			RETVAL_ZVAL(*zparam, 1, 0);
		}

		zval_ptr_dtor(&zparams);
	}
}


PHP_METHOD(HttpParams, offsetUnset)
{
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name_str, &name_len)) {
		zval *zparams = php_http_zsep(1, IS_ARRAY, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0 TSRMLS_CC));

		zend_symtable_del(Z_ARRVAL_P(zparams), name_str, name_len + 1);
		zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), zparams TSRMLS_CC);

		zval_ptr_dtor(&zparams);
	}
}

PHP_METHOD(HttpParams, offsetSet)
{
	zval *nvalue;
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name_str, &name_len, &nvalue)) {
		zval **zparam, *zparams = php_http_zsep(1, IS_ARRAY, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0 TSRMLS_CC));

		if (name_len) {
			if (Z_TYPE_P(nvalue) == IS_ARRAY) {
				zval *new_zparam;

				if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(zparams), name_str, name_len + 1, (void *) &zparam)) {
					new_zparam = php_http_zsep(1, IS_ARRAY, *zparam);
					array_join(Z_ARRVAL_P(nvalue), Z_ARRVAL_P(new_zparam), 0, 0);
				} else {
					new_zparam = nvalue;
					Z_ADDREF_P(new_zparam);
				}
				add_assoc_zval_ex(zparams, name_str, name_len + 1, new_zparam);
			} else {
				zval *tmp;

				if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(zparams), name_str, name_len + 1, (void *) &zparam)) {
					tmp = php_http_zsep(1, IS_ARRAY, *zparam);
				} else {
					MAKE_STD_ZVAL(tmp);
					array_init(tmp);
				}

				Z_ADDREF_P(nvalue);
				add_assoc_zval_ex(tmp, ZEND_STRS("value"), nvalue);
				add_assoc_zval_ex(zparams, name_str, name_len + 1, tmp);
			}
		} else {
			zval *tmp = php_http_ztyp(IS_STRING, nvalue), *arr;

			MAKE_STD_ZVAL(arr);
			array_init(arr);
			add_assoc_bool_ex(arr, ZEND_STRS("value"), 1);
			add_assoc_zval_ex(zparams, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp) + 1, arr);
			zval_ptr_dtor(&tmp);
		}

		zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), zparams TSRMLS_CC);
		zval_ptr_dtor(&zparams);
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

