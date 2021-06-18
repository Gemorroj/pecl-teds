/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: e52eabcb96cc945d9641b0a31d572d375cc83e6a */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Teds_any, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, iterable, IS_ITERABLE, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, callback, IS_CALLABLE, 1, "null")
ZEND_END_ARG_INFO()

#define arginfo_Teds_all arginfo_Teds_any

#define arginfo_Teds_none arginfo_Teds_any

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Teds_fold, 0, 3, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, iterable, IS_ITERABLE, 0)
	ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
	ZEND_ARG_TYPE_INFO(0, initial, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Teds_find, 0, 2, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, iterable, IS_ITERABLE, 0)
	ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, default, IS_MIXED, 0, "null")
ZEND_END_ARG_INFO()


ZEND_FUNCTION(any);
ZEND_FUNCTION(all);
ZEND_FUNCTION(none);
ZEND_FUNCTION(fold);
ZEND_FUNCTION(find);


static const zend_function_entry ext_functions[] = {
	ZEND_NS_FE("Teds", any, arginfo_Teds_any)
	ZEND_NS_FE("Teds", all, arginfo_Teds_all)
	ZEND_NS_FE("Teds", none, arginfo_Teds_none)
	ZEND_NS_FE("Teds", fold, arginfo_Teds_fold)
	ZEND_NS_FE("Teds", find, arginfo_Teds_find)
	ZEND_FE_END
};
