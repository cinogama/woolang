#pragma once
// This file using to store compiler/lexer's error informations.
// Here will be 

#define WO_LANG_EN 0
#define WO_LANG_ZH_CN 1

#define WO_USED_LANGUAGE WO_LANG_ZH_CN





#if WO_USED_LANGUAGE == WO_LANG_ZH_CN

#define WO_TERM_GLOBAL_NAMESPACE L"全局作用域"

#define WO_TERM_OR L"或"

#define WO_TERM_AT L"位于"

#define WO_TERM_AND L"和"

#define WO_TERM_EXCEPTED L"应当是"


#define WO_ERR_MISMATCH_ANNO_SYM L"不匹配的注释符"

#define WO_ERR_UNEXCEPT_CH_AFTER_CH L"未预料的符号 '%lc' 位于 '%lc' 之后"

#define WO_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"未预料的符号 '%lc' 位于 '%lc' 之后, 此处应该是 '%lc'"

#define WO_ERR_UNEXCEPT_EOF L"未预料的文件尾"

#define WO_ERR_UNEXCEPT_TOKEN L"未预料的符号: "

#define WO_ERR_ILLEGAL_LITERAL L"非法的字面常量"

#define WO_ERR_UNKNOW_OPERATOR_STR L"未知运算符: '%ls'"

#define WO_ERR_UNEXCEPTED_EOL_IN_STRING L"在字符串常量中发现换行符"

#define WO_ERR_LEXER_ERR_UNKNOW_NUM_BASE L"词法错误，未知的常量基数"

#define WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L"词法错误, 未知的符号: '%lc'"

#define WO_ERR_UNEXCEPT_AST_NODE_TYPE L"未预料到的语法树节点类型: 应当是语法树节点或标识符"

#define WO_ERR_SHOULD_BE_AST_BASE L"语法分析时发生未知错误: 未知节点类型"

#define WO_ERR_UNABLE_RECOVER_FROM_ERR L"无法从当前错误中恢复，编译终止"

#define WO_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME L"不可以同时声明为 'public', 'protected' 或者 'private'"

#define WO_ERR_TYPE_CANNOT_NEGATIVE L"'%ls' 不能被取负值"

#define WO_ERR_CANNOT_OPEN_FILE L"无法打开文件: '%ls'"

#define WO_ERR_UNPACK_ARG_LESS_THEN_ONE L"展开参数包时至少需要展开一个参数"

#define WO_ERR_CANNOT_FIND_EXT_SYM L"无法找到外部符号: '%ls'"

#define WO_ERR_CANNOT_FIND_EXT_SYM_IN_LIB L"无法找到外部符号: '%ls' 位于 '%ls'"

#define WO_ERR_ARG_DEFINE_AFTER_VARIADIC L"在 '...' 之后不应该有其他参数"

#define WO_ERR_CANNOT_CALC_STR_WITH_THIS_OP L"不支持对字符串进行该运算"

#define WO_ERR_CANNOT_CALC_HANDLE_WITH_THIS_OP L"不支持对句柄类型进行该运算"

#define WO_ERR_CANNOT_ASSIGN_TO_CONSTANT L"不允许向常量赋值"

#define WO_ERR_CANNOT_CALC_WITH_L_AND_R L"运算符左右两边的值类型（%ls和%ls）不兼容，无法计算"

#define WO_ERR_CANNOT_INDEX_STR_WITH_TYPE L"不能使用 '%ls' 类型的值索引字符串"

#define WO_ERR_CANNOT_CAST_TYPE_TO_TYPE  L"无法将 '%ls' 类型的值转换为 '%ls'"

#define WO_ERR_CANNOT_IMPLCAST_TYPE_TO_TYPE L"无法将 '%ls' 类型的值隐式转换为 '%ls'"

#define WO_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE L"不能将 '%ls' 类型的值赋值给 '%ls' 类型的变量"

#define WO_ERR_CANNOT_DO_RET_OUSIDE_FUNC L"非法的返回操作, 不允许在函数范围外进行返回"

#define WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME L"不能同时返回 '%ls' 和 '%ls'"

#define WO_ERR_ERR_PLACE_FOR_USING_NAMESPACE L"使用命名空间声明语句应位于当前语句块的开头"

#define WO_ERR_UNKNOWN_IDENTIFIER L"未定义的标识符 '%ls'"

#define WO_ERR_UNABLE_DECIDE_VAR_TYPE L"无法决断变量类型"

#define WO_ERR_UNABLE_DECIDE_FUNC_OVERRIDE L"无法确定要使用函数的哪一个重载, 或许是: %ls"

#define WO_ERR_UNABLE_DECIDE_FUNC_SYMBOL L"无法确定要使用的函数"

#define WO_ERR_NO_MATCH_FUNC_OVERRIDE L"没有找到匹配参数的函数重载"

#define WO_ERR_ARGUMENT_TOO_FEW L"无法调用 '%ls': 参数过少"

#define WO_ERR_ARGUMENT_TOO_MANY L"无法调用 '%ls': 参数过多"

#define WO_ERR_TYPE_CANNOT_BE_CALL L"无法调用 '%ls'."

#define WO_ERR_UNKNOWN_TYPE L"未知类型 '%ls'."

#define WO_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE L"无法找到函数 '%ls' 类型为 '%ls' 的重载"

#define WO_ERR_NEED_TYPES L"这里应该是 '%ls'."

#define WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC L"只有拥有变长参数的函数中可以使用 '...' 来打包变长参数."

#define WO_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL L"解包运算符只能用于函数调用"

#define WO_ERR_REDEFINED L"在当前作用域中重复定义了 '%ls'"

#define WO_ERR_IS_NOT_A_TYPE L"标识符 '%ls' 不是一个类型"

#define WO_ERR_IS_A_TYPE L"标识符 '%ls' 是一个类型"

#define WO_ERR_SYMBOL_IS_AMBIGUOUS L"标识符 '%ls' 不明确, 它在以下命名空间中被找到: "

#define WO_ERR_CANNOT_IMPLCAST_REF L"不能隐式转换引用传递对象的类型"

#define WO_ERR_NOT_REFABLE_INIT_ITEM L"不可引用的对象不能作为 'ref' 的初始项"

#define WO_ERR_UNINDEXABLE_TYPE L"不可索引的类型 '%ls'"

#define WO_ERR_CANNOT_AS_TYPE L"类型 '%ls' 与要求的类型 '%ls' 不相同"

#define WO_ERR_CANNOT_TEST_COMPLEX_TYPE L"不允许在运行时检查复杂类型"

#define WO_ERR_CANNOT_AS_DYNAMIC L"此处出现的 'dynamic' 是无效的"

#define WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM L"不可赋值的对象"

#define WO_ERR_CANNOT_MAKE_UNASSABLE_ITEM_REF L"不可赋值的对象不能被标注为引用传递"

#define WO_ERR_CANNOT_EXPORT_SYMB_IN_FUNC L"不允许导出定义于函数中的符号"

#define WO_ERR_TEMPLATE_ARG_NOT_MATCH L"泛型参数不匹配"

#define WO_ERR_ARRAY_NEED_ONE_TEMPLATE_ARG L"'array' 类型需要提供一个类型参数"

#define WO_ERR_MAP_NEED_TWO_TEMPLATE_ARG L"'map' 类型需要提供两个类型参数"

#define WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE L"无法推导函数 '%ls' 的返回类型，需要手动标注"

#define WO_ERR_NO_TEMPLATE_VARIABLE_OR_FUNCTION L"此处泛型不能用于修饰非变量/函数对象"

#define WO_ERR_NO_MATCHED_TEMPLATE_FUNC L"未找到符合模板参数的函数重载"

#define WO_ERR_UNDEFINED_MEMBER L"尝试索引未定义的成员 '%ls' "

#define WO_ERR_CANNOT_INDEX_MEMB_WITHOUT_STR L"尝试使用非字符串常量索引具类型映射"

#define WO_ERR_TOO_MANY_ITER_ITEM_FROM_NEXT L"迭代器类型'%ls'的'next'方法无法接受%zu个迭代项目"

#define WO_ERR_VARIADIC_NEXT_IS_ILEAGAL L"迭代器类型'%ls'的'next'方法不可以是变长的"

#define WO_ERR_INVALID_OPERATE L"无效的'%ls'"

#define WO_ERR_FUNC_RETURN_DIFFERENT_TYPES L"函数出现了不同类型的返回值，这是不被允许的"

#define WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID L"嵌套格式化字符串是不被允许的"

#define WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE L"重复的宏定义 '%ls'"

#define WO_ERR_UNKNOWN_PRAGMA_COMMAND L"未知的预处理指令 '%ls'"

#define WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR L"宏控制器编译失败：\n%ls"

#define WO_ERR_HERE_SHOULD_HAVE L"缺少 '%ls'"

#define WO_ERR_UNEXPECT_PATTERN_MODE L"未预料到的模式类型"

#define WO_ERR_UNEXPECT_PATTERN_CASE L"未预料到的分支类型"

#define WO_ERR_TEMPLATE_FUNCTION_SHOULD_SPECIFY_TEMPLATE_AGRS L"函数是一个泛型函数，需要指定泛型参数"

#define WO_ERR_LOGIC_NOT_ONLY_ACCEPT_BOOL L"逻辑非运算符只允许处理 'bool' 类型的参数"

#define WO_ERR_REPEAT_MATCH_CASE L"'match' 语句中不能有重复的 'case' 分支"

#define WO_ERR_MATCH_CASE_NOT_COMPLETE L"'match' 语句必须穷尽所有可能的 'case' 分支"

#define WO_ERR_UNKNOWN_MATCHING_VAL_TYPE L"正在 'match' 的值类型未决断，无法为后续代码进行推导"

#define WO_ERR_UNKNOWN_CASE_TYPE L"无效的 'case'，此处只能是正在match的值类型"

#define WO_ERR_INVALID_CASE_TYPE_NEED_ACCEPT_ARG L"无效的 'case'，union 模式不匹配，需要接收一个参数"

#define WO_ERR_INVALID_CASE_TYPE_NO_ARG_RECV L"无效的 'case'，union 模式不匹配，此处不能接收参数"

#define WO_ERR_CANNOT_EXPORT_SAME_NAME_FUNCTION L"函数符号 '%ls' 此前已经被导出，导出同一命名空间下的同名函数是不允许的"

#define WO_ERR_CANNOT_REACH_PRIVATE_IN_OTHER_FUNC L"无法访问 '%ls'，这是一个私有对象，只能在定义所在的源文件中访问"

#define WO_ERR_CANNOT_REACH_PROTECTED_IN_OTHER_FUNC L"无法访问 '%ls'，这是一个保护对象，只能在定义所在的命名空间中访问"

#define WO_ERR_CANNOT_CAPTURE_IN_NAMED_FUNC L"不能在非匿名函数中捕获变量 '%ls'"

#define WO_ERR_CANNOT_CAPTURE_IN_OUTSIDE_FUNC L"不允许捕获来自更外层的变量 '%ls'"

#define WO_ERR_REF_ARG_IN_OPERATOR_OVERLOAD_FUNC L"运算符 '%ls' 的重载函数不能接收引用参数"

#define WO_ERR_THERE_IS_NO_MEMBER_NAMED L"类型 '%ls' 中没有名为 '%ls' 的成员"

#define WO_ERR_CONSTRUCT_STRUCT_NOT_FINISHED L"构造结构体 '%ls' 时没有提供所有成员的初始值"

#define WO_ERR_ONLY_CONSTRUCT_STRUCT_BY_THIS_WAY L"仅结构体类型允许使用形如 Type { ... } 的方法构建"

#define WO_ERR_TYPE_IN_SHOULD_BE_BOOL L"'%ls' 表达式判别的值类型应该为 'bool'，但是给定的是 '%ls'"

#define WO_ERR_RELATION_CANNOT_COMPARE L"关系运算符 '%ls' 不能用于比较 '%ls'"

#define WO_ERR_VALUE_TYPE_HERE_SHOULD_BE L"此处的值类型应该是 '%ls'，但是给定的是 '%ls'"




#define WO_WARN_WARNING_GIVEN_BY_MACRO L"宏控制器报告了一个警告：\n%ls"

#define WO_WARN_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"以 '%lc' 开头的未知转义序列."

#define WO_WARN_REPEAT_ATTRIBUTE L"重复出现的属性"

#define WO_WARN_OVERRIDDEN_DYNAMIC_TYPE L"类型 'dynamic' 被覆盖"

#define WO_WARN_CAST_REF L"正在尝试转换引用传递对象的类型，'ref' 将失效"

#else

#define WO_TERM_GLOBAL_NAMESPACE L"global namespace"

#define WO_TERM_OR L"or"

#define WO_TERM_AT L"at"

#define WO_TERM_EXCEPTED L"excepted"

#define WO_TERM_AND L"and"


#define WO_ERR_MISMATCH_ANNO_SYM L"Mismatched annotation symbols."

#define WO_ERR_UNEXCEPT_CH_AFTER_CH L"Unexcepted character '%lc' after '%lc'."

#define WO_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"Unexcepted character '%lc' after '%lc', except '%lc'."

#define WO_ERR_UNEXCEPT_EOF L"Unexcepted EOF."

#define WO_ERR_UNEXCEPT_TOKEN L"Unexcepted token: "

#define WO_ERR_ILLEGAL_LITERAL L"Illegal literal."

#define WO_ERR_UNKNOW_OPERATOR_STR L"Unknown operator: '%ls'."

#define WO_ERR_UNEXCEPTED_EOL_IN_STRING L"Unexcepted end of line when parsing string."

#define WO_ERR_LEXER_ERR_UNKNOW_NUM_BASE L"Lexer error, unknown number base."

#define WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L"Lexer error, unknown begin character: '%lc'."

#define WO_ERR_UNEXCEPT_AST_NODE_TYPE L"Unexcepted node type: should be ast node or token."

#define WO_ERR_SHOULD_BE_AST_BASE L"Unknown error when parsing: unexcepted node type."

#define WO_ERR_UNABLE_RECOVER_FROM_ERR L"Unable to recover from now error state, abort."

#define WO_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME L"Can not be declared as 'public', 'protected' or 'private' at the same time."

#define WO_ERR_TYPE_CANNOT_NEGATIVE L"'%ls' cannot be negative."

#define WO_ERR_CANNOT_OPEN_FILE L"Cannot open file: '%ls'."

#define WO_ERR_UNPACK_ARG_LESS_THEN_ONE L"Unpacking operate should unpack at least 1 arguments."

#define WO_ERR_CANNOT_FIND_EXT_SYM L"Cannot find extern symbol: '%ls'."

#define WO_ERR_CANNOT_FIND_EXT_SYM_IN_LIB L"Cannot find extern symbol: '%ls' in '%ls'"

#define WO_ERR_ARG_DEFINE_AFTER_VARIADIC L"There should be no argument after '...'."

#define WO_ERR_CANNOT_CALC_STR_WITH_THIS_OP L"Unsupported string operations."

#define WO_ERR_CANNOT_CALC_HANDLE_WITH_THIS_OP L"Unsupported handle operations."

#define WO_ERR_CANNOT_ASSIGN_TO_CONSTANT L"Can not assign to a constant."

#define WO_ERR_CANNOT_CALC_WITH_L_AND_R L"The value types on the left and right (%ls and %ls) are incompatible."

#define WO_ERR_CANNOT_INDEX_STR_WITH_TYPE L"Can not index string with '%ls'."

#define WO_ERR_CANNOT_CAST_TYPE_TO_TYPE  L"Cannot cast '%ls' to '%ls'."

#define WO_ERR_CANNOT_IMPLCAST_TYPE_TO_TYPE L"Cannot implicit-cast '%ls' to '%ls'."

#define WO_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE L"Cannot assign '%ls' to '%ls'."

#define WO_ERR_CANNOT_DO_RET_OUSIDE_FUNC L"Invalid return, cannot do return ouside of function."

#define WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME L"Cannot return '%ls' and '%ls' at same time."

#define WO_ERR_ERR_PLACE_FOR_USING_NAMESPACE L"The declaration of using-namespace should be placed at the beginning of the statement block."

#define WO_ERR_UNKNOWN_IDENTIFIER L"Unknown identifier '%ls'."

#define WO_ERR_UNABLE_DECIDE_VAR_TYPE L"Unable to decide variable type."

#define WO_ERR_UNABLE_DECIDE_FUNC_OVERRIDE L"Cannot judge which function override to call, maybe: %ls."

#define WO_ERR_UNABLE_DECIDE_FUNC_SYMBOL L"Cannot decided which function to use."

#define WO_ERR_NO_MATCH_FUNC_OVERRIDE L"No matched function override to call."

#define WO_ERR_ARGUMENT_TOO_FEW L"Argument count too few to call '%ls'."

#define WO_ERR_ARGUMENT_TOO_MANY L"Argument count too many to call '%ls'."

#define WO_ERR_TYPE_CANNOT_BE_CALL L"Cannot call '%ls'."

#define WO_ERR_UNKNOWN_TYPE L"Unknown type '%ls'."

#define WO_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE L"Cannot find overload of '%ls' with type '%ls' ."

#define WO_ERR_NEED_TYPES L"Here need '%ls'."

#define WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC L"Only in function having variadic arguments can use '...' to pack variadic arguments."

#define WO_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL L"Unpack is only allowed for arguments in function calls."

#define WO_ERR_REDEFINED L"Redefined '%ls' in this scope."

#define WO_ERR_IS_NOT_A_TYPE L"'%ls' is not a type."

#define WO_ERR_IS_A_TYPE L"'%ls' is a type."

#define WO_ERR_SYMBOL_IS_AMBIGUOUS L"'%ls' is ambiguous, it was found in namespace: "

#define WO_ERR_CANNOT_IMPLCAST_REF L"Cannot implicit-cast the type of reference passing object."

#define WO_ERR_NOT_REFABLE_INIT_ITEM L"Non-referenceable objects cannot be used as the initial item of 'ref'."

#define WO_ERR_UNINDEXABLE_TYPE L"Unindexable type '%ls'."

#define WO_ERR_CANNOT_AS_TYPE L"The type '%ls' is not the same as the requested type '%ls'."

#define WO_ERR_CANNOT_TEST_COMPLEX_TYPE L"Cannot testing complex types at runtime."

#define WO_ERR_CANNOT_AS_DYNAMIC L"'dynamic' here is useless."

#define WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM L"Cannot assign to a non-assignable item."

#define WO_ERR_CANNOT_MAKE_UNASSABLE_ITEM_REF L"Cannot mark a a non-assignable item as 'ref'."

#define WO_ERR_CANNOT_EXPORT_SYMB_IN_FUNC L"Cannot export the symbol defined in function."

#define WO_ERR_TEMPLATE_ARG_NOT_MATCH L"Not matched template arguments."

#define WO_ERR_ARRAY_NEED_ONE_TEMPLATE_ARG L"Type 'array' requires a type as generic parameters."

#define WO_ERR_MAP_NEED_TWO_TEMPLATE_ARG L"Type 'map' requires two type as generic parameters."

#define WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE L"Unable to deduce the return type of function '%ls', it needs to be marked manually."

#define WO_ERR_NO_TEMPLATE_VARIABLE_OR_FUNCTION L"Templated items beside variable or function is invalid here."

#define WO_ERR_NO_MATCHED_TEMPLATE_FUNC L"No matched template function."

#define WO_ERR_UNDEFINED_MEMBER L"try index undefined '%ls'."

#define WO_ERR_CANNOT_INDEX_MEMB_WITHOUT_STR L"Typed mapping only indexable form 'string'."

#define WO_ERR_TOO_MANY_ITER_ITEM_FROM_NEXT L"Iterator '%ls': Cannot get %zu items from function 'next'."

#define WO_ERR_VARIADIC_NEXT_IS_ILEAGAL L"Function 'next' of iterator '%ls' cannot be variadic."

#define WO_ERR_INVALID_OPERATE L"Invalid '%ls'."

#define WO_ERR_FUNC_RETURN_DIFFERENT_TYPES L"Different types of return values in function."

#define WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID L"嵌套格式化字符串是不被允许的"

#define WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE L"重复的宏定义 '%ls'"

#define WO_ERR_UNKNOWN_PRAGMA_COMMAND L"未知的预处理指令 '%ls'"

#define WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR L"宏控制器编译失败：\n%ls"

#define WO_ERR_HERE_SHOULD_HAVE L"缺少 '%ls'"

#define WO_ERR_UNEXPECT_PATTERN_MODE L"未预料到的模式类型"

#define WO_ERR_UNEXPECT_PATTERN_CASE L"未预料到的分支类型"

#define WO_ERR_TEMPLATE_FUNCTION_SHOULD_SPECIFY_TEMPLATE_AGRS L"函数是一个泛型函数，需要指定泛型参数"

#define WO_ERR_LOGIC_NOT_ONLY_ACCEPT_BOOL L"逻辑非运算符只允许处理 'bool' 类型的参数"

#define WO_ERR_REPEAT_MATCH_CASE L"'match' 语句中不能有重复的 'case' 分支"

#define WO_ERR_MATCH_CASE_NOT_COMPLETE L"'match' 语句必须穷尽所有可能的 'case' 分支"

#define WO_ERR_UNKNOWN_MATCHING_VAL_TYPE L"正在 'match' 的值类型未决断，无法为后续代码进行推导"

#define WO_ERR_UNKNOWN_CASE_TYPE L"无效的 'case'，此处只能是正在match的值类型"

#define WO_ERR_INVALID_CASE_TYPE_NEED_ACCEPT_ARG L"无效的 'case'，union 模式不匹配，需要接收一个参数"

#define WO_ERR_INVALID_CASE_TYPE_NO_ARG_RECV L"无效的 'case'，union 模式不匹配，此处不能接收参数"

#define WO_ERR_CANNOT_EXPORT_SAME_NAME_FUNCTION L"函数符号 '%ls' 此前已经被导出，导出同一命名空间下的同名函数是不允许的"

#define WO_ERR_CANNOT_REACH_PRIVATE_IN_OTHER_FUNC L"无法访问 '%ls'，这是一个私有对象，只能在定义所在的源文件中访问"

#define WO_ERR_CANNOT_REACH_PROTECTED_IN_OTHER_FUNC L"无法访问 '%ls'，这是一个保护对象，只能在定义所在的命名空间中访问"

#define WO_ERR_CANNOT_CAPTURE_IN_NAMED_FUNC L"不能在非匿名函数中捕获变量 '%ls'"

#define WO_ERR_CANNOT_CAPTURE_IN_OUTSIDE_FUNC L"不允许捕获来自更外层的变量 '%ls'"

#define WO_ERR_REF_ARG_IN_OPERATOR_OVERLOAD_FUNC L"运算符 '%ls' 的重载函数不能接收引用参数"

#define WO_ERR_THERE_IS_NO_MEMBER_NAMED L"类型 '%ls' 中没有名为 '%ls' 的成员"

#define WO_ERR_CONSTRUCT_STRUCT_NOT_FINISHED L"构造结构体 '%ls' 时没有提供所有成员的初始值"

#define WO_ERR_ONLY_CONSTRUCT_STRUCT_BY_THIS_WAY L"仅结构体类型允许使用形如 Type { ... } 的方法构建"

#define WO_ERR_TYPE_IN_SHOULD_BE_BOOL L"'%ls' 表达式判别的值类型应该为 'bool'，但是给定的是 '%ls'"

#define WO_ERR_RELATION_CANNOT_COMPARE L"关系运算符 '%ls' 不能用于比较 '%ls'"

#define WO_ERR_VALUE_TYPE_HERE_SHOULD_BE L"此处的值类型应该是 '%ls'，但是给定的是 '%ls'"




#define WO_WARN_WARNING_GIVEN_BY_MACRO L"宏控制器报告了一个警告：\n%ls"

#define WO_WARN_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"Unknown escape sequences begin with '%lc'."

#define WO_WARN_REPEAT_ATTRIBUTE L"Duplicate attribute description."

#define WO_WARN_OVERRIDDEN_DYNAMIC_TYPE L"Overridden 'dynamic' attributes."

#define WO_WARN_CAST_REF L"Trying cast the type of reference passing object, 'ref' will be useless."

#endif