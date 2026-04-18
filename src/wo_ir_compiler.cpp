#include "wo_afx.hpp"

#include "wo_ir_compiler.hpp"
#include "wo_compiler_parser.hpp"

namespace wo
{
    IRCompiler::IRCompiler()
    {
        m_ircompiler = woort_IRCompiler_create();

        static_assert(nullptr == NULL);
    }
    IRCompiler::~IRCompiler()
    {
        if (m_ircompiler != nullptr)
            abondon();
    }

    void IRCompiler::abondon()
    {
        wo_assert(m_ircompiler != nullptr);

        m_current_functions_stack.clear();

        woort_IRCompiler_close(m_ircompiler);
        m_ircompiler = nullptr;
    }

    bool IRCompiler::is_abondoned() const
    {
        return m_ircompiler == nullptr;
    }

    woort_IRFunction* IRCompiler::push_function(uint32_t param_count)
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* irfunc;
        if (!woort_IRCompiler_add_function(m_ircompiler, param_count, &irfunc))
        {
            abondon();
            return nullptr;
        }

        m_current_functions_stack.push_back(IRFunction{irfunc, {}});
        return irfunc;
    }

    void IRCompiler::pop_function()
    {
        if (!is_abondoned())
            m_current_functions_stack.pop_back();
    }

    woort_IRConstantIndex IRCompiler::alloc_constant()
    {
        if (is_abondoned())
            return 0;

        return woort_IRCompiler_add_constant(m_ircompiler);
    }

    woort_IRStaticIndex IRCompiler::alloc_static()
    {
        if (is_abondoned())
            return 0;

        return woort_IRCompiler_add_static(m_ircompiler);
    }

    std::optional<woort_CodeEnv*> IRCompiler::commit()
    {
        if (is_abondoned())
            return std::nullopt;

        // All function must be ended.
        wo_assert(m_current_functions_stack.empty());

        woort_CodeEnv* cenv;
        if (!woort_IRCompiler_finish(m_ircompiler, &cenv))
            return std::nullopt;

        m_ircompiler = nullptr;
        return cenv;
    }

    const woort_Bytecode* IRCompiler::get_function(woort_CodeEnv* cenv, woort_IRFunction* irfunc)
    {
        const woort_Bytecode* bytecode;
        if (!woort_CodeEnv_query_function(cenv, irfunc, &bytecode))
            abort();

        return bytecode;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    const woort_IRValue* IRCompiler::load_constant(woort_IRConstantIndex cidx)
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        const woort_IRValue* const result = woort_IRFunction_load_const(cur, cidx);
        if (result == nullptr)
            abondon();

        return result;
    }

    woort_IRValue* IRCompiler::new_value()
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        woort_IRValue* const result = woort_IRFunction_new_vreg(cur);
        if (result == nullptr)
            abondon();

        return result;
    }

    const woort_IRValue* IRCompiler::argument(uint32_t aidx)
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        woort_IRValue* const result = woort_IRFunction_get_argument(cur, aidx);
        if (result == nullptr)
            abondon();

        return result;
    }

    woort_IRLabel* IRCompiler::new_label()
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        woort_IRLabel* const result = woort_IRFunction_new_label(cur);
        if (result == nullptr)
            abondon();

        return result;
    }

    woort_IRLabel* IRCompiler::named_label(ast::AstBase* ast_node, const char* label_name)
    {
        if (is_abondoned())
            return nullptr;

        wo_assert(!m_current_functions_stack.empty());

        IRFunction& current = m_current_functions_stack.back();
        NamedLabelInAst key{ast_node, label_name};

        auto it = current.m_local_named_labels.find(key);
        if (it != current.m_local_named_labels.end())
            return it->second;

        woort_IRLabel* label = woort_IRFunction_new_label(current.m_irfunction);
        if (label == nullptr)
        {
            abondon();
            return nullptr;
        }

        current.m_local_named_labels.emplace(key, label);
        return label;
    }

    void IRCompiler::mov(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MOV(cur, dst, src))
            abondon();
    }

    void IRCompiler::load(woort_IRValue* dst, woort_IRStaticIndex src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LOAD(cur, dst, src))
            abondon();
    }

    void IRCompiler::store(woort_IRStaticIndex dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STORE(cur, dst, src))
            abondon();
    }

    void IRCompiler::pushchk(const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHCHK(cur, src))
            abondon();
    }

    void IRCompiler::pushstaticchk(woort_IRStaticIndex static_src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHSTATICCHK(cur, static_src))
            abondon();
    }

    void IRCompiler::popr(uint32_t count)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_POPR(cur, count))
            abondon();
    }

    void IRCompiler::poprs(const woort_IRValue* count_src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_POPRS(cur, count_src))
            abondon();
    }

    void IRCompiler::itor(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ITOR(cur, dst, src))
            abondon();
    }

    void IRCompiler::itos(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ITOS(cur, dst, src))
            abondon();
    }

    void IRCompiler::rtoi(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_RTOI(cur, dst, src))
            abondon();
    }

    void IRCompiler::rtos(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_RTOS(cur, dst, src))
            abondon();
    }

    void IRCompiler::stoi(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STOI(cur, dst, src))
            abondon();
    }

    void IRCompiler::stor(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STOR(cur, dst, src))
            abondon();
    }

    void IRCompiler::callnwo(woort_IRConstantIndex target, uint32_t argc, woort_IRValue* dst)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CALLNWO(cur, target, argc, dst))
            abondon();
    }

    void IRCompiler::callnfp(woort_IRConstantIndex target, uint32_t argc, woort_IRValue* dst)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CALLNFP(cur, target, argc, dst))
            abondon();
    }

    void IRCompiler::callnjit(woort_IRConstantIndex target, uint32_t argc, woort_IRValue* dst)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CALLNJIT(cur, target, argc, dst))
            abondon();
    }

    void IRCompiler::call(const woort_IRValue* func_val, uint32_t argc, woort_IRValue* dst)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CALL(cur, func_val, argc, dst))
            abondon();
    }

    void IRCompiler::mkclosure(woort_IRValue* dst, uint32_t elem_count, woort_IRConstantIndex func_idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MKCLOSURE(cur, dst, elem_count, func_idx))
            abondon();
    }

    void IRCompiler::mkvec(woort_IRValue* dst, uint32_t elem_count)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MKVEC(cur, dst, elem_count))
            abondon();
    }

    void IRCompiler::mkmap(woort_IRValue* dst, uint32_t kvpair_count)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MKMAP(cur, dst, kvpair_count))
            abondon();
    }

    void IRCompiler::mkstruct(woort_IRValue* dst, uint32_t elem_count)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MKSTRUCT(cur, dst, elem_count))
            abondon();
    }

    void IRCompiler::boxdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_BOXDYN(cur, dst, typ, src))
            abondon();
    }

    void IRCompiler::unboxdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNBOXDYN(cur, dst, typ, src))
            abondon();
    }

    void IRCompiler::checkdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CHECKDYN(cur, dst, typ, src))
            abondon();
    }

    void IRCompiler::pushboxdyn(uint8_t typ, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHBOXDYN(cur, typ, src))
            abondon();
    }

    void IRCompiler::addi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ADDI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::subi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_SUBI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::muli(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MULI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::divi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_DIVI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::modi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MODI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::negi(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_NEGI(cur, dst, src))
            abondon();
    }

    void IRCompiler::lti(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LTI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::gti(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_GTI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::lei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LEI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::gei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_GEI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::eqi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_EQI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::nei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_NEI(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::addr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ADDR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::subr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_SUBR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::mulr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MULR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::divr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_DIVR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::modr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MODR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::negr(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_NEGR(cur, dst, src))
            abondon();
    }

    void IRCompiler::ltr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LTR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::gtr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_GTR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::ler(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LER(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::ger(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_GER(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::eqr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_EQR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::ner(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_NER(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::adds(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ADDS(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::lts(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LTS(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::gts(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_GTS(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::les(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LES(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::ges(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_GES(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::eqs(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_EQS(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::nes(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_NES(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::land(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LAND(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::lor(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LOR(cur, dst, a, b))
            abondon();
    }

    void IRCompiler::lnot(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LNOT(cur, dst, src))
            abondon();
    }

    void IRCompiler::ldidxvec(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXVEC(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidxvecx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXVECX(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidxstruct(woort_IRValue* dst, const woort_IRValue* container, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXSTRUCT(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidxstring(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXSTRING(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidxdicti(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXDICTI(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidxdictr(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXDICTR(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidxdictb(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXDICTB(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidxdictx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDXDICTX(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::stidxveci(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXVECI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxvecr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXVECR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxvecb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXVECB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxvecx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXVECX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictii(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTII(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictir(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTIR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictib(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTIB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictix(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTIX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictri(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTRI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictrr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTRR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictrb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTRB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictrx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTRX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictbi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTBI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictbr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTBR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictbb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTBB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictbx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTBX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictxi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTXI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictxr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTXR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictxb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTXB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxdictxx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXDICTXX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapii(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPII(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapir(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPIR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapib(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPIB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapix(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPIX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapri(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPRI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmaprr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPRR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmaprb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPRB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmaprx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPRX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapbi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPBI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapbr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPBR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapbb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPBB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapbx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPBX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapxi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPXI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapxr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPXR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapxb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPXB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxmapxx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXMAPXX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidxstruct(woort_IRValue* c, uint32_t idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDXSTRUCT(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::unpackstruct(const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNPACKSTRUCT(cur, src))
            abondon();
    }

    void IRCompiler::unpackvec(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNPACKVEC(cur, dst, src))
            abondon();
    }

    void IRCompiler::unpackvecx(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNPACKVECX(cur, dst, src))
            abondon();
    }

    void IRCompiler::pushidxstruct(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDXSTRUCT(cur, src, idx))
            abondon();
    }

    void IRCompiler::pushidxstboxi(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDXSTBOXI(cur, src, idx))
            abondon();
    }

    void IRCompiler::pushidxstboxr(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDXSTBOXR(cur, src, idx))
            abondon();
    }

    void IRCompiler::pushidxstboxb(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDXSTBOXB(cur, src, idx))
            abondon();
    }

    void IRCompiler::pushidxstboxx(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDXSTBOXX(cur, src, idx))
            abondon();
    }

    void IRCompiler::astore(woort_IRStaticIndex idx, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ASTORE(cur, idx, src))
            abondon();
    }

    void IRCompiler::aload(woort_IRValue* dst, woort_IRStaticIndex idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ALOAD(cur, dst, idx))
            abondon();
    }

    void IRCompiler::cas(woort_IRStaticIndex idx, woort_IRValue* expected, const woort_IRValue* desired)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CAS(cur, idx, expected, desired))
            abondon();
    }

    void IRCompiler::bind(woort_IRLabel* label)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_bind(cur, label))
            abondon();
    }

    void IRCompiler::jmp(woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jmp(cur, target))
            abondon();
    }

    void IRCompiler::jifinited(woort_IRStaticIndex cond_idx, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jifinited(cur, cond_idx, target))
            abondon();
    }

    void IRCompiler::jcc(const woort_IRValue* cond, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jcc(cur, cond, target))
            abondon();
    }

    void IRCompiler::jccz(const woort_IRValue* cond, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jccz(cur, cond, target))
            abondon();
    }

    void IRCompiler::jcc_lt(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jcc_lt(cur, a, b, target))
            abondon();
    }

    void IRCompiler::jcc_le(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jcc_le(cur, a, b, target))
            abondon();
    }

    void IRCompiler::jcc_eq(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jcc_eq(cur, a, b, target))
            abondon();
    }

    void IRCompiler::jcc_gt(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jcc_gt(cur, a, b, target))
            abondon();
    }

    void IRCompiler::jcc_ge(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jcc_ge(cur, a, b, target))
            abondon();
    }

    void IRCompiler::jcc_ne(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_jcc_ne(cur, a, b, target))
            abondon();
    }

    void IRCompiler::ret(const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ret(cur, val))
            abondon();
    }

    void IRCompiler::ret_void()
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ret_void(cur))
            abondon();
    }

    void IRCompiler::nop()
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_NOP(cur))
            abondon();
    }

    void IRCompiler::debugtrap()
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_debugtrap(cur))
            abondon();
    }

    void IRCompiler::panic(const woort_IRValue* msg)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_panic(cur, msg))
            abondon();
    }

    void IRCompiler::push_srcloc(const ast::AstBase* node)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IRFunction_push_srcloc(
            cur, 
            node->source_location.source_file->c_str(), 
            node->source_location.begin_at.row,
            node->source_location.begin_at.column,
            node->source_location.end_at.row,
            node->source_location.end_at.column))
            abondon();
    }

    void IRCompiler::pop_srcloc()
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        woort_IRFunction_pop_srcloc(cur);
    }

    //

    woort_IRConstantIndex IRCompiler::imm_int(woort_Int val)
    {
        if (is_abondoned())
            return 0;

        auto it = m_nil_bool_int_handle_imm_pool.find(val);
        if (it == m_nil_bool_int_handle_imm_pool.end())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            m_nil_bool_int_handle_imm_pool.emplace(val, cidx);
            return cidx;
        }
        return it->second;
    }
    woort_IRConstantIndex IRCompiler::imm_box_int(woort_Int val)
    {
        if (is_abondoned())
            return 0;

        auto it = m_boxed_int_imm_pool.find(val);
        if (it == m_boxed_int_imm_pool.end())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            m_boxed_int_imm_pool.emplace(val, cidx);
            return cidx;
        }
        return it->second;
    }
    woort_IRConstantIndex IRCompiler::imm_real(woort_Real val)
    {
        if (is_abondoned())
            return 0;

        auto it = m_real_imm_pool.find(val);
        if (it == m_real_imm_pool.end())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            m_real_imm_pool.emplace(val, cidx);
            return cidx;
        }
        return it->second;
    }
    woort_IRConstantIndex IRCompiler::imm_box_real(woort_Real val)
    {
        if (is_abondoned())
            return 0;

        auto it = m_boxed_real_imm_pool.find(val);
        if (it == m_boxed_real_imm_pool.end())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            m_boxed_real_imm_pool.emplace(val, cidx);
            return cidx;
        }
        return it->second;
    }
    woort_IRConstantIndex IRCompiler::imm_box_bool(bool val)
    {
        if (is_abondoned())
            return 0;

        std::optional<woort_IRConstantIndex>& slot = val ? m_boxed_true_imm : m_boxed_false_imm;
        if (!slot.has_value())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            slot = cidx;
            return cidx;
        }
        return slot.value();
    }
    woort_IRConstantIndex IRCompiler::imm_string(wo_pstring_t val)
    {
        if (is_abondoned())
            return 0;

        auto it = m_string_imm_pool.find(val);
        if (it == m_string_imm_pool.end())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            m_string_imm_pool.emplace(val, cidx);
            return cidx;
        }
        return it->second;
    }
    woort_IRConstantIndex IRCompiler::imm_closure(ast::AstValueFunction* val)
    {
        if (is_abondoned())
            return 0;

        auto it = m_closure_imm_pool.find(val);
        if (it == m_closure_imm_pool.end())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            m_closure_imm_pool.emplace(val, cidx);
            return cidx;
        }
        return it->second;
    }
    woort_IRConstantIndex IRCompiler::imm_function(ast::AstValueFunction* val)
    {
        if (is_abondoned())
            return 0;

        auto it = m_function_imm_pool.find(val);
        if (it == m_function_imm_pool.end())
        {
            woort_IRConstantIndex cidx = alloc_constant();
            m_function_imm_pool.emplace(val, cidx);
            return cidx;
        }
        return it->second;
    }
    woort_IRConstantIndex IRCompiler::imm_nil()
    {
        return imm_int(0);
    }
    woort_IRConstantIndex IRCompiler::imm_bool(bool val)
    {
        return imm_int(val ? 1 : 0);
    }
    woort_IRConstantIndex IRCompiler::imm_handle(woort_Handle handle)
    {
        return imm_int(static_cast<woort_Int>(handle));
    }
    woort_IRConstantIndex IRCompiler::imm_box_handle(woort_Handle handle)
    {
        return imm_box_int(static_cast<woort_Int>(handle));
    }

    const woort_IRValue* IRCompiler::load_imm_int(woort_Int val)
    {
        return load_constant(imm_int(val));
    }
    const woort_IRValue* IRCompiler::load_imm_box_int(woort_Int val)
    {
        return load_constant(imm_box_int(val));
    }
    const woort_IRValue* IRCompiler::load_imm_real(woort_Real val)
    {
        return load_constant(imm_real(val));
    }
    const woort_IRValue* IRCompiler::load_imm_box_real(woort_Real val)
    {
        return load_constant(imm_box_real(val));
    }
    const woort_IRValue* IRCompiler::load_imm_box_bool(bool val)
    {
        return load_constant(imm_box_bool(val));
    }
    const woort_IRValue* IRCompiler::load_imm_string(wo_pstring_t val)
    {
        return load_constant(imm_string(val));
    }
    const woort_IRValue* IRCompiler::load_imm_closure(ast::AstValueFunction* val)
    {
        return load_constant(imm_closure(val));
    }
    const woort_IRValue* IRCompiler::load_imm_function(ast::AstValueFunction* val)
    {
        return load_constant(imm_function(val));
    }
    const woort_IRValue* IRCompiler::load_imm_nil()
    {
        return load_constant(imm_nil());
    }
    const woort_IRValue* IRCompiler::load_imm_bool(bool val)
    {
        return load_constant(imm_bool(val));
    }
    const woort_IRValue* IRCompiler::load_imm_handle(woort_Handle handle)
    {
        return load_constant(imm_handle(handle));
    }
    const woort_IRValue* IRCompiler::load_imm_box_handle(woort_Handle handle)
    {
        return load_constant(imm_box_handle(handle));
    }

    woort_IRConstantIndex IRCompiler::imm_const(
        const ast::ConstantValue& constant, bool boxed)
    {
        if (is_abondoned())
            return 0;

        switch (constant.m_type)
        {
        case ast::ConstantValue::Type::NIL:
            return imm_nil();
        case ast::ConstantValue::Type::BOOL:
            if (boxed)
                return imm_box_bool(constant.value_bool());
            return imm_bool(constant.value_bool());
        case ast::ConstantValue::Type::INTEGER:
            if (boxed)
                return imm_box_int(constant.value_integer());
            return imm_int(constant.value_integer());
        case ast::ConstantValue::Type::HANDLE:
            if (boxed)
                return imm_box_handle(constant.value_handle());
            return imm_handle(constant.value_handle());
        case ast::ConstantValue::Type::REAL:
            if (boxed)
                return imm_box_real(constant.value_real());
            return imm_real(constant.value_real());
        case ast::ConstantValue::Type::PSTRING:
            return imm_string(constant.value_pstring());
        case ast::ConstantValue::Type::FUNCTION:
            return imm_closure(constant.value_function());
        case ast::ConstantValue::Type::STRUCT:
        {
            auto& struct_data = constant.value_struct();
            auto it = m_tuple_imm_pool.find(struct_data);
            if (it == m_tuple_imm_pool.end())
            {
                TupleImm t;
                t.m_fields.reserve(struct_data.m_count);
                for (size_t i = 0; i < struct_data.m_count; ++i)
                {
                    t.m_fields.push_back(
                        imm_const(struct_data.m_elements[i], false));
                }
                t.m_idx = alloc_constant();
                woort_IRConstantIndex result_idx = t.m_idx;
                m_tuple_imm_pool.emplace(struct_data, std::move(t));
                return result_idx;
            }
            return it->second.m_idx;
        }
        default:
            wo_unreachable("Unknown ConstantValue type");
            return 0;
        }
    }
    const woort_IRValue* IRCompiler::load_imm_const(
        const ast::ConstantValue& constant, bool boxed)
    {
        return load_constant(imm_const(constant, boxed));
    }
}
