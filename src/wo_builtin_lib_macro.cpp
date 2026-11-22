#include "wo_afx.hpp"
#include "wo_builtin_lib_macro.hpp"

#include "wo_compiler_lexer.hpp"
using namespace std;
using namespace wo;

namespace wo
{
namespace
{
woort_api builtin_macro_lexer_error()
{
    lexer* const lexer_instance =
        static_cast<lexer*>(woort_pointer(0));

    lexer_instance->produce_lexer_error(
        lexer::msglevel_t::error,
        diagnose::err_raw_message{woort_string(1)});

    return woort_ret_void();
}

woort_api builtin_macro_lexer_peek()
{
    lexer* const lexer_instance =
        static_cast<lexer*>(woort_pointer(0));

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

woort_api builtin_macro_lexer_next()
{
    lexer* const lexer_instance =
        static_cast<lexer*>(woort_pointer(0));

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

woort_api builtin_macro_lexer_current_path()
{
    lexer* const lexer_instance =
        static_cast<lexer*>(woort_pointer(0));

    return woort_ret_string(
        lexer_instance->get_source_path()->c_str());
}

woort_api builtin_macro_lexer_current_location()
{
    lexer* const lexer_instance =
        static_cast<lexer*>(woort_pointer(0));

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

const woort_ExternLibFunc g_macro_funcs[] =
{
    {"macro_lexer_error", (void*)&builtin_macro_lexer_error},
    {"macro_lexer_peek", (void*)&builtin_macro_lexer_peek},
    {"macro_lexer_next", (void*)&builtin_macro_lexer_next},
    {"macro_lexer_current_path", (void*)&builtin_macro_lexer_current_path},
    {"macro_lexer_current_location", (void*)&builtin_macro_lexer_current_location},
    WOORT_EXTERN_LIB_FUNC_END,
};

optional<woort_Dylib*> g_macro_lib;
}
}

bool wo::builtin_macro_lib_bootup()
{
    woort_Dylib* lib = woort_dylib_fake("woolang/compiler", g_macro_funcs, nullptr);
    if (lib != nullptr)
        g_macro_lib.emplace(lib);

    return g_macro_lib.has_value();
}

void wo::builtin_macro_lib_shutdown()
{
    if (g_macro_lib.has_value())
    {
        woort_dylib_unload(g_macro_lib.value(), WOORT_DYLIB_UNREF_AND_BURY);
        g_macro_lib.reset();
    }
}
