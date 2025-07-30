#pragma once
#define __wo_macro_concat(A,B) A##B
#define _wo_macro_concat(A,B) __wo_macro_concat(A,B)
#define wo_macro_concat(A,B) _wo_macro_concat(A,B)

#define wo_macro_boolean_and_01 0
#define wo_macro_boolean_and_00 0
#define wo_macro_boolean_and_10 0
#define wo_macro_boolean_and_11 1

#define wo_macro_boolean_or_01 0
#define wo_macro_boolean_or_00 0
#define wo_macro_boolean_or_10 0
#define wo_macro_boolean_or_11 1

#define wo_macro_boolean_not_0 1
#define wo_macro_boolean_not_1 0

#define wo_macro_boolean_and(A,B) wo_macro_concat(wo_macro_boolean_and_, wo_macro_concat(A,B))
#define wo_macro_boolean_or(A,B) wo_macro_concat(wo_macro_boolean_or_, wo_macro_concat(A,B))
#define wo_macro_boolean_not(A) wo_macro_concat(wo_macro_boolean_not_, A)

#define __wo_macro_get_args_8(_0,_1,_2,_3,_4,_5,_6,_7,NAME,...) NAME
#define _wo_macro_get_args_8(args) __wo_macro_get_args_8 args
#define wo_macro_get_args_8(...) _wo_macro_get_args_8((__VA_ARGS__))

#define wo_macro_has_comma(...) wo_macro_get_args_8(__VA_ARGS__, 1,1,1,1,1,1,1,0,0)
#define wo_macro_eat_args_to_comma(...) ,
#define wo_macro_is_args_empty(...)  wo_macro_boolean_and(    wo_macro_boolean_and(    wo_macro_boolean_not(wo_macro_has_comma(__VA_ARGS__)), \
                                                                                    wo_macro_boolean_not(wo_macro_has_comma(__VA_ARGS__()))), \
                                                            wo_macro_boolean_and(    wo_macro_boolean_not(wo_macro_has_comma(wo_macro_eat_args_to_comma __VA_ARGS__)), \
                                                                                    wo_macro_has_comma(wo_macro_eat_args_to_comma __VA_ARGS__())))
#define wo_macro_sign_if_0(A)
#define wo_macro_sign_if_1(A) A
#define wo_macro_sign_if(SIGN,A) wo_macro_concat(wo_macro_comma_if_, A)(SIGN)

#define wo_macro_comma_if_0
#define wo_macro_comma_if_1 ,
#define wo_macro_comma_if(A) wo_macro_concat(wo_macro_comma_if_, A)

#define wo_macro_va_opt_comma(...) wo_macro_comma_if(wo_macro_boolean_not(wo_macro_is_args_empty(__VA_ARGS__)))

#define wo_macro_va_num(...) wo_macro_get_args_8(__VA_ARGS__ wo_macro_va_opt_comma(__VA_ARGS__) 8,7,6,5,4,3,2,1,0)

#define _wo_macro_invoke(FUNC_NAME,ARGPACK) FUNC_NAME ARGPACK
#define wo_macro_invoke(FUNC_NAME,...) _wo_macro_invoke(FUNC_NAME, (__VA_ARGS__))

#define wo_macro_overload(FUNC_NAME,...) wo_macro_invoke(wo_macro_concat(wo_macro_concat(FUNC_NAME,_),wo_macro_va_num(__VA_ARGS__)),__VA_ARGS__)

// Source attribute.
#ifdef NDEBUG
#   if defined(_MSC_VER)
#       define WO_FORCE_INLINE __forceinline
#   elif defined(__GNUC__) || defined(__clang__)
#       define WO_FORCE_INLINE inline __attribute__((always_inline))
#   else
#       define WO_FORCE_INLINE inline
#   endif
#else
#   define WO_FORCE_INLINE 
#endif
