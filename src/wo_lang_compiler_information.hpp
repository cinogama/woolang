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

#define WO_MACRO_CODE_END_HERE L"此宏定义在此处结束："

#define WO_MACRO_ANALYZE_END_HERE L"此宏解析在此处结束："

#define WO_TOO_MANY_ERROR(count) L"报告的错误数量太多，仅显示 99/" + std::to_wstring(count) +  L" 条，终止"


#define WO_ERR_MISMATCH_ANNO_SYM L"不匹配的注释符"

#define WO_ERR_UNEXCEPT_CH_AFTER_CH L"未预料的字符 '%lc' 位于 '%lc' 之后"

#define WO_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"未预料的字符 '%lc' 位于 '%lc' 之后, 此处应该是 '%lc'"

#define WO_ERR_UNEXCEPT_EOF L"未预料的文件尾"

#define WO_ERR_UNEXCEPT_TOKEN L"未预料的标识符: "

#define WO_ERR_ILLEGAL_LITERAL L"非法的字面常量"

#define WO_ERR_UNKNOW_OPERATOR_STR L"未知运算符: '%ls'"

#define WO_ERR_UNEXCEPTED_EOL_IN_CHAR L"在字符常量中发现换行符"

#define WO_ERR_UNEXCEPTED_EOL_IN_STRING L"在字符串常量中发现换行符"

#define WO_ERR_LEXER_ERR_UNKNOW_NUM_BASE L"词法错误，未知的常量基数"

#define WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L"词法错误, 未知的字符: '%lc'"

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

#define WO_ERR_CANNOT_ASSIGN_TO_CONSTANT L"不允许向常量赋值"

#define WO_ERR_CANNOT_CALC_WITH_L_AND_R L"运算符左右两边的值类型（%ls和%ls）不兼容，无法计算"

#define WO_ERR_CANNOT_INDEX_STR_WITH_TYPE L"不能使用 '%ls' 类型的值索引字符串"

#define WO_ERR_CANNOT_CAST_TYPE_TO_TYPE  L"无法将 '%ls' 类型的值转换为 '%ls'"

#define WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE  L"此处应该是 '%ls'，但发现了 '%ls'"

#define WO_ERR_CANNOT_DO_RET_OUSIDE_FUNC L"非法的返回操作, 只允许在函数或全局命名空间内返回"

#define WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME L"不能同时返回 '%ls' 和 '%ls'"

#define WO_ERR_ERR_PLACE_FOR_USING_NAMESPACE L"使用命名空间声明语句应位于当前语句块的开头"

#define WO_ERR_UNKNOWN_IDENTIFIER L"未定义的标识符 '%ls'"

#define WO_ERR_UNABLE_DECIDE_EXPR_TYPE L"无法决断表达式类型"

#define WO_ERR_ARGUMENT_TOO_FEW L"无法调用 '%ls': 参数过少"

#define WO_ERR_ARGUMENT_TOO_MANY L"无法调用 '%ls': 参数过多"

#define WO_ERR_TYPE_CANNOT_BE_CALL L"无法如同函数一般调用 '%ls'类型的值"

#define WO_ERR_UNKNOWN_TYPE L"未知或不完整的类型 '%ls'."

#define WO_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE L"无法找到函数 '%ls' 类型为 '%ls' 的重载"

#define WO_ERR_NEED_TYPES L"这里应该是 '%ls'."

#define WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC L"只有拥有变长参数的函数中可以使用 '...' 来打包变长参数."

#define WO_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL L"解包运算符只能用于函数调用"

#define WO_ERR_REDEFINED L"在当前作用域中重复定义了 '%ls'"

#define WO_ERR_IS_NOT_A_TYPE L"标识符 '%ls' 不是一个类型"

#define WO_ERR_IS_A_TYPE L"标识符 '%ls' 是一个类型"

#define WO_ERR_SYMBOL_IS_AMBIGUOUS L"标识符 '%ls' 不明确, 它在以下命名空间中被找到: "

#define WO_ERR_UNINDEXABLE_TYPE L"不可索引的类型 '%ls'"

#define WO_ERR_CANNOT_AS_TYPE L"类型 '%ls' 与要求的类型 '%ls' 不相同"

#define WO_ERR_CANNOT_TEST_COMPLEX_TYPE L"类型 '%ls' 无法在运行时验证"

#define WO_ERR_CANNOT_AS_DYNAMIC L"此处出现的 'dynamic' 是无效的"

#define WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM L"不可赋值的对象"

#define WO_ERR_CANNOT_EXPORT_SYMB_IN_FUNC L"不允许导出定义于函数中的符号"

#define WO_ERR_TEMPLATE_ARG_NOT_MATCH L"泛型参数不匹配"

#define WO_ERR_TYPE_NEED_N_TEMPLATE_ARG L"'%ls' 类型需要提供 '%d' 个类型参数"

#define WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE L"无法推导函数 '%ls' 的返回类型，需要手动标注"

#define WO_ERR_NO_TEMPLATE_VARIABLE_OR_FUNCTION L"此处泛型不能用于修饰非变量/函数对象"

#define WO_ERR_NO_MATCHED_FUNC_TEMPLATE L"具体化函数时的泛型参数不匹配"

#define WO_ERR_UNDEFINED_MEMBER L"尝试索引未定义的成员 '%ls' "

#define WO_ERR_CANNOT_INDEX_MEMB_WITHOUT_STR L"尝试使用非字符串常量索引具类型映射"

#define WO_ERR_INVALID_OPERATE L"无效的'%ls'"

#define WO_ERR_FUNC_RETURN_DIFFERENT_TYPES L"函数出现了不同类型的返回值（之前推断或指明的是 '%ls', 但此处是 '%ls'），这是不被允许的"

#define WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID L"嵌套格式化字符串是不被允许的"

#define WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE L"重复的宏定义 '%ls'"

#define WO_ERR_UNKNOWN_PRAGMA_COMMAND L"未知的预处理指令 '%ls'"

#define WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR L"宏控制器编译失败：\n"

#define WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR L"宏 '%ls' 运行发生了错误：%ls"

#define WO_ERR_INVALID_TOKEN_MACRO_CONTROLOR L"宏 '%ls' 生成了非法的词法序列：\n"

#define WO_ERR_HERE_SHOULD_HAVE L"缺少 '%ls'"

#define WO_ERR_UNEXPECT_PATTERN_MODE L"未预料到的模式类型"

#define WO_ERR_UNEXPECT_PATTERN_CASE L"未预料到的分支类型"

#define WO_ERR_REPEAT_MATCH_CASE L"'match' 语句中不能有重复的 'case' 分支"

#define WO_ERR_MATCH_CASE_NOT_COMPLETE L"'match' 语句必须穷尽所有可能的 'case' 分支"

#define WO_ERR_UNKNOWN_MATCHING_VAL_TYPE L"正在 'match' 的值类型未决断，无法为后续代码进行推导"

#define WO_ERR_UNKNOWN_CASE_TYPE L"无效的 'case'，此处只能是正在match的值类型"

#define WO_ERR_INVALID_CASE_TYPE_NEED_ACCEPT_ARG L"无效的 'case'，union 模式不匹配，需要接收一个参数"

#define WO_ERR_INVALID_CASE_TYPE_NO_ARG_RECV L"无效的 'case'，union 模式不匹配，此处不能接收参数"

#define WO_ERR_CANNOT_REACH_PRIVATE_IN_OTHER_FUNC L"无法访问 '%ls'，这是一个私有对象，只能在源文件 '%ls' 中访问"

#define WO_ERR_CANNOT_REACH_PROTECTED_IN_OTHER_FUNC L"无法访问 '%ls'，这是一个保护对象，只能在定义所在的命名空间中访问"

#define WO_ERR_CANNOT_CAPTURE_IN_NAMED_FUNC L"不能在非匿名函数中捕获变量 '%ls'"

#define WO_ERR_CANNOT_CAPTURE_TEMPLATE_VAR L"不能捕获泛型变量 '%ls'"

#define WO_ERR_THERE_IS_NO_MEMBER_NAMED L"类型 '%ls' 中没有名为 '%ls' 的成员"

#define WO_ERR_CONSTRUCT_STRUCT_NOT_FINISHED L"构造结构体 '%ls' 时没有提供所有成员的初始值"

#define WO_ERR_ONLY_CONSTRUCT_STRUCT_BY_THIS_WAY L"仅结构体类型允许使用形如 'Type { ... }' 的方法构建"

#define WO_ERR_RELATION_CANNOT_COMPARE L"关系运算符 '%ls' 不能用于比较 '%ls'"

#define WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"以 '%lc' 开头的未知转义序列."

#define WO_ERR_REPEAT_ATTRIBUTE L"重复出现的属性"

#define WO_ERR_TOO_MANY_TEMPLATE_ARGS L"为类型 '%ls' 给定的泛型参数过多"

#define WO_ERR_TOO_FEW_TEMPLATE_ARGS L"为类型 '%ls' 给定的泛型参数过少"

#define WO_ERR_UNMATCHED_PATTERN_TYPE_NOT_DECIDED L"不匹配的模式：值类型未决"

#define WO_ERR_UNMATCHED_PATTERN_TYPE_EXPECT_TUPLE L"不匹配的模式：期待给定一个元组，但是给定的是 '%ls'"

#define WO_ERR_UNMATCHED_PATTERN_TYPE_TUPLE_DNT_MATCH L"不匹配的模式：期待给定一个拥有%d个元素的元组，但是给定的元组包含%d个元素"

#define WO_ERR_CANNOT_MATCH_SUCH_TYPE L"不允许使用 'match' 语句检查 '%ls' 类型的值，只接受 union 类型"

#define WO_ERR_INVALID_ITEM_OF L"'%ls' 不是 '%ls' 的合法项"

#define WO_ERR_FAILED_TO_INVOKE_FUNC_FOR_TYPE L"找不到名为 '%ls' 的函数，'->' 前的值类型为 '%ls'"

#define WO_ERR_FAILED_TO_DECIDE_ALL_TEMPLATE_ARGS L"一些泛型参数无法被推断"

#define WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_INDEX L"对元组的索引超出范围（元组包含 %d 项，而正在尝试索引第 %d 项）"

#define WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_TYPE L"只允许使用 'int' 类型的常量索引元组"

#define WO_ERR_FAILED_TO_DECIDE_TUPLE_TYPE L"元组元素类型未决，因此无法推导元组类型"

#define WO_ERR_FAILED_TO_CREATE_TUPLE_WITH_VAARG L"元组类型中不允许出现 '...'"

#define WO_ERR_DIFFERENT_TYPES_IN_COND_EXPR L"条件表达式的不同分支的值应该有相同的类型，但此处分别是 '%ls' 和 '%ls'"

#define WO_ERR_NOT_BOOL_VAL_IN_COND_EXPR L"条件表达式的判断表达式应该是bool类型，但此处是 '%ls'"

#define WO_ERR_FAILED_TO_INSTANCE_TEMPLATE_ID L"具体化泛型表达式 '%ls' 时失败"

#define WO_ERR_FAILED_TO_INDEX_VAARG_ERR_TYPE L"'变长参数包' 的索引只能是 'int' 类型的值"

#define WO_ERR_FAILED_TO_INVOKE_BECAUSE L"不满足此函数调用的要求，理由如下："

#define WO_ERR_DIFFERENT_KEY_TYPE_OF L"'%ls' 序列中的键类型不一致，无法为 '%ls' 推导泛型参数"

#define WO_ERR_DIFFERENT_VAL_TYPE_OF L"'%ls' 序列中的值类型不一致，无法为 '%ls' 推导泛型参数"

#define WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE L"'%ls' 序列中的键类型与泛型参数中指定的不一致"

#define WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE L"'%ls' 序列中的值类型与泛型参数中指定的不一致"

#define WO_ERR_DIFFERENT_MEMBER_TYPE_OF L"结构体成员 '%ls' 的类型应该是 '%ls'，但给定的初始值类型为 '%ls'"

#define WO_ERR_WHERE_COND_SHOULD_BE_CONST L"约束项必须得出一个常量结果"

#define WO_ERR_WHERE_COND_TYPE_ERR L"约束项得出的结果类型应该是 'bool'，但此处是 '%ls'"

#define WO_ERR_WHERE_COND_NOT_MEET L"检查发现不满足的条件"

#define WO_ERR_INVALID_KEY_EXPR L"创建映射时遇到了非法的键表达式"

#define WO_ERR_INVALID_GADT_CONFLICT L"发现了冲突的union项类型"

#define WO_ERR_USING_UNSAFE_NAMESPACE L"不允许使用包含 'unsafe' 的命名空间"

#define WO_ERR_NO_AGR_FOR_DEFAULT_PATTERN L"默认的 `union` 模式不可接受参数"

#define WO_ERR_CASE_AFTER_DEFAULT_PATTERN L"默认的 `union` 模式之后不可有其他匹配项"

#define WO_ERR_USELESS_DEFAULT_PATTERN L"无效的默认 `union` 模式，不可能执行到默认分支"

#define WO_ERR_INDEX_OUT_OF_RANGE L"索引常量时发生了访问越界"

#define WO_ERR_UNKNOWN_MACRO_NAMED L"未定义的宏：'%ls'"

#define WO_ERR_NOT_ALLOW_IGNORE_VALUE L"非void类型（此处为 '%ls'）的表达式不允许作为语句，这会导致重要的值被意外忽略"

#define WO_ERR_UNUSED_VARIABLE_DEFINE L"声明了未使用的 '%ls'，于函数 '%ls' 之中"

#define WO_ERR_DECL_BUILTIN_TYPE_IS_NOT_ALLOWED L"不允许创建与内置类型同名的新类型或类型别名"

#define WO_ERR_DECL_TEMPLATE_PATTERN_IS_NOT_ALLOWED L"只允许声明泛型变量，但此处是其他模式"

#define WO_ERR_UNKNOWN_EXTERN_ATTRIB L"未知的外部函数说明符: '%ls'"

#define WO_ERR_CANNOT_DIV_ZERO L"除数不可为 0"


#define WO_INFO_ITEM_IS_DEFINED_HERE L"编译器在此处找到了 '%ls' 的上一次定义"

#define WO_INFO_INIT_EXPR_IS_HERE L"编译器找到了 '%ls' 的初始化表达式"

#define WO_INFO_IS_THIS_ONE L"是否指的是这里定义的 '%ls'？"

#define WO_INFO_CANNOT_USE_UNREACHABLE_TYPE L"不可以在创建实例或转换类型时，使用无法访问的私有或保护类型"

#define WO_INFO_THE_TYPE_IS_ALIAS_OF L"类型 '%ls' 是 '%ls' 的类型别名"

#else

#define WO_TERM_GLOBAL_NAMESPACE L"global namespace"

#define WO_TERM_OR L"or"

#define WO_TERM_AT L"at"

#define WO_TERM_EXCEPTED L"excepted"

#define WO_TERM_AND L"and"

#define WO_MACRO_CODE_END_HERE L"The macro definition ends here:"

#define WO_MACRO_ANALYZE_END_HERE L"This macro parsing ends here:"

#define WO_TOO_MANY_ERROR(count) L"Too many errors, only display 99/" + std::to_wstring(count) +  L" errors, abort."


#define WO_ERR_MISMATCH_ANNO_SYM L"Mismatched annotation symbols."

#define WO_ERR_UNEXCEPT_CH_AFTER_CH L"Unexcepted character '%lc' after '%lc'."

#define WO_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"Unexcepted character '%lc' after '%lc', except '%lc'."

#define WO_ERR_UNEXCEPT_EOF L"Unexcepted EOF."

#define WO_ERR_UNEXCEPT_TOKEN L"Unexcepted token: "

#define WO_ERR_ILLEGAL_LITERAL L"Illegal literal."

#define WO_ERR_UNKNOW_OPERATOR_STR L"Unknown operator: '%ls'."

#define WO_ERR_UNEXCEPTED_EOL_IN_CHAR L"Unexcepted end of line when parsing char."

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

#define WO_ERR_CANNOT_ASSIGN_TO_CONSTANT L"Can not assign to a constant."

#define WO_ERR_CANNOT_CALC_WITH_L_AND_R L"The value types on the left and right (%ls and %ls) are incompatible."

#define WO_ERR_CANNOT_INDEX_STR_WITH_TYPE L"Can not index string with '%ls'."

#define WO_ERR_CANNOT_CAST_TYPE_TO_TYPE  L"Cannot cast '%ls' to '%ls'."

#define WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE  L"Here should be '%ls', but found '%ls'."

#define WO_ERR_CANNOT_DO_RET_OUSIDE_FUNC L"Invalid return, returns are only allowed within functions or global namespaces."

#define WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME L"Cannot return '%ls' and '%ls' at same time."

#define WO_ERR_ERR_PLACE_FOR_USING_NAMESPACE L"The declaration of using-namespace should be placed at the beginning of the statement block."

#define WO_ERR_UNKNOWN_IDENTIFIER L"Unknown identifier '%ls'."

#define WO_ERR_UNABLE_DECIDE_EXPR_TYPE L"Unable to decide the type of expression."

#define WO_ERR_ARGUMENT_TOO_FEW L"Argument count too few to call '%ls'."

#define WO_ERR_ARGUMENT_TOO_MANY L"Argument count too many to call '%ls'."

#define WO_ERR_TYPE_CANNOT_BE_CALL L"Cannot invoke value of '%ls' type."

#define WO_ERR_UNKNOWN_TYPE L"Unknown type '%ls'."

#define WO_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE L"Cannot find overload of '%ls' with type '%ls' ."

#define WO_ERR_NEED_TYPES L"Here need '%ls'."

#define WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC L"Only in function having variadic arguments can use '...' to pack variadic arguments."

#define WO_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL L"Unpack is only allowed for arguments in function calls."

#define WO_ERR_REDEFINED L"Redefined '%ls' in this scope."

#define WO_ERR_IS_NOT_A_TYPE L"'%ls' is not a type."

#define WO_ERR_IS_A_TYPE L"'%ls' is a type."

#define WO_ERR_SYMBOL_IS_AMBIGUOUS L"'%ls' is ambiguous, it was found in namespace: "

#define WO_ERR_UNINDEXABLE_TYPE L"Unindexable type '%ls'."

#define WO_ERR_CANNOT_AS_TYPE L"The type '%ls' is not the same as the requested type '%ls'."

#define WO_ERR_CANNOT_TEST_COMPLEX_TYPE L"Cannot validate type '%ls' at runtime."

#define WO_ERR_CANNOT_AS_DYNAMIC L"'dynamic' here is useless."

#define WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM L"Cannot assign to a non-assignable item."

#define WO_ERR_CANNOT_EXPORT_SYMB_IN_FUNC L"Cannot export the symbol defined in function."

#define WO_ERR_TEMPLATE_ARG_NOT_MATCH L"Not matched template arguments."

#define WO_ERR_TYPE_NEED_N_TEMPLATE_ARG L"Type '%ls' requires '%d' type(s) as template parameters."

#define WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE L"Unable to deduce the return type of function '%ls', it needs to be marked manually."

#define WO_ERR_NO_TEMPLATE_VARIABLE_OR_FUNCTION L"Templated items beside variable or function is invalid here."

#define WO_ERR_NO_MATCHED_FUNC_TEMPLATE L"Template arguments didn't matched for this function."

#define WO_ERR_UNDEFINED_MEMBER L"try index undefined '%ls'."

#define WO_ERR_CANNOT_INDEX_MEMB_WITHOUT_STR L"Typed mapping only indexable form 'string'."

#define WO_ERR_INVALID_OPERATE L"Invalid '%ls'."

#define WO_ERR_FUNC_RETURN_DIFFERENT_TYPES L"Different types of return values in function, '%ls' was inferred or specified earlier, but '%ls' here."

#define WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID L"Recursive format string is invalid."

#define WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE L"Repeated macro: '%ls'."

#define WO_ERR_UNKNOWN_PRAGMA_COMMAND L"Unknown pre-compile command '%ls'."

#define WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR L"Failed macro compiling result: \n"

#define WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR L"An error occurred while running macro '%ls': %ls."

#define WO_ERR_INVALID_TOKEN_MACRO_CONTROLOR L"Macro '%ls' generated an illegal lexical sequence:\n"

#define WO_ERR_HERE_SHOULD_HAVE L"Here should have '%ls'."

#define WO_ERR_UNEXPECT_PATTERN_MODE L"Unexpected pattern mode."

#define WO_ERR_UNEXPECT_PATTERN_CASE L"Unexpected pattern case."

#define WO_ERR_REPEAT_MATCH_CASE L"Repeated cases found in 'match'."

#define WO_ERR_MATCH_CASE_NOT_COMPLETE L"All cases should be walked through in 'match'."

#define WO_ERR_UNKNOWN_MATCHING_VAL_TYPE L"Type inference of the value in 'match' failed."

#define WO_ERR_UNKNOWN_CASE_TYPE L"Illegal 'case', here should be pattern of the type in 'match'."

#define WO_ERR_INVALID_CASE_TYPE_NEED_ACCEPT_ARG L"Invalid 'case',,union pattern not match: need receive a variable."

#define WO_ERR_INVALID_CASE_TYPE_NO_ARG_RECV L"Invalid 'case', union pattern not match: cannot receive any variable."

#define WO_ERR_CANNOT_REACH_PRIVATE_IN_OTHER_FUNC L"Cannot reach '%ls', private target only usable in source: '%ls'."

#define WO_ERR_CANNOT_REACH_PROTECTED_IN_OTHER_FUNC L"Cannot reach '%ls', protected target only usable in same namespace."

#define WO_ERR_CANNOT_CAPTURE_IN_NAMED_FUNC L"Cannot capture '%ls' in named-function."

#define WO_ERR_CANNOT_CAPTURE_TEMPLATE_VAR L"Cannot capture template variable '%ls'."

#define WO_ERR_THERE_IS_NO_MEMBER_NAMED L"Type '%ls' have not member named '%ls'."

#define WO_ERR_CONSTRUCT_STRUCT_NOT_FINISHED L"Not provide initialization for all member when constructing '%ls'."

#define WO_ERR_ONLY_CONSTRUCT_STRUCT_BY_THIS_WAY L"Only struct type can be constructed by 'Type { ... }'."

#define WO_ERR_RELATION_CANNOT_COMPARE L"Relation operator '%ls' can not use for comparing value of type '%ls'."

#define WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"Unknown escape sequences begin with '%lc'."

#define WO_ERR_REPEAT_ATTRIBUTE L"Duplicate attribute description."

#define WO_ERR_TOO_MANY_TEMPLATE_ARGS L"Too many template arguments for type '%ls'."

#define WO_ERR_TOO_FEW_TEMPLATE_ARGS L"Too few template arguments for type '%ls'."

#define WO_ERR_UNMATCHED_PATTERN_TYPE_NOT_DECIDED L"Unmatched pattern: Value type is pending."

#define WO_ERR_UNMATCHED_PATTERN_TYPE_EXPECT_TUPLE L"Unmatched pattern: Here should be a tuple, but get '%ls'"

#define WO_ERR_UNMATCHED_PATTERN_TYPE_TUPLE_DNT_MATCH L"Unmatched pattern: Expecting a tuplt with %d elems, but get tuple with %d elems."

#define WO_ERR_CANNOT_MATCH_SUCH_TYPE L"Cannot check value with type named '%ls' in 'match' expr, expect union-type."

#define WO_ERR_INVALID_ITEM_OF L"'%ls' is not a valid item of '%ls'."

#define WO_ERR_FAILED_TO_INVOKE_FUNC_FOR_TYPE L"Unable to find function named '%ls' for type '%ls'."

#define WO_ERR_FAILED_TO_DECIDE_ALL_TEMPLATE_ARGS L"Some of template params failed to decide."

#define WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_INDEX L"Out of range, the tuple contain %d elmes, but trying index the elems %d."

#define WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_TYPE L"Can only index tuple with constant of 'int' type."

#define WO_ERR_FAILED_TO_DECIDE_TUPLE_TYPE L"The type of elems not decided, failed to decide tuple type."

#define WO_ERR_FAILED_TO_CREATE_TUPLE_WITH_VAARG L"Tuple cannot contain '...'"

#define WO_ERR_DIFFERENT_TYPES_IN_COND_EXPR L"Different types found in conditional expr: '%ls' and '%ls'."

#define WO_ERR_NOT_BOOL_VAL_IN_COND_EXPR L"Judge expr in conditional expr should hava 'bool' type, but get '%ls'"

#define WO_ERR_FAILED_TO_INSTANCE_TEMPLATE_ID L"Failed to instance template identifier '%ls'."

#define WO_ERR_FAILED_TO_INDEX_VAARG_ERR_TYPE L"Can only index 'Variadic argument pack' by value of 'int' type."

#define WO_ERR_FAILED_TO_INVOKE_BECAUSE L"Failed to invoke current function, because: "

#define WO_ERR_DIFFERENT_KEY_TYPE_OF L"Different types of 'key' found in '%ls', failed to decided template arguments for '%ls'."

#define WO_ERR_DIFFERENT_VAL_TYPE_OF L"Different types of 'value' found in '%ls', failed to decided template arguments for '%ls'."

#define WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE L"The type of keys of '%ls' is different from template arguments."

#define WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE L"The type of values of '%ls' is different from template arguments."

#define WO_ERR_DIFFERENT_MEMBER_TYPE_OF L"The type of structure member '%ls' should be '%ls', but found '%ls'."

#define WO_ERR_WHERE_COND_SHOULD_BE_CONST L"Constraints must be a constant."

#define WO_ERR_WHERE_COND_TYPE_ERR L"Constraints should be 'bool', but here is '%ls'"

#define WO_ERR_WHERE_COND_NOT_MEET L"Constraints didn't meet."

#define WO_ERR_INVALID_KEY_EXPR L"Invalid key expr found."

#define WO_ERR_INVALID_GADT_CONFLICT L"Conflict type of union item."

#define WO_ERR_USING_UNSAFE_NAMESPACE L"Using a namespace containing 'unsafe' is not allowed."

#define WO_ERR_NO_AGR_FOR_DEFAULT_PATTERN L"Cannot match argument from default union pattern."

#define WO_ERR_CASE_AFTER_DEFAULT_PATTERN L"Cannot match other cases after default union pattern."

#define WO_ERR_USELESS_DEFAULT_PATTERN L"Useless default union pattern, default case cannot be execute."

#define WO_ERR_INDEX_OUT_OF_RANGE L"Index out of range."

#define WO_ERR_UNKNOWN_MACRO_NAMED L"Unknown macro named: '%ls'."

#define WO_ERR_NOT_ALLOW_IGNORE_VALUE L"Expressions of type non-void are not allowed as statements(here is '%ls'), which causes important values to be accidentally ignored."

#define WO_ERR_UNUSED_VARIABLE_DEFINE L"'%ls' has been declared but never used in function: '%ls'."

#define WO_ERR_DECL_BUILTIN_TYPE_IS_NOT_ALLOWED L"Creation of a new type or type alias with the same name as a built-in type is not allowed."

#define WO_ERR_DECL_TEMPLATE_PATTERN_IS_NOT_ALLOWED L"Only template variables are allowed, but here are other patterns."

#define WO_ERR_UNKNOWN_EXTERN_ATTRIB L"Unknown extern attribute: '%ls'."

#define WO_ERR_CANNOT_DIV_ZERO L"The divisor cannot be 0."


#define WO_INFO_ITEM_IS_DEFINED_HERE L"Compiler found last defination of '%ls'."

#define WO_INFO_INIT_EXPR_IS_HERE L"The compiler found the initialization expression of '%ls'."

#define WO_INFO_IS_THIS_ONE L"Do you means '%ls', which is defined here?"

#define WO_INFO_CANNOT_USE_UNREACHABLE_TYPE L"Cannot use an inaccessible private or protected type when creating an instance or converting a type."

#define WO_INFO_THE_TYPE_IS_ALIAS_OF L"Type '%ls' is an alias of '%ls'."

#endif