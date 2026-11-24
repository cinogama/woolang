// wo_api_impl.cpp
#include "wo_afx.hpp"

#include "wo_source_file_manager.hpp"
#include "wo_ast_base.hpp"
#include "wo_stdlib_embedded.inc"
#include "wo_lang_parser.hpp"
#include "wo_crc_64.hpp"
#include "wo_ir_compiler.hpp"
#include "wo_builtin_lib_macro.hpp"
#include "wo_path_util.hpp"
#include "wo_woodyn_impl.hpp"

using namespace std;
using namespace wo;
using namespace wo::ast;

namespace wo
{
    compile_result compile_impl(
        const char* virtual_src_path,
        const optional<const void*>& src_may_null,
        size_t src_len,
        const optional<lexer*>& append_macro_define_to_this_lexer,
        optional<woort_CodeEnv*>* out_env_if_success,
        optional<unique_ptr<lexer>>* out_lexer_if_failed
#ifndef WO_DISABLE_COMPILER
        , optional<unique_ptr<LangContext>>* out_langcontext_if_pass_grammar
#endif
    );

    bool compile_entry(
        const char* virtual_src_path,
        const optional<const void*>& src_may_null,
        size_t src_len,
        const optional<lexer*>& append_macro_define_to_this_lexer,
        optional<woort_CodeEnv*>* out_env_if_success,
        optional<unique_ptr<lexer>>* out_lexer_if_failed);

    wo_CompileErrors* make_compile_errors(optional<unique_ptr<lexer>> lex);
}

[[noreturn]]
void wo::assert_failure(
    const char* file,
    uint32_t line,
    const char* function,
    const char* judgement,
    const char* reason)
{
    cerr << ANSI_HIR "Assert failed: " ANSI_RST << judgement << endl;
    if (reason)
        cerr << "\t" ANSI_HIY << reason << ANSI_RST << endl;

    cerr << "Function : " << function << endl;
    cerr << "File : " << file << endl;
    cerr << "Line : " << line << endl;
    abort();
}

void wo::warning_message(
    const char* file,
    uint32_t line,
    const char* function,
    const char* judgement,
    const char* reason)
{
    cerr << ANSI_HIY "Warning: " ANSI_RST << judgement << endl;
    if (reason)
        cerr << "\t" ANSI_HIY << reason << ANSI_RST << endl;

    cerr << "Function : " << function << endl;
    cerr << "File : " << file << endl;
    cerr << "Line : " << line << endl;
}

#undef wo_init
void wo_init(
    int argc,
    char** argv,
    /* OPTIONAL */ const wo_WooDyn_Functions* funcs)
{
    // Start up WooRT (this also registers built-in native functions).
    woodyn::bootup_woort_dynamically(
        argc,
        argv,
        funcs == nullptr ? nullopt : make_optional(funcs));

    for (int command_idx = 0; command_idx + 1 < argc; command_idx++)
    {
        string current_arg = argv[command_idx];
        if (current_arg.size() >= 9 && current_arg.substr(0, 9) == "--woolang")
        {
            current_arg = current_arg.substr(2);
            if ("woolang-enable-runtime-checking-integer-division" == current_arg)
                config::ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = (bool)atoi(argv[++command_idx]);
            else if ("woolang-ignore-not-found-extern-func" == current_arg)
                config::ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL = (bool)atoi(argv[++command_idx]);
            else
                wo_warning(("Unknown command line option named `" + current_arg + "`.").c_str());
        }
    }

    // Register internal scripts.
    for (size_t i = 0; i < woo_embedded_file_count; ++i)
        wo_assure(woort_vfs_create(
            woo_embedded_files[i].path,
            woo_embedded_files[i].data,
            strlen(woo_embedded_files[i].data),
            false));

    wstring_pool::init_global_str_pool();
    lexer::init_char_lookup_table();

#ifndef WO_DISABLE_COMPILER
    init_woolang_parser(); // Initialize AST builders when init.
    LangContext::init_lang_processers();
#endif

    // Cache commonly-used built-in function pointers for the compiler.
    rslib_extern_symbols::cache_builtin_pointers();

    // Register the "woolang/compiler" fake library for macro extern functions.
    builtin_macro_lib_bootup();
}

void wo_print_compiler_help(void)
{
    cout << "Woolang Compiler Options (prefix: --woolang-):\n";
    cout << "    --woolang-enable-runtime-checking-integer-division <0|1>\n";
    cout << "        Generate extra code to check for division by zero and overflow. Default: 1.\n";
    cout << "    --woolang-ignore-not-found-extern-func <0|1>\n";
    cout << "        Ignore missing extern function symbols (replace with panic function). Default: 0.\n";
}

#undef wo_finish
void wo_finish(void(*do_after_shutdown)(void*), void* custom_data)
{
    builtin_macro_lib_shutdown();
    woodyn::shutdown_woort_dynamically(do_after_shutdown, custom_data);

    wstring_pool::shutdown_global_str_pool();

#ifndef WO_DISABLE_COMPILER
    LangContext::shutdown_lang_processers();
    shutdown_woolang_parser();
#endif
}

compile_result wo::compile_impl(
    const char* virtual_src_path,
    const optional<const void*>& src_may_null,
    size_t src_len,
    const optional<lexer*>&
    append_macro_define_to_this_lexer,
    optional<woort_CodeEnv*>*
    out_env_if_success,
    optional<unique_ptr<lexer>>*
    out_lexer_if_failed
#ifndef WO_DISABLE_COMPILER
    , optional<unique_ptr<LangContext>>*
    out_langcontext_if_pass_grammar
#endif
)
{
    // 0. Try load binary
    // WOORT_CODEENV_BINARY_MAGIC = 0x30314345u ("EC10")
    compile_result compile_result = compile_result::PROCESS_FAILED;

    optional<woort_CodeEnv*> compile_env_result = nullopt;
    unique_ptr<lexer> compile_lexer;

    // Hoisted out of the compile-success block below: deferred diagnose
    // payloads hold pointers to lang instances, so they must be rendered
    // while this context is still alive (see the failed branch).
    optional<unique_ptr<LangContext>> lang_context;

    optional<woort_VFile*> source_file_instance;
    string real_file_path;

    if (src_may_null.has_value())
    {
        real_file_path = virtual_src_path;
        normalize_path(&real_file_path);

        woort_VFile* vfile;
        if (woort_vfile_open_reader(src_may_null.value(), src_len, &vfile))
            source_file_instance.emplace(vfile);
    }
    else
    {
        if (check_virtual_file_path(
            virtual_src_path,
            nullopt,
            &real_file_path))
        {
            woort_VFile* vfile;
            if (woort_vfile_open(real_file_path.c_str(), &vfile))
                source_file_instance.emplace(vfile);
        }
        else
            real_file_path = virtual_src_path;
    }

    // Try loading binary.
    optional<woort_CodeEnv_RestoreResult> binary_loading_failed = nullopt;
    if (source_file_instance.has_value())
    {
        woort_VFile* const f = source_file_instance.value();

        woort_CodeEnv* v;
        const woort_CodeEnv_RestoreResult r = woort_CodeEnv_restore_binary(f, &v);
        switch (r)
        {
        case WOORT_CODEENV_RESTORE_OK:
            // Ok.
            compile_env_result.emplace(v);
            break;
        case WOORT_CODEENV_RESTORE_FAIL_MAGIC_DOESNT_MATCH:
            // Not binary, continue compiling.
            (void)woort_vfile_seek(f, 0, SEEK_SET);
            break;
        default:
            // Failed.
            binary_loading_failed.emplace(r);
        }
    }

    if (!compile_env_result.has_value())
    {
        optional<unique_ptr<istream>> source_stream;
        if (source_file_instance.has_value())
        {
            source_stream.emplace(
                unique_ptr<istream>(
                    make_unique<vfile_istream>(source_file_instance.value())));

            // File instance will be freed by `wo::vfile_istream`.
            source_file_instance.reset();
        }

        compile_lexer = make_unique<lexer>(
            append_macro_define_to_this_lexer,
            wstring_pool::get_pstr(real_file_path),
            std::move(source_stream),
            nullopt);

        if (binary_loading_failed.has_value())
        {
            // Is Woolang format binary, but failed to load.
            // Failed to load binary, maybe broken or version missing.
            (void)compile_lexer->record_parser_error(
                lexer::msglevel_t::error,
                diagnose::err_raw_message{
                    woort_CodeEnv_restore_failed_desc(binary_loading_failed.value()) });
        }
        else
        {
            // 1. Prepare lexer..
#ifndef WO_DISABLE_COMPILER
            if (!compile_lexer->has_error())
            {
                // 2. Lexer will create ast_tree;
                auto parsed_result = parser::parse_program(*compile_lexer);

                // NOTE: Drop macro vm if useless.
                if (!append_macro_define_to_this_lexer.has_value())
                    compile_lexer->drop_macro_vm_and_code_env();

                if (parsed_result.has_value())
                {
                    compile_result =
                        compile_result::PROCESS_FAILED_BUT_GRAMMAR_OK;

                    unique_ptr<LangContext> langctx = make_unique<LangContext>();

                    compile_result = langctx->process(*compile_lexer, parsed_result.value());
                    if (compile_result::PROCESS_OK == compile_result)
                    {
                        // Finish!, finalize the compiler.
                        compile_env_result =
                            langctx->m_ircontext.finalize(langctx->m_repl_context);

                        if (!compile_env_result.has_value())
                        {
                            compile_result = compile_result::PROCESS_FAILED_BUT_PASS_1_OK;
                            (void)compile_lexer->record_parser_error(
                                lexer::msglevel_t::error,
                                diagnose::lang1::err_out_of_memory{});
                        }
                    }

                    lang_context = std::move(langctx);
                }
            }
#else
            (void)compile_lexer->record_parser_error(
                lexer::msglevel_t::error,
                diagnose::parser::err_compiler_disabled{});
#endif
        }
    }
    else
        // Load binary success. 
        compile_result = compile_result::PROCESS_OK;

    // Close file if need.
    if (source_file_instance.has_value())
        woort_vfile_close(source_file_instance.value());

    // Compile finished.
    if (compile_env_result.has_value())
    {
        // Success
        wo_assert(compile_result == compile_result::PROCESS_OK);

        if (out_env_if_success != nullptr)
            *out_env_if_success = std::move(compile_env_result.value());
    }
    else
    {
        // Failed
        wo_assert((bool)compile_lexer);
        wo_assert(compile_result != compile_result::PROCESS_OK);

        // The compile has definitively failed: generate all deferred message
        // texts now, while lang_context (and the AST arena owned by the
        // caller's thread-local allocator) is still alive.
        //
        // NOTE: if `lang_context` is empty here, that's fine -- when it is empty, 
        //      no error diagnostic messages requiring a lang instance should
        //      be generated, so simply passing nullptr is sufficient.
        //
        // NOTE: VERY UGLY! need to re-impl.
        //
        compile_lexer->realize_pending_diagnose(
            lang_context.has_value() ? lang_context.value().get() : nullptr);

        if (out_lexer_if_failed != nullptr)
            *out_lexer_if_failed = std::move(compile_lexer);
    }

    if (out_langcontext_if_pass_grammar != nullptr)
        *out_langcontext_if_pass_grammar = std::move(lang_context);

    return compile_result;
}

bool wo::compile_entry(
    const char* virtual_src_path,
    const optional<const void*>& src_may_null,
    size_t src_len,
    const optional<lexer*>& append_macro_define_to_this_lexer,
    optional<woort_CodeEnv*>* out_env_if_success,
    optional<unique_ptr<lexer>>* out_lexer_if_failed)
{
    start_string_pool_guard sg;

#ifndef WO_DISABLE_COMPILER
    AstAllocator m_last_context;
    bool need_exchange_back =
        AstBase::exchange_this_thread_ast(m_last_context);
#endif

    const compile_result compile_result =
        compile_impl(
            virtual_src_path,
            src_may_null,
            src_len,
            append_macro_define_to_this_lexer,
            out_env_if_success,
            out_lexer_if_failed
#ifndef WO_DISABLE_COMPILER
            , nullptr
#endif
        );

#ifndef WO_DISABLE_COMPILER
    AstBase::clean_this_thread_ast();

    if (need_exchange_back)
        AstBase::exchange_this_thread_ast(
            m_last_context);
#endif

    return compile_result == compile_result::PROCESS_OK;
}

struct _wo_CompileErrors
{
    optional<unique_ptr<lexer>> m_lexer;
    size_t m_current_index;
    wo_CompileErrorInfo m_current_info;

    _wo_CompileErrors(optional<unique_ptr<lexer>> lex)
        : m_lexer(std::move(lex))
        , m_current_index(0)
    {
        m_current_info.m_file_name = nullptr;
        m_current_info.m_message = nullptr;
        m_current_info.m_begin_row = 0;
        m_current_info.m_begin_col = 0;
        m_current_info.m_end_row = 0;
        m_current_info.m_end_col = 0;
        m_current_info.m_is_error = 0;
        m_current_info.m_layer = 0;
    }

    _wo_CompileErrors(const _wo_CompileErrors&) = delete;
    _wo_CompileErrors(_wo_CompileErrors&&) = delete;
    _wo_CompileErrors& operator=(const _wo_CompileErrors&) = delete;
    _wo_CompileErrors& operator=(_wo_CompileErrors&&) = delete;
};

wo_CompileErrorInfo* wo_compile_errors_next(wo_CompileErrors* errors)
{
    auto& msg_list = errors->m_lexer.value()->get_current_error_frame();
    if (errors->m_current_index >= msg_list.size())
        return nullptr;

    auto& msg = msg_list[errors->m_current_index++];
    errors->m_current_info.m_file_name = msg.filename().c_str();
    errors->m_current_info.m_message = msg.unwrap_describe().c_str();
    errors->m_current_info.m_begin_row = msg.m_range_begin[0];
    errors->m_current_info.m_begin_col = msg.m_range_begin[1];
    errors->m_current_info.m_end_row = msg.m_range_end[0];
    errors->m_current_info.m_end_col = msg.m_range_end[1];
    errors->m_current_info.m_is_error =
        (msg.m_level == lexer::msglevel_t::error) ? 1 : 0;
    errors->m_current_info.m_layer = msg.m_layer;

    return &errors->m_current_info;
}

void wo_compile_errors_free(wo_CompileErrors* errors)
{
    delete errors;
}

// Factory for internal use (e.g., REPL).
wo_CompileErrors* wo::make_compile_errors(optional<unique_ptr<lexer>> lex)
{
    return new _wo_CompileErrors(std::move(lex));
}

namespace wo
{
namespace
{
    string _dump_src_info(
        const string& path,
        const lexer::compiler_message_t& errmsg,
        size_t depth,
        size_t beginaimrow,
        size_t beginpointplace,
        size_t aimrow,
        size_t pointplace,
        wo_inform_style_t style)
    {
        string src_full_path, result;

        if (check_virtual_file_path(path, nullopt, &src_full_path))
        {
            auto content_stream = open_virtual_file_stream(src_full_path);
            if (content_stream)
            {
                auto& content_stream_ptr = content_stream.value();
                wo_assert(content_stream_ptr != nullptr);

                constexpr size_t UP_DOWN_SHOWN_LINE = 2;
                size_t current_row_no = 0;
                size_t current_col_no = 0;
                size_t from = beginaimrow > UP_DOWN_SHOWN_LINE ? beginaimrow - UP_DOWN_SHOWN_LINE : 0;
                size_t to = aimrow + UP_DOWN_SHOWN_LINE;

                bool first_line = true;

                auto print_src_file_print_lineno =
                    [&current_row_no, &result, &first_line, depth]()
                    {
                        char buf[20] = {};
                        if (first_line)
                            first_line = false;
                        else
                            result += "\n";

                        snprintf(buf, 20, "%-5zu | ", current_row_no + 1);
                        result += string(depth == 0 ? 0 : depth + 1, ' ') + buf;
                    };
                auto print_notify_line =
                    [
                        &result,
                        &first_line,
                        &current_row_no,
                        &errmsg,
                        beginpointplace,
                        pointplace,
                        style,
                        beginaimrow,
                        aimrow,
                        depth
                    ](size_t line_end_place)
                {
                    char buf[20] = {};
                    if (first_line)
                        first_line = false;
                    else
                        result += "\n";

                    snprintf(buf, 20, "      | ");
                    string append_result = buf;

                    if (style == WO_COLORFUL)
                        append_result += errmsg.m_level == lexer::msglevel_t::error
                        ? ANSI_HIR
                        : ANSI_HIC;

                    if (current_row_no == aimrow)
                    {
                        if (current_row_no == beginaimrow)
                        {
                            size_t i = 1;
                            for (; i <= beginpointplace; i++)
                                append_result += " ";
                            for (; i < pointplace; i++)
                                append_result += "~";
                        }
                        else
                            for (size_t i = 1; i < pointplace; i++)
                                append_result += "~";

                        append_result +=
                            string("~\\");

                        if (style == WO_COLORFUL)
                            append_result += ANSI_UNDERLNE;

                        append_result +=
                            " " WO_MSG_HERE;

                        if (style == WO_COLORFUL)
                            append_result += ANSI_NUNDERLNE;

                        append_result += "_";

                        if (depth != 0)
                            append_result += ": " + errmsg.unwrap_describe();
                    }
                    else
                    {
                        if (current_row_no == beginaimrow)
                        {
                            size_t i = 1;
                            for (; i <= beginpointplace; i++)
                                append_result += " ";
                            if (i < line_end_place)
                                for (; i < line_end_place; i++)
                                    append_result += "~";
                            else
                                return;
                        }
                        else
                        {
                            size_t i = 1;
                            if (i < line_end_place)
                                for (; i < line_end_place; i++)
                                    append_result += "~";
                            else
                                return;
                        }
                    }

                    if (style == WO_COLORFUL)
                        append_result += ANSI_RST;

                    result += string(depth == 0 ? 0 : depth + 1, ' ') + append_result;
                };

                if (from <= current_row_no && current_row_no <= to)
                    print_src_file_print_lineno();

                for (;;)
                {
                    char ch;
                    content_stream_ptr->read(&ch, 1);

                    if (content_stream_ptr->eof() || !*content_stream_ptr)
                        break;

                    if (ch == '\n')
                    {
                        if (current_row_no >= beginaimrow && current_row_no <= aimrow)
                            print_notify_line(current_col_no);
                        current_col_no = 0;
                        current_row_no++;
                        if (from <= current_row_no && current_row_no <= to)
                            print_src_file_print_lineno();
                        continue;
                    }
                    else if (ch == '\r')
                    {
                        if (current_row_no >= beginaimrow && current_row_no <= aimrow)
                            print_notify_line(current_col_no);
                        current_col_no = 0;
                        current_row_no++;
                        if (from <= current_row_no && current_row_no <= to)
                            print_src_file_print_lineno();

                        auto index = content_stream_ptr->tellg();
                        content_stream_ptr->read(&ch, 1);
                        if (content_stream_ptr->eof() || !*content_stream_ptr || ch != L'\n')
                        {
                            content_stream_ptr->clear(content_stream_ptr->rdstate() & ~ios_base::failbit);
                            content_stream_ptr->seekg(index);
                        }
                        continue;
                    }
                    ++current_col_no;
                    if (from <= current_row_no && current_row_no <= to && ch)
                        result += ch;

                }
                if (current_row_no >= beginaimrow && current_row_no <= aimrow)
                    print_notify_line(current_col_no);

                result += "\n";
            }
        }
        return result;
    }
}
}
namespace wo
{
    namespace
    {
        string dump_lexer_context_error(lexer* lex, wo_inform_style_t style)
        {
            string src_file_path;
            string _vm_compile_errors;

            size_t last_depth = 0;

            for (auto& err_info : lex->get_current_error_frame())
            {
                if (err_info.m_layer != 0)
                {
                    auto see_also = last_depth >= err_info.m_layer ? WO_MSG_SEE_ALSO : WO_MSG_SEE_HERE;
                    if (style == WO_COLORFUL)
                        _vm_compile_errors
                        += string(err_info.m_layer, ' ')
                        + ANSI_HIY + see_also + ANSI_RST
                        + ":\n";
                    else
                        _vm_compile_errors
                        += string(err_info.m_layer, ' ')
                        + see_also
                        + ":\n";
                }

                last_depth = err_info.m_layer;

                if (src_file_path != err_info.filename())
                {
                    if (style == WO_COLORFUL)
                        _vm_compile_errors +=
                        ANSI_HIR "In file: '" ANSI_RST
                        + (src_file_path = err_info.filename())
                        + ANSI_HIR "'" ANSI_RST "\n";
                    else
                        _vm_compile_errors +=
                        "In file: '"
                        + (src_file_path = err_info.filename())
                        + "'\n";
                }

                if (err_info.m_layer == 0)
                    _vm_compile_errors += err_info.to_string(style == WO_COLORFUL) + "\n";

                // Print source informations..
                _vm_compile_errors +=
                    _dump_src_info(
                        src_file_path,
                        err_info,
                        err_info.m_layer,
                        err_info.m_range_begin[0],
                        err_info.m_range_begin[1],
                        err_info.m_range_end[0],
                        err_info.m_range_end[1],
                        style);
            }

            if (lex->get_current_error_frame().size() >= WO_MAX_ERROR_COUNT)
                _vm_compile_errors += WO_MSG_TOO_MANY_ERROR(WO_MAX_ERROR_COUNT) + "\n";

            return _vm_compile_errors;
        }
    }
}

const char* wo_get_compile_error(
    wo_CompileErrors* errors,
    wo_inform_style_t style)
{
    thread_local string _vm_compile_errors;
    _vm_compile_errors.clear();

    if (errors != nullptr)
    {
        auto* impl = reinterpret_cast<_wo_CompileErrors*>(errors);
        if (impl->m_lexer.has_value())
        {
            _vm_compile_errors += dump_lexer_context_error(
                impl->m_lexer.value().get(), style);
        }
    }
    return _vm_compile_errors.c_str();
}

/* OPTIONAL */woort_CodeEnv* wo_load_binary(
    woort_U8CString virtual_src_path,
    const void* buffer,
    size_t length,
    /* OPTIONAL */ wo_CompileErrors** out_errors)
{
    if (out_errors != nullptr)
        *out_errors = nullptr;

    static atomic_size_t vcount = 0;
    string vpath;
    if (virtual_src_path == nullptr)
        vpath = "/woolang/__runtime_script_" + to_string(++vcount) + "__";
    else
        vpath = virtual_src_path;

    if (!woort_vfs_create(vpath.c_str(), buffer, length, true))
        return nullptr;

    optional<woort_CodeEnv*> code_env;
    optional<unique_ptr<lexer>> failed_lexer;

    bool ok = compile_entry(
        vpath.c_str(),
        buffer,
        length,
        nullopt,
        &code_env,
        &failed_lexer);

    if (ok)
    {
        wo_assert(code_env.has_value());
        return code_env.value();
    }

    if (out_errors != nullptr && failed_lexer.has_value())
        *out_errors = make_compile_errors(std::move(failed_lexer));

    return nullptr;
}

/* OPTIONAL */woort_CodeEnv* wo_load_source(
    woort_U8CString virtual_src_path,
    woort_U8CString src,
    /* OPTIONAL */ wo_CompileErrors** out_errors)
{
    return wo_load_binary(virtual_src_path, src, strlen(src), out_errors);
}

/* OPTIONAL */woort_CodeEnv* wo_load_file(
    woort_U8CString virtual_src_path,
    /* OPTIONAL */ wo_CompileErrors** out_errors)
{
    if (out_errors != nullptr)
        *out_errors = nullptr;

    optional<woort_CodeEnv*> code_env;
    optional<unique_ptr<lexer>> failed_lexer;

    const bool ok = compile_entry(
        virtual_src_path,
        nullopt,
        0,
        nullopt,
        &code_env,
        &failed_lexer);

    if (ok)
    {
        wo_assert(code_env.has_value());
        return code_env.value();
    }

    if (out_errors != nullptr && failed_lexer.has_value())
    {
        *out_errors = reinterpret_cast<wo_CompileErrors*>(
            new _wo_CompileErrors(std::move(failed_lexer)));
    }
    return nullptr;
}

uint64_t wo_crc64_u8(uint8_t byte, uint64_t crc)
{
    return crc_64(byte, crc);
}

uint64_t wo_crc64_str(const char* text)
{
    if (text == nullptr)
        return 0;

    return crc_64(text, 0);
}

uint64_t wo_crc64_file(woort_VFile* file)
{
    return crc_64(file, 0);
}

uint64_t wo_crc64_file_from_path(const char* filepath)
{
    if (filepath == nullptr)
        return 0;

    char* resolved = nullptr;
    if (!woort_vfs_resolve_path(filepath, nullptr, 0, &resolved))
        return 0;

    woort_VFile* vfile = nullptr;
    const bool opened = woort_vfile_open(resolved, &vfile);
    woort_free(resolved);

    if (!opened)
        return 0;

    const uint64_t crc = wo_crc64_file(vfile);
    woort_vfile_close(vfile);

    return crc;
}
