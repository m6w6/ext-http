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
	.param = def_param_sep_ptr,
	.arg = def_arg_sep_ptr,
	.val = def_val_sep_ptr
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

static inline void sanitize_string(char *str, size_t len, zval *zv TSRMLS_DC)
{
	/* trim whitespace */
	php_trim(str, len, NULL, 0, zv, 3 TSRMLS_CC);

	/* dequote */
	if (Z_STRVAL_P(zv)[0] == '"' && Z_STRVAL_P(zv)[Z_STRLEN_P(zv) - 1] == '"') {
		size_t deq_len = Z_STRLEN_P(zv) - 2;
		char *deq = estrndup(Z_STRVAL_P(zv) + 1, deq_len);

		zval_dtor(zv);
		ZVAL_STRINGL(zv, deq, deq_len, 0);
	}

	/* strip slashes */
	php_stripslashes(Z_STRVAL_P(zv), &Z_STRLEN_P(zv) TSRMLS_CC);
}

static void push_param(HashTable *params, php_http_params_state_t *state, const php_http_params_opts_t *opts TSRMLS_DC)
{
	if (state->val.str) {
		if (0 < (state->val.len = state->input.str - state->val.str)) {
			sanitize_string(state->val.str, state->val.len, *(state->current.val) TSRMLS_CC);
		}
	} else if (state->arg.str) {
		if (0 < (state->arg.len = state->input.str - state->arg.str)) {
			zval *val, key;

			INIT_PZVAL(&key);
			sanitize_string(state->arg.str, state->arg.len, &key TSRMLS_CC);
			if (Z_STRLEN(key)) {
				MAKE_STD_ZVAL(val);
				ZVAL_TRUE(val);
				zend_symtable_update(Z_ARRVAL_PP(state->current.args), Z_STRVAL(key), Z_STRLEN(key) + 1, (void *) &val, sizeof(zval *), (void *) &state->current.val);
			}
			zval_dtor(&key);
		}
	} else if (state->param.str) {
		if (0 < (state->param.len = state->input.str - state->param.str)) {
			zval *prm, *arg, *val, key;

			INIT_PZVAL(&key);
			sanitize_string(state->param.str, state->param.len, &key TSRMLS_CC);
			if (Z_STRLEN(key)) {
				MAKE_STD_ZVAL(prm);
				array_init(prm);
				MAKE_STD_ZVAL(val);
				ZVAL_TRUE(val);
				zend_hash_update(Z_ARRVAL_P(prm), "value", sizeof("value"), (void *) &val, sizeof(zval *), (void *) &state->current.val);

				MAKE_STD_ZVAL(arg);
				array_init(arg);
				zend_hash_update(Z_ARRVAL_P(prm), "arguments", sizeof("arguments"), (void *) &arg, sizeof(zval *), (void *) &state->current.args);

				zend_symtable_update(params, Z_STRVAL(key), Z_STRLEN(key) + 1, (void *) &prm, sizeof(zval *), (void *) &state->current.param);
			}
			zval_dtor(&key);
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
		if (!state.param.str) {
			/* initialize */
			state.param.str = state.input.str;
		} else {
			size_t sep_len;
			/* are we at a param separator? */
			if (0 < (sep_len = check_sep(&state, opts->param))) {
				push_param(params, &state, opts TSRMLS_CC);

				/* start off with a new param */
				state.param.str = state.input.str + sep_len;
				state.param.len = 0;
				state.arg.str = NULL;
				state.arg.len = 0;
				state.val.str = NULL;
				state.val.len = 0;
			} else
			/* are we at an arg separator? */
			if (0 < (sep_len = check_sep(&state, opts->arg))) {
				push_param(params, &state, opts TSRMLS_CC);

				/* continue with a new arg */
				state.arg.str = state.input.str + sep_len;
				state.arg.len = 0;
				state.val.str = NULL;
				state.val.len = 0;
			} else
			/* are we at a val separator? */
			if (0 < (sep_len = check_sep(&state, opts->val))) {
				/* only handle separator if we're not already reading in a val */
				if (!state.val.str) {
					push_param(params, &state, opts TSRMLS_CC);

					state.val.str = state.input.str + sep_len;
					state.val.len = 0;
				}
			}
		}

		++state.input.str;
		--state.input.len;
	}
	/* finalize */
	push_param(params, &state, opts TSRMLS_CC);

	return params;
}

PHP_HTTP_API php_http_buffer_t *php_http_params_to_string(php_http_buffer_t *buf, HashTable *params, const char *pss, size_t psl, const char *ass, size_t asl, const char *vss, size_t vsl TSRMLS_DC)
{
	zval **zparam;
	HashPosition pos1, pos2;
	php_http_array_hashkey_t key1 = php_http_array_hashkey_init(0), key2 = php_http_array_hashkey_init(0);

	if (!buf) {
		buf = php_http_buffer_init(NULL);
	}

	FOREACH_HASH_KEYVAL(pos1, params, key1, zparam) {
		/* new param ? */
		if (PHP_HTTP_BUFFER_LEN(buf)) {
			php_http_buffer_append(buf, pss, psl);
		}

		/* add name */
		if (key1.type == HASH_KEY_IS_STRING) {
			php_http_buffer_append(buf, key1.str, key1.len - 1);
		} else {
			php_http_buffer_appendf(buf, "%lu", key1.num);
		}

		if (Z_TYPE_PP(zparam) == IS_ARRAY) {
			zval **zvalue, **zargs, **zarg;

			/* got a value? */
			if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(zparam), ZEND_STRS("value"), (void *) &zvalue)) {
				if (Z_TYPE_PP(zvalue) != IS_BOOL) {
					zval *tmp = php_http_ztyp(IS_STRING, *zvalue);

					php_http_buffer_append(buf, vss, vsl);
					php_http_buffer_append(buf, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
					zval_ptr_dtor(&tmp);
				} else if (!Z_BVAL_PP(zvalue)) {
					php_http_buffer_append(buf, vss, vsl);
					php_http_buffer_appends(buf, "0");
				}
			}
			/* add arguments */
			if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(zparam), ZEND_STRS("arguments"), (void *) &zargs)) {
				if (Z_TYPE_PP(zargs) == IS_ARRAY) {
					FOREACH_KEYVAL(pos2, *zargs, key2, zarg) {
						/* new arg? */
						if (PHP_HTTP_BUFFER_LEN(buf)) {
							php_http_buffer_append(buf, ass, asl);
						}

						/* add name */
						if (key2.type == HASH_KEY_IS_STRING) {
							php_http_buffer_append(buf, key2.str, key2.len - 1);
						} else {
							php_http_buffer_appendf(buf, "%lu", key2.num);
						}
						/* add value */
						if (Z_TYPE_PP(zarg) != IS_BOOL) {
							zval *tmp = php_http_ztyp(IS_STRING, *zarg);
							int escaped_len;

							Z_STRVAL_P(tmp) = php_addslashes(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp), &escaped_len, 1 TSRMLS_CC);
							php_http_buffer_append(buf, vss, vsl);
							if (escaped_len != Z_STRLEN_P(tmp)) {
								php_http_buffer_appends(buf, "\"");
								php_http_buffer_append(buf, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp) = escaped_len);
								php_http_buffer_appends(buf, "\"");
							} else {
								php_http_buffer_append(buf, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
							}
							zval_ptr_dtor(&tmp);
						} else if (!Z_BVAL_PP(zarg)) {
							php_http_buffer_append(buf, vss, vsl);
							php_http_buffer_appends(buf, "0");
						}
					}
				}
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

zend_class_entry *php_http_params_class_entry;
zend_function_entry php_http_params_method_entry[] = {
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
	PHP_HTTP_REGISTER_CLASS(http, Params, http_params, php_http_object_class_entry, 0);

	zend_class_implements(php_http_params_class_entry TSRMLS_CC, 1, zend_ce_arrayaccess);

	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_PARAM_SEP"), ZEND_STRL(",") TSRMLS_CC);
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_ARG_SEP"), ZEND_STRL(";") TSRMLS_CC);
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("DEF_VAL_SEP"), ZEND_STRL("=") TSRMLS_CC);
	zend_declare_class_constant_stringl(php_http_params_class_entry, ZEND_STRL("COOKIE_PARAM_SEP"), ZEND_STRL("") TSRMLS_CC);

	zend_declare_property_null(php_http_params_class_entry, ZEND_STRL("params"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("param_sep"), ZEND_STRL(","), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("arg_sep"), ZEND_STRL(";"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_stringl(php_http_params_class_entry, ZEND_STRL("val_sep"), ZEND_STRL("="), ZEND_ACC_PUBLIC TSRMLS_CC);

	return SUCCESS;
}

static php_http_params_token_t **parse_sep(zval *zv TSRMLS_DC)
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

static void free_sep(php_http_params_token_t **separator) {
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
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		zval *zcopy, *zparams = NULL, *param_sep = NULL, *arg_sep = NULL, *val_sep = NULL;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!/z/z/z/", &zparams, &param_sep, &arg_sep, &val_sep)) {
			switch (ZEND_NUM_ARGS()) {
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
								.input = {
									.str = Z_STRVAL_P(zcopy),
									.len = Z_STRLEN_P(zcopy)
								},
								.param = parse_sep(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), 0 TSRMLS_CC) TSRMLS_CC),
								.arg = parse_sep(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), 0 TSRMLS_CC) TSRMLS_CC),
								.val = parse_sep(zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), 0 TSRMLS_CC) TSRMLS_CC)
							};

							MAKE_STD_ZVAL(zparams);
							array_init(zparams);
							php_http_params_parse(Z_ARRVAL_P(zparams), &opts TSRMLS_CC);
							zend_update_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), zparams TSRMLS_CC);
							zval_ptr_dtor(&zparams);

							free_sep(opts.param);
							free_sep(opts.arg);
							free_sep(opts.val);
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
	zval *zparams, *zpsep, *zasep, *zvsep;
	php_http_buffer_t buf;

	zparams = php_http_ztyp(IS_ARRAY, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("params"), 0 TSRMLS_CC));
	zpsep = php_http_ztyp(IS_STRING, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("param_sep"), 0 TSRMLS_CC));
	zasep = php_http_ztyp(IS_STRING, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("arg_sep"), 0 TSRMLS_CC));
	zvsep = php_http_ztyp(IS_STRING, zend_read_property(php_http_params_class_entry, getThis(), ZEND_STRL("val_sep"), 0 TSRMLS_CC));

	php_http_buffer_init(&buf);
	php_http_params_to_string(&buf, Z_ARRVAL_P(zparams), Z_STRVAL_P(zpsep), Z_STRLEN_P(zpsep), Z_STRVAL_P(zasep), Z_STRLEN_P(zasep), Z_STRVAL_P(zvsep), Z_STRLEN_P(zvsep) TSRMLS_CC);

	zval_ptr_dtor(&zparams);
	zval_ptr_dtor(&zpsep);
	zval_ptr_dtor(&zasep);
	zval_ptr_dtor(&zvsep);

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

