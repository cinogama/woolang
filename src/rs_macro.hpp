#pragma once

#define _CRT_SECURE_NO_WARNINGS

#define __rs_macro_concat(A,B) A##B
#define _rs_macro_concat(A,B) __rs_macro_concat(A,B)
#define rs_macro_concat(A,B) _rs_macro_concat(A,B)

#define rs_macro_boolean_and_01 0
#define rs_macro_boolean_and_00 0
#define rs_macro_boolean_and_10 0
#define rs_macro_boolean_and_11 1

#define rs_macro_boolean_or_01 0
#define rs_macro_boolean_or_00 0
#define rs_macro_boolean_or_10 0
#define rs_macro_boolean_or_11 1

#define rs_macro_boolean_not_0 1
#define rs_macro_boolean_not_1 0

#define rs_macro_boolean_and(A,B) rs_macro_concat(rs_macro_boolean_and_, rs_macro_concat(A,B))
#define rs_macro_boolean_or(A,B) rs_macro_concat(rs_macro_boolean_or_, rs_macro_concat(A,B))
#define rs_macro_boolean_not(A) rs_macro_concat(rs_macro_boolean_not_, A)

#define __rs_macro_get_args_8(_0,_1,_2,_3,_4,_5,_6,_7,NAME,...) NAME
#define _rs_macro_get_args_8(args) __rs_macro_get_args_8 args
#define rs_macro_get_args_8(...) _rs_macro_get_args_8((__VA_ARGS__))

#define rs_macro_has_comma(...) rs_macro_get_args_8(__VA_ARGS__, 1,1,1,1,1,1,1,0,0)
#define rs_macro_eat_args_to_comma(...) ,
#define rs_macro_is_args_empty(...)  rs_macro_boolean_and(    rs_macro_boolean_and(    rs_macro_boolean_not(rs_macro_has_comma(__VA_ARGS__)), \
                                                                                    rs_macro_boolean_not(rs_macro_has_comma(__VA_ARGS__()))), \
                                                            rs_macro_boolean_and(    rs_macro_boolean_not(rs_macro_has_comma(rs_macro_eat_args_to_comma __VA_ARGS__)), \
                                                                                    rs_macro_has_comma(rs_macro_eat_args_to_comma __VA_ARGS__())))
#define rs_macro_sign_if_0(A)
#define rs_macro_sign_if_1(A) A
#define rs_macro_sign_if(SIGN,A) rs_macro_concat(rs_macro_comma_if_, A)(SIGN)

#define rs_macro_comma_if_0
#define rs_macro_comma_if_1 ,
#define rs_macro_comma_if(A) rs_macro_concat(rs_macro_comma_if_, A)

#define rs_macro_va_opt_comma(...) rs_macro_comma_if(rs_macro_boolean_not(rs_macro_is_args_empty(__VA_ARGS__)))

#define rs_macro_va_num(...) rs_macro_get_args_8(__VA_ARGS__ rs_macro_va_opt_comma(__VA_ARGS__) 8,7,6,5,4,3,2,1,0)

#define _rs_macro_invoke(FUNC_NAME,ARGPACK) FUNC_NAME ARGPACK
#define rs_macro_invoke(FUNC_NAME,...) _rs_macro_invoke(FUNC_NAME, (__VA_ARGS__))

#define rs_macro_overload(FUNC_NAME,...) rs_macro_invoke(rs_macro_concat(rs_macro_concat(FUNC_NAME,_),rs_macro_va_num(__VA_ARGS__)),__VA_ARGS__)
