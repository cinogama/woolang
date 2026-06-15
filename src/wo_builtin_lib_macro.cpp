#include "wo_afx.hpp"
#include "wo_builtin_lib_macro.hpp"

#include "wo_compiler_lexer.hpp"

static woort_api builtin_macro_lexer_error()
{
    wo::lexer* const lexer_instance =
        static_cast<wo::lexer*>(woort_pointer(0));

    lexer_instance->produce_lexer_error(
        wo::lexer::msglevel_t::error, "%s", woort_string(1));

    return woort_ret_void();
}

static woort_api builtin_macro_lexer_peek()
{
    wo::lexer* const lexer_instance =
        static_cast<wo::lexer*>(woort_pointer(0));

    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_set_struct(WOORT_RETURN_SLOT, 2);

    const auto* token = lexer_instance->peek(true);

    woort_set_int(s, static_cast<woort_Int>(token->m_lex_type));
    woort_struct_set(WOORT_RETURN_SLOT, 0, s);

    woort_set_string(s, token->m_token_text->c_str());
    woort_struct_set(WOORT_RETURN_SLOT, 1, s);

    return woort_ret();
}

static woort_api builtin_macro_lexer_next()
{
    wo::lexer* const lexer_instance =
        static_cast<wo::lexer*>(woort_pointer(0));

    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_set_struct(WOORT_RETURN_SLOT, 2);

    const auto* token = lexer_instance->peek(true);

    woort_set_int(s, static_cast<woort_Int>(token->m_lex_type));
    woort_struct_set(WOORT_RETURN_SLOT, 0, s);

    woort_set_string(s, token->m_token_text->c_str());
    woort_struct_set(WOORT_RETURN_SLOT, 1, s);

    // Use consume_forward to avoid recursive macro handler invoke.
    lexer_instance->consume_forward();

    return woort_ret();
}

static woort_api builtin_macro_lexer_current_path()
{
    wo::lexer* const lexer_instance =
        static_cast<wo::lexer*>(woort_pointer(0));

    return woort_ret_string(
        lexer_instance->get_source_path()->c_str());
}

static woort_api builtin_macro_lexer_current_location()
{
    wo::lexer* const lexer_instance =
        static_cast<wo::lexer*>(woort_pointer(0));

    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_set_struct(WOORT_RETURN_SLOT, 2);

    const auto* token = lexer_instance->peek(true);

    woort_set_int(s, static_cast<woort_Int>(token->m_lex_type));
    woort_struct_set(WOORT_RETURN_SLOT, 0, s);

    woort_set_string(s, token->m_token_text->c_str());
    woort_struct_set(WOORT_RETURN_SLOT, 1, s);

    return woort_ret();
}

static const woort_ExternLibFunc g_macro_funcs[] =
{
    {"macro_lexer_error", (void*)&builtin_macro_lexer_error},
    {"macro_lexer_peek", (void*)&builtin_macro_lexer_peek},
    {"macro_lexer_next", (void*)&builtin_macro_lexer_next},
    {"macro_lexer_current_path", (void*)&builtin_macro_lexer_current_path},
    {"macro_lexer_current_location", (void*)&builtin_macro_lexer_current_location},
    WOORT_EXTERN_LIB_FUNC_END,
};

static /* OPTIONAL */ woort_Dylib* g_macro_lib = nullptr;

bool wo::builtin_macro_lib_bootup()
{
    g_macro_lib = woort_dylib_fake("woolang/compiler", g_macro_funcs, nullptr);
    return g_macro_lib != nullptr;
}

void wo::builtin_macro_lib_shutdown()
{
    if (g_macro_lib != nullptr)
    {
        woort_dylib_unload(g_macro_lib, WOORT_DYLIB_UNREF_AND_BURY);
        g_macro_lib = nullptr;
    }
}
