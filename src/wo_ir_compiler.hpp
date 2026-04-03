#pragma once

#include "woort.h"

#include <optional>

namespace wo
{
    class IRCompiler;

    class IRFunction
    {
        IRCompiler* const m_ircompiler;
        /* OPTIONAL */ woort_IRFunction* m_irfunction;

        friend class IRCompiler;

    public:
        IRFunction(IRCompiler* c, /* OPTIONAL */ woort_IRFunction* irfunc);
        IRFunction(const IRFunction&) = default;
        IRFunction(IRFunction&&) = default;
        IRFunction& operator =(const IRFunction&) = default;
        IRFunction& operator =(IRFunction&&) = default;

    public:
        const woort_IRValue* load_constant(woort_IRConstantIndex cidx);
        woort_IRValue* new_value();
        const woort_IRValue* argument(uint32_t aidx);

        woort_IRLabel* new_label();

        // IR commands.
        void mov(woort_IRValue* dst, const woort_IRValue* src);
        void load(woort_IRValue* dst, woort_IRStaticIndex src);
        void store(woort_IRStaticIndex dst, const woort_IRValue* src);
        void pushchk(const woort_IRValue* src);
        void popr(uint32_t count);
        void poprs(const woort_IRValue* count_src);

        /* --- Type Conversions --- */
        void itor(woort_IRValue* dst, const woort_IRValue* src);
        void itos(woort_IRValue* dst, const woort_IRValue* src);
        void rtoi(woort_IRValue* dst, const woort_IRValue* src);
        void rtos(woort_IRValue* dst, const woort_IRValue* src);
        void stoi(woort_IRValue* dst, const woort_IRValue* src);
        void stor(woort_IRValue* dst, const woort_IRValue* src);

        /* --- Function Calls --- */
        void callnwo(woort_IRConstantIndex target, uint32_t argc, /* OPTIONAL */ woort_IRValue* dst);
        void callnfp(woort_IRConstantIndex target, uint32_t argc, /* OPTIONAL */ woort_IRValue* dst);
        void callnjit(woort_IRConstantIndex target, uint32_t argc, /* OPTIONAL */ woort_IRValue* dst);
        void call(const woort_IRValue* func_val, uint32_t argc, /* OPTIONAL */ woort_IRValue* dst);

        /* --- Closures / Containers --- */
        void mkclosure(woort_IRValue* dst, uint32_t elem_count, woort_IRConstantIndex func_idx);
        void mkvec(woort_IRValue* dst, uint32_t elem_count);
        void mkmap(woort_IRValue* dst, uint32_t kvpair_count);
        void mkstruct(woort_IRValue* dst, uint32_t elem_count);

        /* --- Dynamic Typing --- */
        void boxdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src);
        void unboxdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src);
        void checkdyn(woort_IRValue* dst, uint8_t typ, const woort_IRValue* src);
        void pushboxdyn(uint8_t typ, const woort_IRValue* src);

        /* --- Integer Arithmetic --- */
        void addi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void subi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void muli(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void divi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void modi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void negi(woort_IRValue* dst, const woort_IRValue* src);

        /* --- Integer Comparison --- */
        void lti(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void gti(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void lei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void gei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void eqi(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void nei(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);

        /* --- Real Arithmetic --- */
        void addr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void subr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void mulr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void divr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void modr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void negr(woort_IRValue* dst, const woort_IRValue* src);

        /* --- Real Comparison --- */
        void ltr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void gtr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void ler(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void ger(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void eqr(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void ner(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);

        /* --- String --- */
        void adds(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void lts(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void gts(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void les(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void ges(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void eqs(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void nes(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);

        /* --- Logical --- */
        void land(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void lor(woort_IRValue* dst, const woort_IRValue* a, const woort_IRValue* b);
        void lnot(woort_IRValue* dst, const woort_IRValue* src);

        /* --- Index Load --- */
        void ldidxvec(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx);
        void ldidxvecx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx);
        void ldidxstruct(woort_IRValue* dst, const woort_IRValue* container, uint32_t idx);
        void ldidxstring(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx);
        void ldidxdicti(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx);
        void ldidxdictr(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx);
        void ldidxdictb(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx);
        void ldidxdictx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx);

        /* --- Index Store --- */
        void stidxveci(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxvecr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxvecb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxvecx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);

        void stidxdictii(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictir(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictib(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictix(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictri(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictrr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictrb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictrx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictbi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictbr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictbb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictbx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictxi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictxr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictxb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxdictxx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapii(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapir(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapib(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapix(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapri(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmaprr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmaprb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmaprx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapbi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapbr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapbb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapbx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapxi(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapxr(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapxb(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxmapxx(woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);
        void stidxstruct(woort_IRValue* c, uint32_t idx, const woort_IRValue* val);

        /* --- Unpacking --- */
        void unpackstruct(const woort_IRValue* src);
        void unpackvec(woort_IRValue* dst, const woort_IRValue* src);
        void unpackvecx(woort_IRValue* dst, const woort_IRValue* src);

        /* --- Struct Field Push to Stack --- */
        void pushidxstruct(const woort_IRValue* src, uint32_t idx);
        void pushidxstboxi(const woort_IRValue* src, uint32_t idx);
        void pushidxstboxr(const woort_IRValue* src, uint32_t idx);
        void pushidxstboxb(const woort_IRValue* src, uint32_t idx);
        void pushidxstboxx(const woort_IRValue* src, uint32_t idx);

        /* --- Control Flow --- */
        void bind(woort_IRLabel* label);
        void jmp(woort_IRLabel* target);
        void jcc(const woort_IRValue* cond, woort_IRLabel* target);
        void jccz(const woort_IRValue* cond, woort_IRLabel* target);
        void jcc_lt(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target);
        void jcc_le(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target);
        void jcc_eq(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target);
        void jcc_gt(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target);
        void jcc_ge(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target);
        void jcc_ne(const woort_IRValue* a, const woort_IRValue* b, woort_IRLabel* target);

        /* --- Return --- */
        void ret(const woort_IRValue* val);
        void ret_void();

        /* --- Source Location --- */
        void push_srcloc(
            /* OPTIONAL */ const char* filepath,
            uint32_t begin_line,
            uint32_t begin_column,
            uint32_t end_line,
            uint32_t end_column);
        void pop_srcloc();
    };

    class IRCompiler
    {
        /* OPTIONAL */ woort_IRCompiler* m_ircompiler;

        IRCompiler(const IRCompiler&) = delete;
        IRCompiler(IRCompiler&&) = delete;
        IRCompiler& operator = (const IRCompiler&) = delete;
        IRCompiler& operator = (IRCompiler&&) = delete;

    public:
        IRCompiler();
        ~IRCompiler();

        void abondon();

    public:
        bool is_abondoned() const;

        IRFunction add_function(uint32_t param_count);

        woort_IRConstantIndex alloc_constant();
        woort_IRStaticIndex alloc_static();

        std::optional<woort_CodeEnv*> commit();
        const woort_Bytecode* get_function(woort_CodeEnv* cenv, IRFunction f);
    };
}
