#pragma once
// This file using to store compiler/lexer's error informations.
// Here will be 

#define RS_LANG_EN 0
#define RS_LANG_ZH_CN 1

#define RS_USED_LANGUAGE RS_LANG_ZH_CN

#if RS_USED_LANGUAGE == RS_LANG_ZH_CN

#define RS_TERM_GLOBAL_NAMESPACE L"全局作用域"

#define RS_TERM_OR L"或"

#define RS_TERM_AT L"位于"

#define RS_TERM_AND L"和"

#define RS_TERM_EXCEPTED L"应当是"


#define RS_ERR_MISMATCH_ANNO_SYM L"不匹配的注释符"

#define RS_ERR_UNEXCEPT_CH_AFTER_CH L"未预料的符号 '%c' 位于 '%c' 之后"

#define RS_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"未预料的符号 '%c' 位于 '%c' 之后, 此处应该是 '%c'"

#define RS_ERR_UNEXCEPT_EOF L"未预料的文件尾"

#define RS_ERR_UNEXCEPT_TOKEN L"未预料的符号: "

#define RS_ERR_ILLEGAL_LITERAL L"非法的字面常量"

#define RS_ERR_UNKNOW_OPERATOR_STR L"未知运算符: '%s'"

#define RS_ERR_UNEXCEPTED_EOL_IN_STRING L"在字符串常量中发现换行符"

#define RS_ERR_LEXER_ERR_UNKNOW_NUM_BASE L"词法错误，未知的常量基数"

#define RS_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L"词法错误, 未知的符号: '%c'"

#define RS_ERR_UNEXCEPT_AST_NODE_TYPE L"未预料到的语法树节点类型: 应当是语法树节点或标识符"

#define RS_ERR_SHOULD_BE_AST_BASE L"语法分析时发生未知错误: 未知节点类型"

#define RS_ERR_UNABLE_RECOVER_FROM_ERR L"无法从当前错误中恢复，编译终止"

#define RS_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME L"不可以同时声明为 'public', 'protected' 或者 'private'"

#define RS_ERR_TYPE_CANNOT_NEGATIVE L"'%s' 不能被取负值"

#define RS_ERR_CANNOT_OPEN_FILE L"无法打开文件: '%s'"

#define RS_ERR_UNPACK_ARG_LESS_THEN_ONE L"展开参数包时至少需要展开一个参数"

#define RS_ERR_CANNOT_FIND_EXT_SYM L"无法找到外部符号: '%s'"

#define RS_ERR_ARG_DEFINE_AFTER_VARIADIC L"在 '...' 之后不应该有其他参数"

#define RS_ERR_CANNOT_CALC_STR_WITH_THIS_OP L"不支持对字符串进行该运算"

#define RS_ERR_CANNOT_CALC_HANDLE_WITH_THIS_OP L"不支持对句柄类型进行该运算"

#define RS_ERR_CANNOT_ASSIGN_TO_CONSTANT L"不允许向常量赋值"

#define RS_ERR_CANNOT_CALC_WITH_L_AND_R L"运算符左右两边的值类型不匹配，无法计算"

#define RS_ERR_CANNOT_INDEX_STR_WITH_TYPE L"不能使用 '%s' 类型的值索引字符串"

#define RS_ERR_CANNOT_CAST_TYPE_TO_TYPE  L"无法将 '%s' 类型的值转换为 '%s'"

#define RS_ERR_CANNOT_IMPLCAST_TYPE_TO_TYPE L"无法将 '%s' 类型的值隐式转换为 '%s'"

#define RS_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE L"不能将 '%s' 类型的值赋值给 '%s' 类型的变量"

#define RS_ERR_CANNOT_DO_RET_OUSIDE_FUNC L"非法的返回操作, 不允许在函数范围外进行返回"

#define RS_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME L"不能同时返回 '%s' 和 '%s'"

#define RS_ERR_ERR_PLACE_FOR_USING_NAMESPACE L"使用命名空间声明语句应位于当前语句块的开头"

#define RS_ERR_UNKNOWN_IDENTIFIER L"未定义的标识符 '%s'"

#define RS_ERR_UNABLE_DECIDE_VAR_TYPE L"无法决断变量类型"

#define RS_ERR_UNABLE_DECIDE_FUNC_OVERRIDE L"无法确定要使用函数的哪一个重载, 或许是: %s"

#define RS_ERR_UNABLE_DECIDE_FUNC_SYMBOL L"无法确定要使用的函数"

#define RS_ERR_NO_MATCH_FUNC_OVERRIDE L"没有找到匹配参数的函数重载"

#define RS_ERR_ARGUMENT_TOO_FEW L"无法调用 '%s': 参数过少"

#define RS_ERR_ARGUMENT_TOO_MANY L"无法调用 '%s': 参数过多"

#define RS_ERR_TYPE_CANNOT_BE_CALL L"无法调用 '%s'."

#define RS_ERR_UNKNOWN_TYPE L"未知类型 '%s'."

#define RS_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE L"无法找到函数 '%s' 类型为 '%s' 的重载"

#define RS_ERR_NEED_TYPES L"这里应该是 '%s'."

#define RS_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC L"只有拥有变长参数的函数中可以使用 '...' 来打包变长参数."

#define RS_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL L"解包运算符只能用于函数调用"

#define RS_ERR_REDEFINED L"在当前作用域中重复定义了 '%s'"

#define RS_ERR_IS_NOT_A_TYPE L"标识符 '%s' 不是一个类型"

#define RS_ERR_IS_A_TYPE L"标识符 '%s' 是一个类型"

#define RS_ERR_SYMBOL_IS_AMBIGUOUS L"标识符 '%s' 不明确, 它在以下命名空间中被找到: "

#define RS_ERR_CANNOT_IMPLCAST_REF L"不能隐式转换引用传递对象的类型"

#define RS_ERR_NOT_REFABLE_INIT_ITEM L"不可引用的对象不能作为 'ref' 的初始项"

#define RS_ERR_UNINDEXABLE_TYPE L"不可索引的类型 '%s'"

#define RS_ERR_CANNOT_AS_TYPE L"类型 '%s' 与要求的类型 '%s' 不相同"

#define RS_ERR_CANNOT_AS_COMPLEX_TYPE L"'as' 不允许在运行时测试复杂类型"

#define RS_ERR_CANNOT_AS_DYNAMIC L"'as dynamic' 是无效的"


#define RS_WARN_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"以 '%c' 开头的未知转义序列."

#define RS_WARN_REPEAT_ATTRIBUTE L"重复出现的属性"

#define RS_WARN_FUNC_WILL_RETURN_DYNAMIC L"函数中返回的类型不兼容, 函数返回类型将被标记为 'dynamic'"

#define RS_WARN_OVERRIDDEN_DYNAMIC_TYPE L"类型 'dynamic' 被覆盖"

#define RS_WARN_CAST_REF L"正在尝试转换引用传递对象的类型，'ref' 将失效"

#else

#define RS_TERM_GLOBAL_NAMESPACE L"global namespace"

#define RS_TERM_OR L"or"

#define RS_TERM_AT L"at"

#define RS_TERM_EXCEPTED L"excepted"



#define RS_ERR_MISMATCH_ANNO_SYM L"Mismatched annotation symbols."

#define RS_ERR_UNEXCEPT_CH_AFTER_CH L"Unexcepted character '%c' after '%c'."

#define RS_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"Unexcepted character '%c' after '%c', except '%c'."

#define RS_ERR_UNEXCEPT_EOF L"Unexcepted EOF."

#define RS_ERR_UNEXCEPT_TOKEN L"Unexcepted token: "

#define RS_ERR_ILLEGAL_LITERAL L"Illegal literal."

#define RS_ERR_UNKNOW_OPERATOR_STR L"Unknown operator: '%s'."

#define RS_ERR_UNEXCEPTED_EOL_IN_STRING L"Unexcepted end of line when parsing string."

#define RS_ERR_LEXER_ERR_UNKNOW_NUM_BASE L"Lexer error, unknown number base."

#define RS_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L"Lexer error, unknown begin character: '%c'."

#define RS_ERR_UNEXCEPT_AST_NODE_TYPE L"Unexcepted node type: should be ast node or token."

#define RS_ERR_SHOULD_BE_AST_BASE L"Unknown error when parsing: unexcepted node type."

#define RS_ERR_UNABLE_RECOVER_FROM_ERR L"Unable to recover from now error state, abort."

#define RS_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME L"Can not be declared as 'public', 'protected' or 'private' at the same time."

#define RS_ERR_TYPE_CANNOT_NEGATIVE L"'%s' cannot be negative."

#define RS_ERR_CANNOT_OPEN_FILE L"Cannot open file: '%s'."

#define RS_ERR_UNPACK_ARG_LESS_THEN_ONE L"Unpacking operate should unpack at least 1 arguments."

#define RS_ERR_CANNOT_FIND_EXT_SYM L"Cannot find extern symbol: '%s'."

#define RS_ERR_ARG_DEFINE_AFTER_VARIADIC L"There should be no argument after '...'."

#define RS_ERR_CANNOT_CALC_STR_WITH_THIS_OP L"Unsupported string operations."

#define RS_ERR_CANNOT_CALC_HANDLE_WITH_THIS_OP L"Unsupported handle operations."

#define RS_ERR_CANNOT_ASSIGN_TO_CONSTANT L"Can not assign to a constant."

#define RS_ERR_CANNOT_CALC_WITH_L_AND_R L"The value types on the left and right are incompatible."

#define RS_ERR_CANNOT_INDEX_STR_WITH_TYPE L"Can not index string with '%s'."

#define RS_ERR_CANNOT_CAST_TYPE_TO_TYPE  L"Cannot cast '%s' to '%s'."

#define RS_ERR_CANNOT_IMPLCAST_TYPE_TO_TYPE L"Cannot implicit-cast '%s' to '%s'."

#define RS_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE L"Cannot assign '%s' to '%s'."

#define RS_ERR_CANNOT_DO_RET_OUSIDE_FUNC L"Invalid return, cannot do return ouside of function."

#define RS_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME L"Cannot return '%s' and '%s' at same time."

#define RS_ERR_ERR_PLACE_FOR_USING_NAMESPACE L"The declaration of using-namespace should be placed at the beginning of the statement block."

#define RS_ERR_UNKNOWN_IDENTIFIER L"Unknown identifier '%s'."

#define RS_ERR_UNABLE_DECIDE_VAR_TYPE L"Unable to decide variable type."

#define RS_ERR_UNABLE_DECIDE_FUNC_OVERRIDE L"Cannot judge which function override to call, maybe: %s."

#define RS_ERR_UNABLE_DECIDE_FUNC_SYMBOL L"Cannot decided which function to use."

#define RS_ERR_NO_MATCH_FUNC_OVERRIDE L"No matched function override to call."

#define RS_ERR_ARGUMENT_TOO_FEW L"Argument count too few to call '%s'."

#define RS_ERR_ARGUMENT_TOO_MANY L"Argument count too many to call '%s'."

#define RS_ERR_TYPE_CANNOT_BE_CALL L"Cannot call '%s'."

#define RS_ERR_UNKNOWN_TYPE L"Unknown type '%s'."

#define RS_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE L"Cannot find overload of '%s' with type '%s' ."

#define RS_ERR_NEED_TYPES L"Here need '%s'."

#define RS_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC L"Only in function having variadic arguments can use '...' to pack variadic arguments."

#define RS_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL L"Unpack is only allowed for arguments in function calls."

#define RS_ERR_REDEFINED L"Redefined '%s' in this scope."

#define RS_ERR_IS_NOT_A_TYPE L"'%s' is not a type."

#define RS_ERR_IS_A_TYPE L"'%s' is a type."

#define RS_ERR_SYMBOL_IS_AMBIGUOUS L"'%s' is ambiguous, it was found in namespace: "

#define RS_ERR_CANNOT_IMPLCAST_REF L"Cannot implicit-cast the type of reference passing object."

#define RS_ERR_NOT_REFABLE_INIT_ITEM L"Non-referenceable objects cannot be used as the initial item of 'ref'."

#define RS_ERR_UNINDEXABLE_TYPE L"Unindexable type '%s'."

#define RS_ERR_CANNOT_AS_TYPE L"The type '%s' is not the same as the requested type '%s'."

#define RS_ERR_CANNOT_AS_COMPLEX_TYPE L"The 'as' operation does not allow testing complex types at runtime."

#define RS_ERR_CANNOT_AS_DYNAMIC L"'as dynamic' is useless."



#define RS_WARN_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"Unknown escape sequences begin with '%c'."

#define RS_WARN_REPEAT_ATTRIBUTE L"Duplicate attribute description."

#define RS_WARN_FUNC_WILL_RETURN_DYNAMIC L"Incompatible with the return type, the return value will be determined to be 'dynamic'."

#define RS_WARN_OVERRIDDEN_DYNAMIC_TYPE L"Overridden 'dynamic' attributes."

#define RS_WARN_CAST_REF L"Trying cast the type of reference passing object, 'ref' will be useless."

#endif