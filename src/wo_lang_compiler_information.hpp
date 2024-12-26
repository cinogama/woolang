#pragma once
// This file using to store compiler/lexer's error informations.
// Here will be 

#define WO_LANG_EN 0
#define WO_LANG_ZH_CN 1

#define WO_MAX_ERROR_COUNT 100

#ifndef WO_USED_LANGUAGE
#   define WO_USED_LANGUAGE WO_LANG_ZH_CN
#endif


#define WO_TERM_EXCEPTED L"应当是"

#define WO_MACRO_CODE_END_HERE L"此宏定义在此处结束："

#define WO_MACRO_ANALYZE_END_HERE L"此宏解析在此处结束："

#define WO_TOO_MANY_ERROR(count) L"报告的错误数量太多，仅显示 " + std::to_wstring(count) +  L" 条，终止"


#define WO_ERR_MISMATCH_ANNO_SYM L"不匹配的注释符"
#define WO_ERR_UNEXCEPT_CH_AFTER_CH L"未预料的字符 '%lc' 位于 '%lc' 之后"
#define WO_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"未预料的字符 '%lc' 位于 '%lc' 之后, 此处应该是 '%lc'"
#define WO_ERR_UNEXCEPT_EOF L"未预料的文件尾"
#define WO_ERR_UNEXCEPT_TOKEN L"未预料的: "
#define WO_ERR_ILLEGAL_LITERAL L"非法的字面常量"
#define WO_ERR_UNKNOW_OPERATOR_STR L"未知运算符: '%ls'"
#define WO_ERR_UNEXCEPTED_EOL_IN_CHAR L"在字符常量中发现换行符"
#define WO_ERR_UNEXCEPTED_EOL_IN_STRING L"在字符串常量中发现换行符"
#define WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE L"重复的宏定义 '%ls'"
#define WO_ERR_UNKNOWN_PRAGMA_COMMAND L"未知的预处理指令 '%ls'"
#define WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR L"宏控制器编译失败：\n"
#define WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR L"宏 '%ls' 运行发生了错误：%ls"
#define WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"以 '%lc' 开头的未知转义序列."
#define WO_ERR_INVALID_TOKEN_MACRO_CONTROLOR L"宏 '%ls' 生成了非法的词法序列：\n"
#define WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID L"嵌套格式化字符串是不被允许的"
#define WO_ERR_HERE_SHOULD_HAVE L"缺少 '%ls'"
#define WO_ERR_LEXER_ERR_UNKNOW_NUM_BASE L"词法错误，未知的常量基数"
#define WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L"未知的字符: '%lc'"
#define WO_ERR_UNEXCEPT_AST_NODE_TYPE L"未预料到的语法树节点类型: 应当是语法树节点或标识符"
#define WO_ERR_SHOULD_BE_AST_BASE L"语法分析时发生未知错误: 未知节点类型"
#define WO_ERR_UNABLE_RECOVER_FROM_ERR L"无法从当前错误中恢复，编译终止"

#define WO_ERR_CANNOT_OPEN_FILE L"无法打开文件: '%ls'"
#define WO_ERR_ARG_DEFINE_AFTER_VARIADIC L"在 '...' 之后不应该有其他参数"
#define WO_ERR_UNKNOWN_TYPE L"未知或不完整的类型"
#define WO_ERR_REDEFINED L"在当前作用域中重复定义了 '%ls'"
#define WO_ERR_REPEAT_MEMBER_NAME L"重复的成员：'%ls'"
#define WO_ERR_REPEAT_ATTRIBUTE L"重复出现的属性"
#define WO_ERR_FAILED_TO_CREATE_TUPLE_WITH_VAARG L"元组类型中不允许出现 '...'"
#define WO_ERR_INVALID_KEY_EXPR L"创建映射时遇到了非法的键表达式"
#define WO_ERR_UNKNOWN_MACRO_NAMED L"未定义的宏：'%ls'"
#define WO_ERR_UNKNOWN_EXTERN_ATTRIB L"未知的外部函数说明符: '%ls'"
#define WO_ERR_COMPILER_DISABLED L"编译器功能被禁用"
#define WO_ERR_CANNOT_START_NAMESPACE L"此处不允许定义命名空间"

#define WO_ERR_UNKNOWN_IDENTIFIER L"未定义的标识符：'%ls'"
#define WO_ERR_UNEXPECTED_TYPE_SYMBOL L"'%ls' 是一个类型"
#define WO_ERR_UNEXPECTED_VAR_SYMBOL L"'%ls' 不是类型"
#define WO_ERR_UNEXPECTED_TEMPLATE_COUNT L"目标期待有且仅有 '%zu' 个泛型参数"
#define WO_ERR_EXPECTED_TEMPLATE_ARGUMENT L"这是一个泛型目标"
#define WO_ERR_UNEXPECTED_TEMPLATE_ARGUMENT L"这不是一个泛型目标"
#define WO_ERR_UNEXPECTED_MATCH_TYPE_FOR_TUPLE L"此处匹配期待一个元组"
#define WO_ERR_UNEXPECTED_MATCH_COUNT_FOR_TUPLE L"此处匹配期待一个包含 '%zu' 个元素的元组，但给定的元组包含 '%zu' 个元素"
#define WO_ERR_VALUE_TYPE_DETERMINED_FAILED L"变量的类型未获确定"
#define WO_ERR_TYPE_DETERMINED_FAILED L"类型未获确定"
#define WO_ERR_TYPE_NAMED_DETERMINED_FAILED L"类型 '%ls' 暂未决，此处无法进行类型检查"
#define WO_ERR_RECURSIVE_TEMPLATE_INSTANCE L"类型推断递归地依赖了正在具体化的泛型实例"
#define WO_ERR_RECURSIVE_EVAL_PASS1 L"递归地依赖了自身"
#define WO_ERR_CONSTRAINT_SHOULD_BE_CONST L"约束项应当是一个常量表达式"
#define WO_ERR_CONSTRAINT_SHOULD_BE_BOOL L"约束项的值类型应该是 'bool'"
#define WO_ERR_CONSTRAINT_FAILED L"约束项不满足"

#define WO_ERR_UNMATCHED_RETURN_TYPE_NAMED L"函数的实际返回类型（'%ls'）与标注或推导的类型（'%ls'）不匹配"
#define WO_ERR_UNMATCHED_ARRAY_ELEMENT_TYPE_NAMED L"元素的类型（'%ls'）与先前推导的（'%ls'）不匹配"
#define WO_ERR_UNMATCHED_DICT_KEY_TYPE_NAMED L"键元素的类型（'%ls'）与先前推导的（'%ls'）不匹配"
#define WO_ERR_UNMATCHED_DICT_VALUE_TYPE_NAMED L"值元素的类型（'%ls'）与先前推导的（'%ls'）不匹配"
#define WO_ERR_UNINDEXABLE_TYPE_NAMED L"不能作为容器的类型（'%ls'）"
#define WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE L"容器类型（'%ls'）不接受索引类型（'%ls'）"
#define WO_ERR_CANNOT_INDEX_STRUCT_WITH_NON_CONST L"结构体类型不接受非常量索引"