#include "wo_afx.hpp"

#include "wo_ir_compiler.hpp"
#include "wo_repl_context.hpp"
#include "wo_compiler_parser.hpp"
#include "wo_lang_ast.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    IRCompiler::IRCompiler()
    {
        m_ircompiler = woort_IRCompiler_create();

        static_assert(
            static_cast<void*>(nullptr) == static_cast<void*>(NULL));
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

    void IRCompiler::reset()
    {
        if (m_ircompiler != nullptr)
            abondon();

        m_entry_function.reset();
        m_current_functions_stack.clear();

        m_nil_bool_int_handle_imm_pool.clear();
        m_boxed_int_imm_pool.clear();
        m_boxed_true_imm.reset();
        m_boxed_false_imm.reset();
        m_real_imm_pool.clear();
        m_boxed_real_imm_pool.clear();
        m_string_imm_pool.clear();
        m_closure_imm_pool.clear();
        m_function_imm_pool.clear();
        m_extern_symbols.clear();
        m_loaded_extern_libs.clear();
        m_tuple_imm_pool.clear();
        m_ordered_tuple_imm_list.clear();

        m_ircompiler = woort_IRCompiler_create();
    }

    woort_IRFunction* IRCompiler::push_function(
        uint32_t param_count, uint32_t captured_count)
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* irfunc;
        if (!woort_IRCompiler_add_function(m_ircompiler, param_count, captured_count, &irfunc))
        {
            abondon();
            return nullptr;
        }
        m_current_functions_stack.push_back(IRFunction{ irfunc, {} });
        return irfunc;
    }
    void IRCompiler::set_entry_function(woort_IRFunction* f)
    {
        wo_assert(!m_entry_function.has_value());
        m_entry_function.emplace(f);
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

    std::optional<woort_CodeEnv*> IRCompiler::commit(const std::optional<REPLContext*>& repl_ctx)
    {
        if (is_abondoned())
            return std::nullopt;

        // All function must be ended.
        wo_assert(m_current_functions_stack.empty());

        // Must contain a entry function.
        wo_assert(m_entry_function.has_value());

        const woort_IRConstantIndex entry_function_index = alloc_constant();
        register_extern_symbols(WOORT_DEFAULT_ENTRY, entry_function_index);

        woort_CodeEnv* cenv;
        if (!woort_IRCompiler_finish(m_ircompiler, &cenv))
            return std::nullopt;

        woort_CodeEnv_lock(cenv);

        // Apply constant.
        for (const auto& [val, cidx] : m_nil_bool_int_handle_imm_pool)
        {
            woort_CodeEnv_set_const_int(cenv, cidx, val);
        }

        for (const auto& [val, cidx] : m_real_imm_pool)
        {
            woort_CodeEnv_set_const_real(cenv, cidx, val);
        }

        for (const auto& [val, cidx] : m_string_imm_pool)
        {
            woort_CodeEnv_set_const_buffer(
                cenv, cidx, val->c_str(), val->size());
        }

        for (const auto& [val, cidx] : m_boxed_int_imm_pool)
        {
            woort_CodeEnv_set_const_box_int(cenv, cidx, val);
        }

        for (const auto& [val, cidx] : m_boxed_real_imm_pool)
        {
            woort_CodeEnv_set_const_box_real(cenv, cidx, val);
        }

        if (m_boxed_true_imm.has_value())
        {
            woort_CodeEnv_set_const_box_bool(cenv, *m_boxed_true_imm, true);
        }

        if (m_boxed_false_imm.has_value())
        {
            woort_CodeEnv_set_const_box_bool(cenv, *m_boxed_false_imm, false);
        }

        // Record bytecodes for all script functions emitted in this compile
        // session into m_prior_function_bytecode. This is a dedicated step,
        // decoupled from the imm-pool loops below, so that every emitted
        // function is recorded — including ones never referenced via an
        // immediate (e.g. functions whose eval result was ignored). Without
        // this, a prior-eval function missing from the map would cause
        // AstValueFunctionCall to wrongly dispatch callnwo instead of far
        // call in the next REPL eval.
        if (repl_ctx.has_value())
        {
            auto& emitted = repl_ctx.value()->m_emitted_script_functions_for_REPL;
            for (ast::AstValueFunction* func : emitted)
            {
                repl_ctx.value()->m_prior_function_bytecode[func] =
                    get_function(cenv, func->m_IR_function_MUST_BE_CLEAR_FOR_REPL.value());
            }
        }

        for (const auto& [func, cidx] : m_function_imm_pool)
        {
            if (func->m_LANG_extern_information.has_value())
            {
                woort_CodeEnv_set_const_extern_function(
                    cenv,
                    cidx,
                    func->m_LANG_extern_information.value()
                        ->m_IR_externed_function_MUST_BE_CLEAR_FOR_REPL.value());
            }
            else if (repl_ctx.has_value())
            {
                // All script functions (freshly emitted or prior-eval) are
                // in m_prior_function_bytecode after the recording step above.
                woort_CodeEnv_set_const_script_function(
                    cenv, cidx, repl_ctx.value()->m_prior_function_bytecode.at(func));
            }
            else
            {
                woort_CodeEnv_set_const_script_function(
                    cenv, cidx,
                    get_function(cenv, func->m_IR_function_MUST_BE_CLEAR_FOR_REPL.value()));
            }
        }

        for (const auto& [func, cidx] : m_closure_imm_pool)
        {
            if (func->m_LANG_extern_information.has_value())
            {
                woort_CodeEnv_set_const_extern_closure(
                    cenv,
                    cidx,
                    func->m_LANG_extern_information.value()
                        ->m_IR_externed_function_MUST_BE_CLEAR_FOR_REPL.value());
            }
            else if (repl_ctx.has_value())
            {
                woort_CodeEnv_set_const_script_closure(
                    cenv, cidx, repl_ctx.value()->m_prior_function_bytecode.at(func));
            }
            else
            {
                woort_CodeEnv_set_const_script_closure(
                    cenv, cidx,
                    get_function(cenv, func->m_IR_function_MUST_BE_CLEAR_FOR_REPL.value()));
            }
        }


        // NOTE: Make sure tuple constant set at last, and use `m_ordered_tuple_imm_list` 
        //      instead of walking `m_tuple_imm_pool` to keep order.
        for (const auto* tuple_imm : m_ordered_tuple_imm_list)
        {
            woort_CodeEnv_set_const_struct(
                cenv, tuple_imm->m_idx, tuple_imm->m_fields.data(), tuple_imm->m_fields.size());
        }

        // Apply entry codes.
        woort_CodeEnv_set_const_script_closure(
            cenv, entry_function_index, get_function(cenv, m_entry_function.value()));

        for (const auto& [sym_name, sym_cidx] : m_extern_symbols)
        {
            if (!woort_CodeEnv_register_extern_constant(cenv, sym_name.c_str(), sym_cidx))
            {
                woort_CodeEnv_unlock(cenv);
                woort_CodeEnv_drop(cenv);

                abondon();
                return std::nullopt;
            }
        }

        // Bind loaded extern libraries to CodeEnv lifecycle
        for (woort_Dylib* lib : m_loaded_extern_libs)
        {
            if (!woort_CodeEnv_add_extern_lib(cenv, lib))
            {
                woort_CodeEnv_unlock(cenv);
                woort_CodeEnv_drop(cenv);

                abondon();
                return std::nullopt;
            }
        }

        woort_CodeEnv_unlock(cenv);

        abondon();
        return cenv;
    }

    const woort_Bytecode* IRCompiler::get_function(woort_CodeEnv* cenv, woort_IRFunction* irfunc)
    {
        const woort_Bytecode* bytecode;
        if (!woort_CodeEnv_query_function(cenv, irfunc, &bytecode))
            abort();

        return bytecode;
    }

    void IRCompiler::register_extern_symbols(std::string_view name, woort_IRConstantIndex cidx)
    {
        if (m_extern_symbols.find(std::string(name)) != m_extern_symbols.end())
            wo_error("Duplicate extern symbol name");

        m_extern_symbols.emplace(name, cidx);
    }

    void IRCompiler::add_extern_lib(woort_Dylib* lib)
    {
        (void)m_loaded_extern_libs.insert(lib);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    const woort_IRValue* IRCompiler::fetch_constant(woort_IRConstantIndex cidx)
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        const woort_IRValue* const result = woort_IRFunction_fetch_const(cur, cidx);
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

    const woort_IRValue* IRCompiler::captured(uint32_t aidx)
    {
        if (is_abondoned())
            return nullptr;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        woort_IRValue* const result = woort_IRFunction_get_captured(cur, aidx);
        if (result == nullptr)
            abondon();

        return result;
    }

    void IRCompiler::record_local_var(const char* name, woort_IRValue* v)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        woort_IRFunction_record_local_var(cur, name, v);
    }

    void IRCompiler::record_static_var(const char* name, woort_IRStaticIndex idx)
    {
        if (is_abondoned())
            return;

        woort_IRCompiler_record_static_var(m_ircompiler, name, idx);
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
        NamedLabelInAst key{ ast_node, label_name };

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

    void IRCompiler::mkpvalue(woort_IRValue* dst, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MKPVALUE(cur, dst, src))
            abondon();
    }

    void IRCompiler::loadpvalue(woort_IRValue* dst, const woort_IRValue* ptr)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LOADPVALUE(cur, dst, ptr))
            abondon();
    }

    void IRCompiler::storepvalue(const woort_IRValue* ptr, const woort_IRValue* src)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STOREPVALUE(cur, ptr, src))
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

    void IRCompiler::caststo(woort_IRValue* dst, const woort_IRValue* src, woort_BoxValueType t)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CASTSTO(cur, dst, static_cast<uint8_t>(t), src))
            abondon();
    }

    void IRCompiler::castsfrom(woort_IRValue* dst, const woort_IRValue* src, woort_BoxValueType t)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CASTSFROM(cur, dst, static_cast<uint8_t>(t), src))
            abondon();
    }

    void IRCompiler::castdyn(woort_IRValue* dst, const woort_IRValue* src, woort_BoxValueType t)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CASTDYN(cur, dst, static_cast<uint8_t>(t), src))
            abondon();
    }

    void IRCompiler::assertdyn(const woort_IRValue* src, woort_BoxValueType t)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_ASSERTDYN(cur, static_cast<uint8_t>(t), src))
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

    void IRCompiler::packarg(woort_IRValue* dst, uint16_t named_param_count)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PACKARG(cur, named_param_count, dst))
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

    void IRCompiler::mkunion(woort_IRValue* dst, const woort_IRValue* src, uint32_t union_id)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_MKUNION(cur, dst, src, union_id))
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

    void IRCompiler::chkdivil(const woort_IRValue* a)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CHKDIVIL(cur, a))
            abondon();
    }

    void IRCompiler::chkdivir(const woort_IRValue* a)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CHKDIVIR(cur, a))
            abondon();
    }

    void IRCompiler::chkdivirz(const woort_IRValue* a)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CHKDIVIRZ(cur, a))
            abondon();
    }

    void IRCompiler::chkdivilr(const woort_IRValue* a, const woort_IRValue* b)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_CHKDIVILR(cur, a, b))
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

    void IRCompiler::ldidvec(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDVEC(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidvecx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDVECX(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidstruct(woort_IRValue* dst, const woort_IRValue* container, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDSTRUCT(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldidstring(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDSTRING(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddicti(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTI(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddictr(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTR(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddictb(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTB(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddictx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTX(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddictix(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTIX(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddictrx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTRX(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddictbx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTBX(cur, dst, container, idx))
            abondon();
    }

    void IRCompiler::ldiddictxx(woort_IRValue* dst, const woort_IRValue* container, const woort_IRValue* idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_LDIDDICTXX(cur, dst, container, idx))
            abondon();
    }


    void IRCompiler::stidveci(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDVECI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidvecr(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDVECR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidvecb(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDVECB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidvecx(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDVECX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictii(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTII(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictir(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTIR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictib(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTIB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictix(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTIX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictri(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTRI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictrr(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTRR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictrb(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTRB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictrx(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTRX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictbi(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTBI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictbr(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTBR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictbb(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTBB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictbx(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTBX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictxi(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTXI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictxr(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTXR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictxb(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTXB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stiddictxx(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDDICTXX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapii(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPII(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapir(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPIR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapib(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPIB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapix(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPIX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapri(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPRI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmaprr(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPRR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmaprb(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPRB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmaprx(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPRX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapbi(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPBI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapbr(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPBR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapbb(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPBB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapbx(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPBX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapxi(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPXI(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapxr(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPXR(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapxb(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPXB(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidmapxx(const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDMAPXX(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::stidstruct(const woort_IRValue* c, uint32_t idx, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_STIDSTRUCT(cur, c, idx, val))
            abondon();
    }

    void IRCompiler::unpackvec(uint8_t count, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNPACKVEC(cur, count, val))
            abondon();
    }

    void IRCompiler::unpackvecx(uint8_t count, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNPACKVECX(cur, count, val))
            abondon();
    }

    void IRCompiler::unpackvecall(woort_IRValue* dst, uint8_t count, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNPACKVECALL(cur, dst, count, val))
            abondon();
    }

    void IRCompiler::unpackvecxall(woort_IRValue* dst, uint8_t count, const woort_IRValue* val)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_UNPACKVECXALL(cur, dst, count, val))
            abondon();
    }

    void IRCompiler::pushidstboxi(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDSTBOXI(cur, src, idx))
            abondon();
    }

    void IRCompiler::pushidstboxr(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDSTBOXR(cur, src, idx))
            abondon();
    }

    void IRCompiler::pushidstboxb(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDSTBOXB(cur, src, idx))
            abondon();
    }

    void IRCompiler::pushidstruct(const woort_IRValue* src, uint32_t idx)
    {
        if (is_abondoned())
            return;

        woort_IRFunction* cur = m_current_functions_stack.back().m_irfunction;
        if (!woort_IR_PUSHIDSTRUCT(cur, src, idx))
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
            (uint32_t)node->source_location.begin_at.row,
            (uint32_t)node->source_location.begin_at.column,
            (uint32_t)node->source_location.end_at.row,
            (uint32_t)node->source_location.end_at.column))
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
        return fetch_constant(imm_int(val));
    }
    const woort_IRValue* IRCompiler::load_imm_box_int(woort_Int val)
    {
        return fetch_constant(imm_box_int(val));
    }
    const woort_IRValue* IRCompiler::load_imm_real(woort_Real val)
    {
        return fetch_constant(imm_real(val));
    }
    const woort_IRValue* IRCompiler::load_imm_box_real(woort_Real val)
    {
        return fetch_constant(imm_box_real(val));
    }
    const woort_IRValue* IRCompiler::load_imm_box_bool(bool val)
    {
        return fetch_constant(imm_box_bool(val));
    }
    const woort_IRValue* IRCompiler::load_imm_string(wo_pstring_t val)
    {
        return fetch_constant(imm_string(val));
    }
    const woort_IRValue* IRCompiler::load_imm_closure(ast::AstValueFunction* val)
    {
        return fetch_constant(imm_closure(val));
    }
    const woort_IRValue* IRCompiler::load_imm_function(ast::AstValueFunction* val)
    {
        return fetch_constant(imm_function(val));
    }
    const woort_IRValue* IRCompiler::load_imm_nil()
    {
        return fetch_constant(imm_nil());
    }
    const woort_IRValue* IRCompiler::load_imm_bool(bool val)
    {
        return fetch_constant(imm_bool(val));
    }
    const woort_IRValue* IRCompiler::load_imm_handle(woort_Handle handle)
    {
        return fetch_constant(imm_handle(handle));
    }
    const woort_IRValue* IRCompiler::load_imm_box_handle(woort_Handle handle)
    {
        return fetch_constant(imm_box_handle(handle));
    }

    woort_IRConstantIndex IRCompiler::imm_const(
        const ast::ConstantValue& constant)
    {
        if (is_abondoned())
            return 0;

        switch (constant.m_type)
        {
        case ast::ConstantValue::Type::NIL:
            return imm_nil();
        case ast::ConstantValue::Type::BOOL:
            return imm_bool(constant.value_bool());
        case ast::ConstantValue::Type::INTEGER:
            return imm_int(constant.value_integer());
        case ast::ConstantValue::Type::HANDLE:
            return imm_handle(constant.value_handle());
        case ast::ConstantValue::Type::REAL:
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
                        imm_const(struct_data.m_elements[i]));
                }
                t.m_idx = alloc_constant();
                woort_IRConstantIndex result_idx = t.m_idx;
                m_ordered_tuple_imm_list.push_back(
                    &m_tuple_imm_pool.emplace(struct_data, std::move(t)).first->second);
                return result_idx;
            }
            return it->second.m_idx;
        }
        default:
            wo_unreachable("Unknown ConstantValue type");
            return 0;
        }
    }
    woort_IRConstantIndex IRCompiler::imm_box_const(
        const ast::ConstantValue& constant)
    {
        if (is_abondoned())
            return 0;

        switch (constant.m_type)
        {
        case ast::ConstantValue::Type::BOOL:
            return imm_box_bool(constant.value_bool());
        case ast::ConstantValue::Type::INTEGER:
            return imm_box_int(constant.value_integer());
        case ast::ConstantValue::Type::HANDLE:
            return imm_box_handle(constant.value_handle());
        case ast::ConstantValue::Type::REAL:
            return imm_box_real(constant.value_real());
        default:
            return imm_const(constant);
        }
    }
    const woort_IRValue* IRCompiler::load_imm_const(
        const ast::ConstantValue& constant)
    {
        return fetch_constant(imm_const(constant));
    }
    const woort_IRValue* IRCompiler::load_imm_box_const(
        const ast::ConstantValue& constant)
    {
        return fetch_constant(imm_box_const(constant));
    }
#endif
}
