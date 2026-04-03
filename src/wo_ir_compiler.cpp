#include "wo_afx.hpp"

#include "wo_ir_compiler.hpp"

namespace wo
{
    IRFunction::IRFunction(IRCompiler* c, woort_IRFunction* irfunc)
        : m_ircompiler(c)
        , m_irfunction(irfunc)
    {
    }

    IRCompiler::IRCompiler()
    {
        m_ircompiler = woort_IRCompiler_create();

        // Assure nullptr same as NULL.
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

        woort_IRCompiler_close(m_ircompiler);
        m_ircompiler = nullptr;
    }

    bool IRCompiler::is_abondoned() const
    {
        return m_ircompiler == nullptr;
    }

    IRFunction IRCompiler::add_function(uint32_t param_count)
    {
        if (is_abondoned())
            // IR 系列接口的设计原则是：压制内存申请失败，如同一切正常——直到最终提交代码时
            // 以失败结束
            return IRFunction(this, nullptr);

        woort_IRFunction* irfunc;
        if (!woort_IRCompiler_add_function(m_ircompiler, param_count, &irfunc))
        {
            abondon();
            return IRFunction(this, nullptr);
        }
        return IRFunction(this, irfunc);
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

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    const woort_IRValue* IRFunction::load_constant(woort_IRConstantIndex cidx)
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        const woort_IRValue* const result = woort_IRFunction_load_const(m_irfunction, cidx);
        if (result == nullptr)
            m_ircompiler->abondon();

        return result;
    }

    woort_IRValue* IRFunction::new_value()
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        woort_IRValue* const result = woort_IRFunction_new_vreg(m_irfunction);
        if (result == nullptr)
            m_ircompiler->abondon();

        return result;
    }

    const woort_IRValue* IRFunction::argument(uint32_t aidx)
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        woort_IRValue* const result = woort_IRFunction_get_argument(m_irfunction, aidx);
        if (result == nullptr)
            m_ircompiler->abondon();

        return result;
    }

    woort_IRLabel* IRFunction::new_label()
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        woort_IRLabel* const result = woort_IRFunction_new_label(m_irfunction);
        if (result == nullptr)
            m_ircompiler->abondon();

        return result;
    }

    void IRFunction::mov(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MOV(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::load(woort_IRValue* dst, woort_IRStaticIndex src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LOAD(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::store(woort_IRStaticIndex dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STORE(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::pushchk(const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_PUSHCHK(m_irfunction, src))
            m_ircompiler->abondon();
    }

    void IRFunction::popr(uint32_t count)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_POPR(m_irfunction, count))
            m_ircompiler->abondon();
    }

    void IRFunction::poprs(const woort_IRValue* count_src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_POPRS(m_irfunction, count_src))
            m_ircompiler->abondon();
    }

    void IRFunction::itor(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_ITOR(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::itos(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_ITOS(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::rtoi(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_RTOI(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::rtos(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_RTOS(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::stoi(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STOI(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::stor(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STOR(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::callnwo(woort_IRConstantIndex target, uint32_t argc, woort_IRValue* dst)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_CALLNWO(m_irfunction, target, argc, dst))
            m_ircompiler->abondon();
    }

    void IRFunction::callnfp(woort_IRConstantIndex target, uint32_t argc, woort_IRValue* dst)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_CALLNFP(m_irfunction, target, argc, dst))
            m_ircompiler->abondon();
    }

    void IRFunction::callnjit(woort_IRConstantIndex target, uint32_t argc, woort_IRValue* dst)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_CALLNJIT(m_irfunction, target, argc, dst))
            m_ircompiler->abondon();
    }

    void IRFunction::call(const woort_IRValue* func_val, uint32_t argc, woort_IRValue* dst)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_CALL(m_irfunction, func_val, argc, dst))
            m_ircompiler->abondon();
    }

    void IRFunction::mkclosure(woort_IRValue* dst, uint32_t elem_count, woort_IRConstantIndex func_idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MKCLOSURE(m_irfunction, dst, elem_count, func_idx))
            m_ircompiler->abondon();
    }

    void IRFunction::mkvec(woort_IRValue* dst, uint32_t elem_count)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MKVEC(m_irfunction, dst, elem_count))
            m_ircompiler->abondon();
    }

    void IRFunction::mkmap(woort_IRValue* dst, uint32_t kvpair_count)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MKMAP(m_irfunction, dst, kvpair_count))
            m_ircompiler->abondon();
    }

    void IRFunction::mkstruct(woort_IRValue* dst, uint32_t elem_count)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MKSTRUCT(m_irfunction, dst, elem_count))
            m_ircompiler->abondon();
    }

    void IRFunction::boxdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_BOXDYN(m_irfunction, dst, typ, src))
            m_ircompiler->abondon();
    }

    void IRFunction::unboxdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_UNBOXDYN(m_irfunction, dst, typ, src))
            m_ircompiler->abondon();
    }

    void IRFunction::checkdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_CHECKDYN(m_irfunction, dst, typ, src))
            m_ircompiler->abondon();
    }

    void IRFunction::pushboxdyn(uint8_t typ, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_PUSHBOXDYN(m_irfunction, typ, src))
            m_ircompiler->abondon();
    }

    void IRFunction::addi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_ADDI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::subi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_SUBI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::muli(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MULI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::divi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_DIVI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::modi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MODI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::negi(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_NEGI(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::lti(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LTI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::gti(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_GTI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::lei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LEI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::gei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_GEI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::eqi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_EQI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::nei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_NEI(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::addr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_ADDR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::subr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_SUBR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::mulr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MULR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::divr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_DIVR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::modr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MODR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::negr(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_NEGR(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::ltr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LTR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::gtr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_GTR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::ler(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LER(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::ger(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_GER(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::eqr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_EQR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::ner(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_NER(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::adds(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_ADDS(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::lts(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LTS(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::gts(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_GTS(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::les(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LES(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::ges(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_GES(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::eqs(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_EQS(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::nes(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_NES(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::land(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LAND(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::lor(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LOR(m_irfunction, dst, a, b))
            m_ircompiler->abondon();
    }

    void IRFunction::lnot(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LNOT(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxvec(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXVEC(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxvecx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXVECX(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxstruct(woort_IRValue* dst, const woort_IRValue* container, uint32_t idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXSTRUCT(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxstring(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXSTRING(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxdicti(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXDICTI(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxdictr(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXDICTR(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxdictb(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXDICTB(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::ldidxdictx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LDIDXDICTX(m_irfunction, dst, container, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxveci(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXVECI(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxvecr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXVECR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxvecb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXVECB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxvecx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXVECX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictii(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTII(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictir(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTIR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictib(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTIB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictix(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTIX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictri(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTRI(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictrr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTRR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictrb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTRB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictrx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTRX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictbi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTBI(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictbr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTBR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictbb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTBB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictbx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTBX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictxi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTXI(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictxr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTXR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictxb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTXB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxdictxx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXDICTXX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapii(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPII(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapir(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPIR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapib(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPIB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapix(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPIX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapri(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPRI(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmaprr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPRR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmaprb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPRB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmaprx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPRX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapbi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPBI(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapbr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPBR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapbb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPBB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapbx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPBX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapxi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPXI(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapxr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPXR(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapxb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPXB(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxmapxx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXMAPXX(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::stidxstruct(woort_IRValue* c, uint32_t idx, const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_STIDXSTRUCT(m_irfunction, c, idx, val))
            m_ircompiler->abondon();
    }

    void IRFunction::unpackstruct(const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_UNPACKSTRUCT(m_irfunction, src))
            m_ircompiler->abondon();
    }

    void IRFunction::unpackvec(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_UNPACKVEC(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::unpackvecx(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_UNPACKVECX(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::pushidxstruct(const woort_IRValue* src, uint32_t idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_PUSHIDXSTRUCT(m_irfunction, src, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::pushidxstboxi(const woort_IRValue* src, uint32_t idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_PUSHIDXSTBOXI(m_irfunction, src, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::pushidxstboxr(const woort_IRValue* src, uint32_t idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_PUSHIDXSTBOXR(m_irfunction, src, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::pushidxstboxb(const woort_IRValue* src, uint32_t idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_PUSHIDXSTBOXB(m_irfunction, src, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::pushidxstboxx(const woort_IRValue* src, uint32_t idx)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_PUSHIDXSTBOXX(m_irfunction, src, idx))
            m_ircompiler->abondon();
    }

    void IRFunction::bind(woort_IRLabel* label)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_bind(m_irfunction, label))
            m_ircompiler->abondon();
    }

    void IRFunction::jmp(woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jmp(m_irfunction, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jcc(const woort_IRValue* cond, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jcc(m_irfunction, cond, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jccz(const woort_IRValue* cond, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jccz(m_irfunction, cond, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jcc_lt(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jcc_lt(m_irfunction, a, b, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jcc_le(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jcc_le(m_irfunction, a, b, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jcc_eq(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jcc_eq(m_irfunction, a, b, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jcc_gt(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jcc_gt(m_irfunction, a, b, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jcc_ge(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jcc_ge(m_irfunction, a, b, target))
            m_ircompiler->abondon();
    }

    void IRFunction::jcc_ne(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_jcc_ne(m_irfunction, a, b, target))
            m_ircompiler->abondon();
    }

    void IRFunction::ret(const woort_IRValue* val)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_ret(m_irfunction, val))
            m_ircompiler->abondon();
    }

    void IRFunction::ret_void()
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_ret_void(m_irfunction))
            m_ircompiler->abondon();
    }

    void IRFunction::push_srcloc(
        const char* filepath,
        uint32_t begin_line,
        uint32_t begin_column,
        uint32_t end_line,
        uint32_t end_column)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IRFunction_push_srcloc(m_irfunction, filepath, begin_line, begin_column, end_line, end_column))
            m_ircompiler->abondon();
    }

    void IRFunction::pop_srcloc()
    {
        if (m_ircompiler->is_abondoned())
            return;

        woort_IRFunction_pop_srcloc(m_irfunction);
    }

    std::optional<woort_CodeEnv*> IRCompiler::commit()
    {
        if (is_abondoned())
            return std::nullopt;

        woort_CodeEnv* cenv;
        if (!woort_IRCompiler_finish(m_ircompiler, &cenv))
            return std::nullopt;

        m_ircompiler = nullptr;
        return cenv;
    }

    const woort_Bytecode* IRCompiler::get_function(woort_CodeEnv* cenv, IRFunction f)
    {
        const woort_Bytecode* bytecode;
        if (!woort_CodeEnv_query_function(cenv, f.m_irfunction, &bytecode))
            // Should not happend.
            abort();

        return bytecode;
    }
}