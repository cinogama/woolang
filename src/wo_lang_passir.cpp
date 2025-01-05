#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    void LangContext::init_passir()
    {
        // WO_LANG_REGISTER_PROCESSER(AstList, AstBase::AST_LIST, passir);
        // WO_LANG_REGISTER_PROCESSER(AstIdentifier, AstBase::AST_IDENTIFIER, passir);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldDefine, AstBase::AST_STRUCT_FIELD_DEFINE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstTypeHolder, AstBase::AST_TYPE_HOLDER, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueMarkAsMutable, AstBase::AST_VALUE_MARK_AS_MUTABLE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueMarkAsImmutable, AstBase::AST_VALUE_MARK_AS_IMMUTABLE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAs, AstBase::AST_VALUE_TYPE_CHECK_AS, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstWhereConstraints, AstBase::AST_WHERE_CONSTRAINTS, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextA,
        //    AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_A, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextB,
        //    AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_B, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall, AstBase::AST_VALUE_FUNCTION_CALL, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueBinaryOperator, AstBase::AST_VALUE_BINARY_OPERATOR, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueUnaryOperator, AstBase::AST_VALUE_UNARY_OPERATOR, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueTribleOperator, AstBase::AST_VALUE_TRIBLE_OPERATOR, passir);
        // WO_LANG_REGISTER_PROCESSER(AstFakeValueUnpack, AstBase::AST_FAKE_VALUE_UNPACK, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueVariadicArgumentsPack, AstBase::AST_VALUE_VARIADIC_ARGUMENTS_PACK, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueIndex, AstBase::AST_VALUE_INDEX, passir);
        // WO_LANG_REGISTER_PROCESSER(AstPatternVariable, AstBase::AST_PATTERN_VARIABLE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstPatternIndex, AstBase::AST_PATTERN_INDEX, passir);
        // WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, passir);
        // WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, passir);
        // WO_LANG_REGISTER_PROCESSER(AstFunctionParameterDeclare, AstBase::AST_FUNCTION_PARAMETER_DECLARE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunction, AstBase::AST_VALUE_FUNCTION, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueArrayOrVec, AstBase::AST_VALUE_ARRAY_OR_VEC, passir);
        // WO_LANG_REGISTER_PROCESSER(AstKeyValuePair, AstBase::AST_KEY_VALUE_PAIR, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueDictOrMap, AstBase::AST_VALUE_DICT_OR_MAP, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueTuple, AstBase::AST_VALUE_TUPLE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldValuePair, AstBase::AST_STRUCT_FIELD_VALUE_PAIR, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueStruct, AstBase::AST_VALUE_STRUCT, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueAssign, AstBase::AST_VALUE_ASSIGN, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValuePackedArgs, AstBase::AST_VALUE_PACKED_ARGS, passir);
        // WO_LANG_REGISTER_PROCESSER(AstNamespace, AstBase::AST_NAMESPACE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstScope, AstBase::AST_SCOPE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstMatchCase, AstBase::AST_MATCH_CASE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstMatch, AstBase::AST_MATCH, passir);
        // WO_LANG_REGISTER_PROCESSER(AstIf, AstBase::AST_IF, passir);
        // WO_LANG_REGISTER_PROCESSER(AstWhile, AstBase::AST_WHILE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstFor, AstBase::AST_FOR, passir);
        // WO_LANG_REGISTER_PROCESSER(AstForeach, AstBase::AST_FOREACH, passir);
        // WO_LANG_REGISTER_PROCESSER(AstBreak, AstBase::AST_BREAK, passir);
        // WO_LANG_REGISTER_PROCESSER(AstContinue, AstBase::AST_CONTINUE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstReturn, AstBase::AST_RETURN, passir);
        // WO_LANG_REGISTER_PROCESSER(AstLabeled, AstBase::AST_LABELED, passir);
        // WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstValueMakeUnion, AstBase::AST_VALUE_MAKE_UNION, passir);
        // WO_LANG_REGISTER_PROCESSER(AstUsingNamespace, AstBase::AST_USING_NAMESPACE, passir);
        // WO_LANG_REGISTER_PROCESSER(AstExternInformation, AstBase::AST_EXTERN_INFORMATION, passir);
    }

    LangContext::pass_behavior LangContext::pass_final_process_bytecode_generation(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        return m_passir_processers->process_node(this, lex, node_state, out_stack);
    }

#endif
}