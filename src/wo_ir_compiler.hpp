#pragma once

#include <optional>
#include <stack>
#include <string_view>

#include "wo_const_string_pool.hpp"

namespace wo
{
    namespace ast
    {
        class AstBase;
    }

    struct NamedLabelInAst
    {
        ast::AstBase* m_ast;
        const char* m_label_name;

        bool operator == (const NamedLabelInAst& other) const
        {
            return m_ast == other.m_ast
                && 0 == strcmp(m_label_name, other.m_label_name);
        }
    };
}

template<>
struct std::hash<wo::NamedLabelInAst>
{
    size_t operator () (const wo::NamedLabelInAst& val) const
    {
        size_t h = std::hash<const void*>()(val.m_ast);
        h ^= std::hash<std::string_view>()(val.m_label_name) + 0x9e3779b9
            + (h << 6) + (h >> 2);
        return h;
    }
};

namespace wo
{
    struct IRFunction
    {
        woort_IRFunction* m_irfunction;
        std::unordered_map<NamedLabelInAst, woort_IRLabel*>
            m_local_named_labels;
    };

    class IRCompiler
    {
        /* OPTIONAL */ woort_IRCompiler* m_ircompiler;

        IRCompiler(const IRCompiler&) = delete;
        IRCompiler(IRCompiler&&) = delete;
        IRCompiler& operator = (const IRCompiler&) = delete;
        IRCompiler& operator = (IRCompiler&&) = delete;

        std::vector<IRFunction> m_current_functions_stack;

        // Constant context
        std::unordered_map<woort_Int, woort_IRConstantIndex>
            m_nil_bool_int_handle_imm_pool;
        std::unordered_map<woort_Int, woort_IRConstantIndex>
            m_boxed_int_imm_pool;
        std::optional<woort_IRConstantIndex> m_boxed_true_imm;
        std::optional<woort_IRConstantIndex> m_boxed_false_imm;
        std::unordered_map<woort_Real, woort_IRConstantIndex>
            m_real_imm_pool;
        std::unordered_map<woort_Real, woort_IRConstantIndex>
            m_boxed_real_imm_pool;
        std::unordered_map<wo_pstring_t, woort_IRConstantIndex>
            m_string_imm_pool;
        std::unordered_map<ast::AstValueFunction*, woort_IRConstantIndex>
            m_closure_imm_pool;
        std::unordered_map<ast::AstValueFunction*, woort_IRConstantIndex>
            m_function_imm_pool;

        struct TupleImm
        {
            woort_IRConstantIndex m_idx;
            std::vector<woort_IRConstantIndex> m_fields;
        };
        std::map<ast::ConstantValue::StructStorage, TupleImm>
            m_tuple_imm_pool;

    public:
        IRCompiler();
        ~IRCompiler();

        void abondon();

    public:
        bool is_abondoned() const;

        woort_IRFunction* push_function(uint32_t param_count, uint32_t captured_count);
        void pop_function();

        woort_IRConstantIndex alloc_constant();
        woort_IRStaticIndex alloc_static();

        std::optional<woort_CodeEnv*> commit();
        const woort_Bytecode* get_function(woort_CodeEnv* cenv, woort_IRFunction* irfunc);

    public:
        const woort_IRValue* fetch_constant(woort_IRConstantIndex cidx);
        woort_IRValue* new_value();
        const woort_IRValue* argument(uint32_t aidx);
        const woort_IRValue* captured(uint32_t aidx);

        woort_IRLabel* new_label();
        woort_IRLabel* named_label(ast::AstBase* ast, const char* label_name);

        void mov(woort_IRValue* dst, const woort_IRValue* src);
        void load(woort_IRValue* dst, woort_IRStaticIndex src);
        void store(woort_IRStaticIndex dst, const woort_IRValue* src);
        void pushchk(const woort_IRValue* src);
        void pushstaticchk(woort_IRStaticIndex static_src);
        void popr(uint32_t count);
        void poprs(const woort_IRValue* count_src);

        /* --- Type Conversions --- */
        void itor(woort_IRValue* dst, const woort_IRValue* src);
        void itos(woort_IRValue* dst, const woort_IRValue* src);
        void rtoi(woort_IRValue* dst, const woort_IRValue* src);
        void rtos(woort_IRValue* dst, const woort_IRValue* src);
        void caststo(woort_IRValue* dst, const woort_IRValue* src, woort_BoxValueType t);
        void castsfrom(woort_IRValue* dst, const woort_IRValue* src, woort_BoxValueType t);
        void castdyn(woort_IRValue* dst, const woort_IRValue* src, woort_BoxValueType t);
        void assertdyn(const woort_IRValue* src, woort_BoxValueType t);

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

        /* --- Atomic --- */
        void astore(woort_IRStaticIndex idx, const woort_IRValue* src);
        void aload(woort_IRValue* dst, woort_IRStaticIndex idx);
        void cas(woort_IRStaticIndex idx, woort_IRValue* expected, const woort_IRValue* desired);

        /* --- Control Flow --- */
        void bind(woort_IRLabel* label);
        void jmp(woort_IRLabel* target);
        void jifinited(woort_IRStaticIndex cond_idx, woort_IRLabel* target);
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

        /* --- Imm load helper */
        woort_IRConstantIndex imm_int(woort_Int val);
        woort_IRConstantIndex imm_box_int(woort_Int val);
        woort_IRConstantIndex imm_real(woort_Real val);
        woort_IRConstantIndex imm_box_real(woort_Real val);
        woort_IRConstantIndex imm_box_bool(bool val);
        woort_IRConstantIndex imm_string(wo_pstring_t val);
        woort_IRConstantIndex imm_closure(ast::AstValueFunction* val);
        woort_IRConstantIndex imm_function(ast::AstValueFunction* val);

        woort_IRConstantIndex imm_nil();
        woort_IRConstantIndex imm_bool(bool val);
        woort_IRConstantIndex imm_handle(woort_Handle handle);
        woort_IRConstantIndex imm_box_handle(woort_Handle handle);

        const woort_IRValue* load_imm_int(woort_Int val);
        const woort_IRValue* load_imm_box_int(woort_Int val);
        const woort_IRValue* load_imm_real(woort_Real val);
        const woort_IRValue* load_imm_box_real(woort_Real val);
        const woort_IRValue* load_imm_box_bool(bool val);
        const woort_IRValue* load_imm_string(wo_pstring_t val);
        const woort_IRValue* load_imm_closure(ast::AstValueFunction* val);
        const woort_IRValue* load_imm_function(ast::AstValueFunction* val);

        const woort_IRValue* load_imm_nil();
        const woort_IRValue* load_imm_bool(bool val);
        const woort_IRValue* load_imm_handle(woort_Handle handle);
        const woort_IRValue* load_imm_box_handle(woort_Handle handle);

        woort_IRConstantIndex imm_const(
            const ast::ConstantValue& constant);
        woort_IRConstantIndex imm_box_const(
            const ast::ConstantValue& constant);
        const woort_IRValue* load_imm_const(
            const ast::ConstantValue& constant);
        const woort_IRValue* load_imm_box_const(
            const ast::ConstantValue& constant);


        /* --- Debug --- */
        void nop();
        void debugtrap();
        void panic(const woort_IRValue* msg);

        /* --- Source Location --- */
        void push_srcloc(const ast::AstBase* node);
        void pop_srcloc();

    };
}
