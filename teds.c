/*
  +----------------------------------------------------------------------+
  | teds extension for PHP                                               |
  | See COPYING file for further copyright information                   |
  +----------------------------------------------------------------------+
  | Author: Tyson Andre <tandre@php.net>                                 |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"

#if PHP_VERSION_ID < 80000
#error PHP versions prior to php 8.0 are not supported
#endif

#if PHP_VERSION_ID < 80100
#define RETURN_COPY_DEREF(zv)			do { ZVAL_COPY_DEREF(return_value, zv); return; } while (0)
#endif

#include <ctype.h>
#include "zend_interfaces.h"
#include "zend_types.h"
#include "zend_smart_str.h"
#include "zend_strtod_int.h" /* WORDS_BIGENDIAN */
#include "ext/spl/spl_iterators.h"
#include "ext/standard/php_string.h"
#include "ext/standard/info.h"

#include "teds_deque.h"
#include "teds_immutablekeyvaluesequence.h"
#include "teds_immutablesequence.h"
#include "teds_keyvaluevector.h"
#include "teds_sortedstrictmap.h"
#include "teds_sortedstrictset.h"
#include "teds_stableheap.h"
#include "teds_strictmap.h"
#include "teds_strictset.h"
#include "teds_vector.h"
#include "teds_bswap.h"
#include "teds.h"

#include "teds_arginfo.h"
#include "php_teds.h"

#if PHP_VERSION_ID >= 80100
#include "zend_enum.h"
#endif

#ifndef ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE
#define ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, type_hint, allow_null, default_value) \
	ZEND_ARG_TYPE_INFO(pass_by_ref, name, type_hint, allow_null)
#endif

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

/* Copied from array_reduce implementation in https://github.com/php/php-src/blob/master/ext/standard/array.c */
static zend_always_inline void php_array_reduce(HashTable *htbl, zend_fcall_info fci, zend_fcall_info_cache fci_cache, zval* return_value) /* {{{ */
{
	zval args[2];
	zval *operand;
	zval retval;

	if (zend_hash_num_elements(htbl) == 0) {
		return;
	}

	fci.retval = &retval;
	fci.param_count = 2;

	ZEND_HASH_FOREACH_VAL(htbl, operand) {
		ZVAL_COPY_VALUE(&args[0], return_value);
		ZVAL_COPY(&args[1], operand);
		fci.params = args;

		if (zend_call_function(&fci, &fci_cache) == SUCCESS && Z_TYPE(retval) != IS_UNDEF) {
			zval_ptr_dtor(&args[1]);
			zval_ptr_dtor(&args[0]);
			ZVAL_COPY_VALUE(return_value, &retval);
			if (UNEXPECTED(Z_ISREF_P(return_value))) {
				zend_unwrap_reference(return_value);
			}
		} else {
			zval_ptr_dtor(&args[1]);
			zval_ptr_dtor(&args[0]);
			RETURN_NULL();
		}
	} ZEND_HASH_FOREACH_END();
}
/* }}} */

typedef struct {
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
	zval args[2];
} traversable_reduce_data;

static int teds_traversable_reduce_elem(zend_object_iterator *iter, void *puser) /* {{{ */
{
	traversable_reduce_data *reduce_data = puser;
	zend_fcall_info *fci = &reduce_data->fci;
	ZEND_ASSERT(ZEND_FCI_INITIALIZED(*fci));

	zval *operand = iter->funcs->get_current_data(iter);
	if (UNEXPECTED(!operand || EG(exception))) {
		return ZEND_HASH_APPLY_STOP;
	}
	ZVAL_DEREF(operand);
	ZVAL_COPY_VALUE(&reduce_data->args[0], fci->retval);
	ZVAL_COPY(&reduce_data->args[1], operand);
	ZVAL_NULL(fci->retval);
	int result = zend_call_function(&reduce_data->fci, &reduce_data->fcc);
	zval_ptr_dtor(operand);
	zval_ptr_dtor(&reduce_data->args[0]);
	if (UNEXPECTED(result == FAILURE || EG(exception))) {
		return ZEND_HASH_APPLY_STOP;
	}
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

static zend_always_inline void teds_traversable_reduce(zval *obj, zend_fcall_info fci, zend_fcall_info_cache fci_cache, zval* return_value) /* {{{ */
{
	traversable_reduce_data reduce_data;
	reduce_data.fci = fci;
	reduce_data.fci.retval = return_value;
	reduce_data.fci.param_count = 2;
	reduce_data.fci.params = reduce_data.args;
	reduce_data.fcc = fci_cache;

	spl_iterator_apply(obj, teds_traversable_reduce_elem, (void*)&reduce_data);
}
/* }}} */


static inline void php_array_until(zval *return_value, HashTable *htbl, zend_fcall_info fci, zend_fcall_info_cache fci_cache, int stop_value, int negate) /* {{{ */
{
	zval args[1];
	bool have_callback = 0;
	zval *operand;
	zval retval;
	int result;

	if (zend_hash_num_elements(htbl) == 0) {
		RETURN_BOOL(!stop_value ^ negate);
	}

	if (ZEND_FCI_INITIALIZED(fci)) {
		have_callback = 1;
		fci.retval = &retval;
		fci.param_count = 1;
		fci.params = args;
	}

	ZEND_HASH_FOREACH_VAL(htbl, operand) {
		if (have_callback) {
			bool retval_true;
			ZVAL_COPY(&args[0], operand);

			/* Treat the operand like an array of size 1  */
			result = zend_call_function(&fci, &fci_cache);
			zval_ptr_dtor(operand);
			if (result == FAILURE) {
				return;
			}
			retval_true = zend_is_true(&retval);
			zval_ptr_dtor(&retval);
			/* The user-provided callback rarely returns refcounted values. */
			if (retval_true == stop_value) {
				RETURN_BOOL(stop_value ^ negate);
			}
		} else {
			if (zend_is_true(operand) == stop_value) {
				RETURN_BOOL(stop_value ^ negate);
			}
		}
	} ZEND_HASH_FOREACH_END();

	RETURN_BOOL(!stop_value ^ negate);
}
/* }}} */

typedef struct {
	int                    stop_value;
	int                    result;
	zend_long              has_callback;
	zend_fcall_info        fci;
	zend_fcall_info_cache  fcc;
	bool                   found;
} php_iterator_until_info;

static int teds_traversable_func_until(zend_object_iterator *iter, void *puser) /* {{{ */
{
	zend_fcall_info fci;
	zval retval;
	php_iterator_until_info *until_info = (php_iterator_until_info*) puser;
	int result;
	bool stop;

	fci = until_info->fci;
	if (ZEND_FCI_INITIALIZED(fci)) {
		zval *operand = iter->funcs->get_current_data(iter);
		if (UNEXPECTED(!operand || EG(exception))) {
			until_info->result = FAILURE;
			return ZEND_HASH_APPLY_STOP;
		}
		ZVAL_DEREF(operand);
		fci.retval = &retval;
		fci.param_count = 1;
		/* Use the operand like an array of size 1 */
		fci.params = operand;
		fci.retval = &retval;
		Z_TRY_ADDREF_P(operand);
		result = zend_call_function(&fci, &until_info->fcc);
		zval_ptr_dtor(operand);
		if (result == FAILURE) {
			until_info->result = FAILURE;
			return ZEND_HASH_APPLY_STOP;
		}

		stop = zend_is_true(&retval) == until_info->stop_value;
		zval_ptr_dtor(&retval);
	} else {
		zval *operand = iter->funcs->get_current_data(iter);
		if (UNEXPECTED(!operand || EG(exception))) {
			return ZEND_HASH_APPLY_STOP;
		}
		ZVAL_DEREF(operand);
		stop = zend_is_true(operand) == until_info->stop_value;
		zval_ptr_dtor(operand);
	}
	if (stop) {
		until_info->found = true;
		return ZEND_HASH_APPLY_STOP;
	}
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

typedef struct {
	zval target;
	int result;
	bool found;
} php_traversable_search_info;

static int teds_traversable_func_search_value(zend_object_iterator *iter, void *puser) /* {{{ */
{
	php_traversable_search_info *search_info = (php_traversable_search_info*) puser;

	zval *operand = iter->funcs->get_current_data(iter);
	if (UNEXPECTED(!operand || EG(exception))) {
		search_info->result = FAILURE;
		return ZEND_HASH_APPLY_STOP;
	}
	ZVAL_DEREF(operand);
	bool stop = fast_is_identical_function(operand, &search_info->target);
	if (stop) {
		search_info->found = true;
		return ZEND_HASH_APPLY_STOP;
	}
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

static inline void teds_iterable_until(INTERNAL_FUNCTION_PARAMETERS, int stop_value, int negate) /* {{{ */
{
	zval *input;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fci_cache = empty_fcall_info_cache;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ITERABLE(input)
		Z_PARAM_OPTIONAL
		Z_PARAM_FUNC_OR_NULL(fci, fci_cache)
	ZEND_PARSE_PARAMETERS_END();

	switch (Z_TYPE_P(input)) {
		case IS_ARRAY:
			php_array_until(return_value, Z_ARRVAL_P(input), fci, fci_cache, stop_value, negate);
			return;
		case IS_OBJECT: {
			ZEND_ASSERT(instanceof_function(Z_OBJCE_P(input), zend_ce_traversable));
			php_iterator_until_info until_info;

			until_info.fci = fci;
			until_info.fcc = fci_cache;

			until_info.stop_value = stop_value;
			until_info.result = SUCCESS;
			until_info.found = false;

			if (spl_iterator_apply(input, teds_traversable_func_until, (void*)&until_info) == SUCCESS && until_info.result == SUCCESS) {
				RETURN_BOOL(!(until_info.found ^ stop_value ^ negate));
			}
			return;
		}
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}

/* Determines whether the predicate holds for all elements in the array */
PHP_FUNCTION(all)
{
	teds_iterable_until(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 0);
}
/* }}} */

/* {{{ Determines whether the predicate holds for at least one element in the array */
PHP_FUNCTION(any)
{
	teds_iterable_until(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 0);
}
/* }}} */

/* {{{ Determines whether the predicate holds for no elements of the array */
PHP_FUNCTION(none)
{
	teds_iterable_until(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 1);
}
/* }}} */

/* Determines whether the predicate holds for all elements in the array */
PHP_FUNCTION(includes_value)
{
	zval *input;
	zval *value;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_ITERABLE(input)
		Z_PARAM_ZVAL(value)
	ZEND_PARSE_PARAMETERS_END();

	switch (Z_TYPE_P(input)) {
		case IS_ARRAY: {
			zval *entry;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(input), entry) {
				ZVAL_DEREF(entry);
				if (fast_is_identical_function(value, entry)) {
					RETURN_TRUE;
				}
			} ZEND_HASH_FOREACH_END();
			RETURN_FALSE;
			return;
	    }
		case IS_OBJECT: {
			ZEND_ASSERT(instanceof_function(Z_OBJCE_P(input), zend_ce_traversable));
			php_traversable_search_info search_info;

			search_info.result = SUCCESS;
			search_info.found = false;
			ZVAL_COPY_VALUE(&search_info.target, value);

			if (spl_iterator_apply(input, teds_traversable_func_search_value, (void*)&search_info) == SUCCESS && search_info.result == SUCCESS) {
				RETURN_BOOL(search_info.found);
			}
			return;
		}
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}
/* }}} */

/* {{{ folds values */
PHP_FUNCTION(fold)
{
	zval *input;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
	zval *initial = NULL;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_ITERABLE(input)
		Z_PARAM_FUNC(fci, fci_cache)
		Z_PARAM_ZVAL(initial)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_COPY(return_value, initial);

	switch (Z_TYPE_P(input)) {
		case IS_ARRAY:
			php_array_reduce(Z_ARRVAL_P(input), fci, fci_cache, return_value);
			return;
		case IS_OBJECT: {
			ZEND_ASSERT(instanceof_function(Z_OBJCE_P(input), zend_ce_traversable));
			teds_traversable_reduce(input, fci, fci_cache, return_value);
			return;
		}
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}
/* }}} */

// Source: php_array_find from php-src/ext/standard/array.c
static zend_always_inline void teds_array_find(HashTable *htbl, zend_fcall_info fci, zend_fcall_info_cache fci_cache, zval* return_value, zval *default_value) /* {{{ */
{
	zval retval;
	zval *operand;

	if (zend_hash_num_elements(htbl) > 0) {
		fci.retval = &retval;
		fci.param_count = 1;

		ZEND_HASH_FOREACH_VAL(htbl, operand) {
			fci.params = operand;
			Z_TRY_ADDREF_P(operand);

			if (zend_call_function(&fci, &fci_cache) == SUCCESS) {
				bool found = zend_is_true(&retval);
				if (found) {
					ZVAL_COPY_VALUE(return_value, operand);
					return;
				} else {
					zval_ptr_dtor(operand);
				}
			} else {
				zval_ptr_dtor(operand);
				return;
			}
		} ZEND_HASH_FOREACH_END();
	}

	if (default_value) {
		ZVAL_COPY(return_value, default_value);
	} else {
		ZVAL_NULL(return_value);
	}
}
/* }}} */

typedef struct {
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
	zval *return_value;
	bool found;
} traversable_find_data;

static int teds_traversable_find_elem(zend_object_iterator *iter, void *puser) /* {{{ */
{
	zval retval;
	traversable_find_data *find_data = puser;
	zend_fcall_info *fci = &find_data->fci;
	ZEND_ASSERT(ZEND_FCI_INITIALIZED(*fci));

	zval *operand = iter->funcs->get_current_data(iter);
	if (UNEXPECTED(!operand || EG(exception))) {
		return ZEND_HASH_APPLY_STOP;
	}
	ZVAL_DEREF(operand);
    // Treat this as a 1-element array.
	fci->params = operand;
	fci->retval = &retval;
	Z_TRY_ADDREF_P(operand);
	int result = zend_call_function(&find_data->fci, &find_data->fcc);
	if (UNEXPECTED(result == FAILURE || EG(exception))) {
		return ZEND_HASH_APPLY_STOP;
	}
	fci->retval = &retval;
	bool found = zend_is_true(&retval);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(EG(exception))) {
		return ZEND_HASH_APPLY_STOP;
	}
	if (found) {
		ZVAL_COPY_VALUE(find_data->return_value, operand);
		find_data->found = 1;
		return ZEND_HASH_APPLY_STOP;
	}
	zval_ptr_dtor(operand);
	return ZEND_HASH_APPLY_KEEP;
}

static zend_always_inline void teds_traversable_find(zval *obj, zend_fcall_info fci, zend_fcall_info_cache fci_cache, zval* return_value, zval *default_value) /* {{{ */
{
	traversable_find_data find_data;
	find_data.fci = fci;
	find_data.fci.param_count = 1;
	find_data.fcc = fci_cache;
	find_data.return_value = return_value;
	find_data.found = 0;

	if (spl_iterator_apply(obj, teds_traversable_find_elem, (void*)&find_data) != SUCCESS || EG(exception)) {
		return;
	}
	if (find_data.found) {
		return;
	}
	if (default_value) {
		ZVAL_COPY(return_value, default_value);
	} else {
		ZVAL_NULL(return_value);
	}
}
/* }}} */

/* {{{ Finds the first value or returns the default */
PHP_FUNCTION(find)
{
	zval *input;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
	zval *default_value = NULL;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_ITERABLE(input)
		Z_PARAM_FUNC(fci, fci_cache)
		Z_PARAM_OPTIONAL
		Z_PARAM_ZVAL(default_value)
	ZEND_PARSE_PARAMETERS_END();

	switch (Z_TYPE_P(input)) {
		case IS_ARRAY:
			teds_array_find(Z_ARRVAL_P(input), fci, fci_cache, return_value, default_value);
			return;
		case IS_OBJECT: {
			teds_traversable_find(input, fci, fci_cache, return_value, default_value);
			return;
		}
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}
/* }}} */

/* {{{ Get the value of the first element of the array */
PHP_FUNCTION(array_value_first)
{
	HashTable *target_hash;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(target_hash)
	ZEND_PARSE_PARAMETERS_END();

	if (zend_hash_num_elements(target_hash) == 0) {
		RETURN_NULL();
	}

	HashPosition pos = 0;
	RETURN_COPY_DEREF(zend_hash_get_current_data_ex(target_hash, &pos));
}
/* }}} */

/* {{{ Get the value of the last element of the array */
PHP_FUNCTION(array_value_last)
{
	HashTable *target_hash;
	HashPosition pos;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(target_hash)
	ZEND_PARSE_PARAMETERS_END();

	if (zend_hash_num_elements(target_hash) == 0) {
		RETURN_NULL();
	}
	zend_hash_internal_pointer_end_ex(target_hash, &pos);
	RETURN_COPY_DEREF(zend_hash_get_current_data_ex(target_hash, &pos));
}
/* }}} */

#define SPACESHIP_OP(n1, n2) ((n1) < (n2) ? -1 : (n1) > (n2) ? 1 : 0)

static zend_always_inline int teds_stable_compare_wrap(const void *v1, const void *v2) {
	return teds_stable_compare(v1, v2);
}

zend_long teds_stable_compare(const zval *v1, const zval *v2) {
	ZVAL_DEREF(v1);
	ZVAL_DEREF(v2);
	const zend_uchar t1 = Z_TYPE_P(v1);
	const zend_uchar t2 = Z_TYPE_P(v2);
	if (t1 != t2) {
		if (((1 << t1) | (1 << t2)) & ~((1 << IS_LONG) | (1 << IS_DOUBLE))) {
			/* At least one type is not a long or a double. */
			return t1 < t2 ? -1 : 1;
		}
		if (t1 == IS_DOUBLE) {
			ZEND_ASSERT(t2 == IS_LONG);

			zend_long i2 = Z_LVAL_P(v2);
			/* Windows long double is an alias of double. */
			double n1 = Z_DVAL_P(v1);
			double n2 = i2;
			if (n1 != n2) {
				return n1 < n2 ? -1 : 1;
			}
			if (EXPECTED(i2 == (zend_long)n1)) {
				/* long before double */
				return 1;
			}
			if (i2 > 0) {
				zend_ulong i1 = (zend_ulong)n1;
				return i1 < (zend_ulong) i2 ? -1 : 1;
			} else {
				zend_ulong i1 = (zend_ulong)-n1;
				return i1 < (zend_ulong) -i2 ? 1 : -1;
			}
		} else {
			ZEND_ASSERT(t1 == IS_LONG);
			ZEND_ASSERT(t2 == IS_DOUBLE);

			zend_long i1 = Z_LVAL_P(v1);
			double n1 = i1;
			double n2 = Z_DVAL_P(v2);
			if (n1 != n2) {
				return n2 < n1 ? 1 : -1;
			}
			if (EXPECTED(i1 == (zend_long)n2)) {
				return -1;
			}
			if (i1 > 0) {
				zend_ulong i2 = (zend_ulong)n2;
				return ((zend_ulong)i1) < i2 ? -1 : 1;
			} else {
				zend_ulong i2 = (zend_ulong)-n2;
				return ((zend_ulong)-i1) < i2 ? 1 : -1;
			}
		}
	}
	switch (t1) {
		case IS_NULL:
		case IS_FALSE:
		case IS_TRUE:
			return 0;
		case IS_LONG:
			return SPACESHIP_OP(Z_LVAL_P(v1), Z_LVAL_P(v2));
		case IS_DOUBLE: {
			double n1 = Z_DVAL_P(v1);
			double n2 = Z_DVAL_P(v2);
			if (n1 == n2) {
				return 0;
			}
			if (UNEXPECTED(n2 != n2)) {
				/* Treat INF as smaller than NAN */
				return n1 != n1 ? 0 : -1;
			}
			return n1 < n2 ? -1 : 1;
		}
		case IS_STRING: {
			int result = zend_binary_zval_strcmp((zval *)v1, (zval *)v2);
			/* Avoid platform dependence, zend_binary_zval_strcmp is different on Windows */
			return SPACESHIP_OP(result, 0);
		}
		case IS_ARRAY:  {
			HashTable *h1 = Z_ARR_P(v1);
			HashTable *h2 = Z_ARR_P(v2);
			return zend_hash_compare(h1, h2, teds_stable_compare_wrap, true);
		}
		case IS_OBJECT: {
			/* Sort by class name, then by object handle, to group objects of the same class together. */
			zend_object *o1 = Z_OBJ_P(v1);
			zend_object *o2 = Z_OBJ_P(v2);
			if (o1 == o2) {
				return 0;
			}
			if (o1->ce != o2->ce) {
				zend_string *c1 = o1->ce->name;
				zend_string *c2 = o2->ce->name;
				int result = zend_binary_strcmp(ZSTR_VAL(c1), ZSTR_LEN(c1), ZSTR_VAL(c2), ZSTR_LEN(c2));
				if (result != 0) {
					return result < 0 ? -1 : 1;
				}
			}
			ZEND_ASSERT(Z_OBJ_HANDLE_P(v1) != Z_OBJ_HANDLE_P(v2)); /* Implied by o1 != o2 */
			return Z_OBJ_HANDLE_P(v1) < Z_OBJ_HANDLE_P(v2) ? -1 : 1;
		}
		case IS_RESOURCE:
			return SPACESHIP_OP(Z_RES_HANDLE_P(v1), Z_RES_HANDLE_P(v2));
		default:
			ZEND_UNREACHABLE();
	}
}

/* {{{ Compare two elements in a stable order. */
PHP_FUNCTION(stable_compare)
{
	zval *v1;
	zval *v2;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_ZVAL(v1)
		Z_PARAM_ZVAL(v2)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_LONG(teds_stable_compare(v1, v2));
}
/* }}} */

/* This assumes that pointers differ in low addresses rather than high addresses.
 * Copied from code written for igbinary. */
inline static uint64_t teds_inline_hash_of_uint64(uint64_t orig) {
	/* Works best when data that frequently differs is in the least significant bits of data */
	uint64_t data = orig * 0x5e2d58d8b3bce8d9;
	/* bswap is a single assembly instruction on recent compilers/platforms */
	return bswap_64(data);
}

inline static uint64_t teds_convert_double_to_uint64_t(double *value) {
	uint8_t *data = (uint8_t *)value;
#ifndef WORDS_BIGENDIAN
	return
		(((uint64_t)data[0]) << 56) |
		(((uint64_t)data[1]) << 48) |
		(((uint64_t)data[2]) << 40) |
		(((uint64_t)data[3]) << 32) |
		(((uint64_t)data[4]) << 24) |
		(((uint64_t)data[5]) << 16) |
		(((uint64_t)data[6]) << 8) |
		(((uint64_t)data[7]));
#else
	return
		(((uint64_t)data[7]) << 56) |
		(((uint64_t)data[6]) << 48) |
		(((uint64_t)data[5]) << 40) |
		(((uint64_t)data[4]) << 32) |
		(((uint64_t)data[3]) << 24) |
		(((uint64_t)data[2]) << 16) |
		(((uint64_t)data[1]) << 8) |
		(((uint64_t)data[0]));
#endif
}

static zend_long teds_strict_hash_inner(zval *value) {
again:
	switch (Z_TYPE_P(value)) {
		case IS_NULL:
			return 8310;
		case IS_FALSE:
			return 8311;
		case IS_TRUE:
			return 8312;
		case IS_LONG:
			return Z_LVAL_P(value);
		case IS_DOUBLE:
			return teds_convert_double_to_uint64_t(&Z_DVAL_P(value)) + 8315;
		case IS_STRING:
			/* Compute the hash if needed, return it */
			return ZSTR_HASH(Z_STR_P(value));
		case IS_ARRAY: {
			zend_long num_key;
			zend_string *str_key;
			zval *field_value;

			uint64_t result = 1;
			HashTable *ht = Z_ARR_P(value);

			if (zend_hash_num_elements(ht) == 0) {
				return 8313;
			}

			if (!(GC_FLAGS(ht) & GC_IMMUTABLE)) {
				if (GC_IS_RECURSIVE(ht)) {
					/* This is recursively trying to hash itself. */
					return 8314;
				}
				/* GC_ADDREF(ht); */
				GC_PROTECT_RECURSION(ht);
			}

			/* teds_strict_hash_inner has code to dereference IS_INDIRECT/IS_REFERENCE,
			 * but IS_INDIRECT is probably impossible as of php 8.1's removal of direct access to $GLOBALS? */
			ZEND_HASH_FOREACH_KEY_VAL(Z_ARR_P(value), num_key, str_key, field_value) {
				/* str_key is in a hash table meaning the hash was already computed. */
				result += str_key ? ZSTR_H(str_key) : (zend_ulong) num_key;
				result += (teds_strict_hash_inner(field_value) + (result << 7));
				result = teds_inline_hash_of_uint64(result);
			} ZEND_HASH_FOREACH_END();

			if (!(GC_FLAGS(ht) & GC_IMMUTABLE)) {
				GC_UNPROTECT_RECURSION(ht);
				/* GC_DELREF(ht); */
			}
			return result;
	    }
		case IS_OBJECT:
		    /* Avoid hash collisions between objects and small numbers. */
			return Z_OBJ_HANDLE_P(value) + 31415926;
		case IS_RESOURCE:
			return Z_RES_HANDLE_P(value) + 27182818;
		case IS_REFERENCE:
			value = Z_REFVAL_P(value);
			goto again;
		case IS_INDIRECT:
			value = Z_INDIRECT_P(value);
			goto again;
		default:
			ZEND_UNREACHABLE();
	}
}

/* {{{ Generate a hash */
zend_long teds_strict_hash(zval *value) {
	uint64_t raw_data = teds_strict_hash_inner(value);
	return teds_inline_hash_of_uint64(raw_data);
}
/* }}} */

/* {{{ Compare two elements in a stable order. */
PHP_FUNCTION(strict_hash)
{
	zval *value;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(value)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_LONG(teds_strict_hash(value));
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(teds)
{
	PHP_MINIT(teds_deque)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_immutablekeyvaluesequence)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_immutablesequence)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_keyvaluevector)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_sortedstrictmap)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_sortedstrictset)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_stableheap)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_strictmap)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_strictset)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(teds_vector)(INIT_FUNC_ARGS_PASSTHRU);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(teds)
{
#if defined(ZTS) && defined(COMPILE_DL_TEDS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(teds)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "teds support", "enabled");
	php_info_print_table_row(2, "teds version", PHP_TEDS_VERSION);
	php_info_print_table_end();
}
/* }}} */

/* {{{ teds_module_entry */
zend_module_entry teds_module_entry = {
	STANDARD_MODULE_HEADER,
	"teds",				/* Extension name */
	ext_functions,		/* zend_function_entry */
	PHP_MINIT(teds),	/* PHP_MINIT - Module initialization */
	NULL,				/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(teds),		/* PHP_RINIT - Request initialization */
	NULL,				/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(teds),		/* PHP_MINFO - Module info */
	PHP_TEDS_VERSION,	/* Version */
	STANDARD_MODULE_PROPERTIES
};
/* }}} */


#ifdef COMPILE_DL_TEDS
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(teds)
#endif
