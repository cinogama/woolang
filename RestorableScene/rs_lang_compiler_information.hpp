#pragma once
// This file using to store compiler/lexer's error informations.
// Here will be 

#define RS_LANG_EN 0
#define RS_LANG_ZH_CN 1

#define RS_USED_LANGUAGE RS_LANG_ZH_CN

#if RS_USED_LANGUAGE == RS_LANG_ZH_CN

#define RS_TERM_GLOBAL_NAMESPACE L"ȫ��������"

#define RS_TERM_OR L"��"

#define RS_TERM_AT L"λ��"

#define RS_TERM_AND L"��"

#define RS_TERM_EXCEPTED L"Ӧ����"


#define RS_ERR_MISMATCH_ANNO_SYM L"��ƥ���ע�ͷ�"

#define RS_ERR_UNEXCEPT_CH_AFTER_CH L"δԤ�ϵķ��� '%c' λ�� '%c' ֮��"

#define RS_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH L"δԤ�ϵķ��� '%c' λ�� '%c' ֮��, �˴�Ӧ���� '%c'"

#define RS_ERR_UNEXCEPT_EOF L"δԤ�ϵ��ļ�β"

#define RS_ERR_UNEXCEPT_TOKEN L"δԤ�ϵķ���: "

#define RS_ERR_ILLEGAL_LITERAL L"�Ƿ������泣��"

#define RS_ERR_UNKNOW_OPERATOR_STR L"δ֪�����: '%s'"

#define RS_ERR_UNEXCEPTED_EOL_IN_STRING L"���ַ��������з��ֻ��з�"

#define RS_ERR_LEXER_ERR_UNKNOW_NUM_BASE L"�ʷ�����δ֪�ĳ�������"

#define RS_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L"�ʷ�����, δ֪�ķ���: '%c'"

#define RS_ERR_UNEXCEPT_AST_NODE_TYPE L"δԤ�ϵ����﷨���ڵ�����: Ӧ�����﷨���ڵ���ʶ��"

#define RS_ERR_SHOULD_BE_AST_BASE L"�﷨����ʱ����δ֪����: δ֪�ڵ�����"

#define RS_ERR_UNABLE_RECOVER_FROM_ERR L"�޷��ӵ�ǰ�����лָ���������ֹ"

#define RS_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME L"������ͬʱ����Ϊ 'public', 'protected' ���� 'private'"

#define RS_ERR_TYPE_CANNOT_NEGATIVE L"'%s' ���ܱ�ȡ��ֵ"

#define RS_ERR_CANNOT_OPEN_FILE L"�޷����ļ�: '%s'"

#define RS_ERR_UNPACK_ARG_LESS_THEN_ONE L"չ��������ʱ������Ҫչ��һ������"

#define RS_ERR_CANNOT_FIND_EXT_SYM L"�޷��ҵ��ⲿ����: '%s'"

#define RS_ERR_ARG_DEFINE_AFTER_VARIADIC L"�� '...' ֮��Ӧ������������"

#define RS_ERR_CANNOT_CALC_STR_WITH_THIS_OP L"��֧�ֶ��ַ������и�����"

#define RS_ERR_CANNOT_CALC_HANDLE_WITH_THIS_OP L"��֧�ֶԾ�����ͽ��и�����"

#define RS_ERR_CANNOT_ASSIGN_TO_CONSTANT L"������������ֵ"

#define RS_ERR_CANNOT_CALC_WITH_L_AND_R L"������������ߵ�ֵ���Ͳ�ƥ�䣬�޷�����"

#define RS_ERR_CANNOT_INDEX_STR_WITH_TYPE L"����ʹ�� '%s' ���͵�ֵ�����ַ���"

#define RS_ERR_CANNOT_CAST_TYPE_TO_TYPE  L"�޷��� '%s' ���͵�ֵת��Ϊ '%s'"

#define RS_ERR_CANNOT_IMPLCAST_TYPE_TO_TYPE L"�޷��� '%s' ���͵�ֵ��ʽת��Ϊ '%s'"

#define RS_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE L"���ܽ� '%s' ���͵�ֵ��ֵ�� '%s' ���͵ı���"

#define RS_ERR_CANNOT_DO_RET_OUSIDE_FUNC L"�Ƿ��ķ��ز���, �������ں�����Χ����з���"

#define RS_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME L"����ͬʱ���� '%s' �� '%s'"

#define RS_ERR_ERR_PLACE_FOR_USING_NAMESPACE L"ʹ�������ռ��������Ӧλ�ڵ�ǰ����Ŀ�ͷ"

#define RS_ERR_UNKNOWN_IDENTIFIER L"δ����ı�ʶ�� '%s'"

#define RS_ERR_UNABLE_DECIDE_VAR_TYPE L"�޷����ϱ�������"

#define RS_ERR_UNABLE_DECIDE_FUNC_OVERRIDE L"�޷�ȷ��Ҫʹ�ú�������һ������, ������: %s"

#define RS_ERR_UNABLE_DECIDE_FUNC_SYMBOL L"�޷�ȷ��Ҫʹ�õĺ���"

#define RS_ERR_NO_MATCH_FUNC_OVERRIDE L"û���ҵ�ƥ������ĺ�������"

#define RS_ERR_ARGUMENT_TOO_FEW L"�޷����� '%s': ��������"

#define RS_ERR_ARGUMENT_TOO_MANY L"�޷����� '%s': ��������"

#define RS_ERR_TYPE_CANNOT_BE_CALL L"�޷����� '%s'."

#define RS_ERR_UNKNOWN_TYPE L"δ֪���� '%s'."

#define RS_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE L"�޷��ҵ����� '%s' ����Ϊ '%s' ������"

#define RS_ERR_NEED_TYPES L"����Ӧ���� '%s'."

#define RS_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC L"ֻ��ӵ�б䳤�����ĺ����п���ʹ�� '...' ������䳤����."

#define RS_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL L"��������ֻ�����ں�������"

#define RS_ERR_REDEFINED L"�ڵ�ǰ���������ظ������� '%s'"

#define RS_ERR_IS_NOT_A_TYPE L"��ʶ�� '%s' ����һ������"

#define RS_ERR_IS_A_TYPE L"��ʶ�� '%s' ��һ������"

#define RS_ERR_SYMBOL_IS_AMBIGUOUS L"��ʶ�� '%s' ����ȷ, �������������ռ��б��ҵ�: "

#define RS_ERR_CANNOT_IMPLCAST_REF L"������ʽת�����ô��ݶ��������"

#define RS_ERR_NOT_REFABLE_INIT_ITEM L"�������õĶ�������Ϊ 'ref' �ĳ�ʼ��"

#define RS_ERR_UNINDEXABLE_TYPE L"�������������� '%s'"

#define RS_ERR_CANNOT_AS_TYPE L"���� '%s' ��Ҫ������� '%s' ����ͬ"

#define RS_ERR_CANNOT_AS_COMPLEX_TYPE L"'as' ������������ʱ���Ը�������"

#define RS_ERR_CANNOT_AS_DYNAMIC L"'as dynamic' ����Ч��"


#define RS_WARN_UNKNOW_ESCSEQ_BEGIN_WITH_CH L"�� '%c' ��ͷ��δ֪ת������."

#define RS_WARN_REPEAT_ATTRIBUTE L"�ظ����ֵ�����"

#define RS_WARN_FUNC_WILL_RETURN_DYNAMIC L"�����з��ص����Ͳ�����, �����������ͽ������Ϊ 'dynamic'"

#define RS_WARN_OVERRIDDEN_DYNAMIC_TYPE L"���� 'dynamic' ������"

#define RS_WARN_CAST_REF L"���ڳ���ת�����ô��ݶ�������ͣ�'ref' ��ʧЧ"

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