// wo_api_impl.cpp
#include "wo_afx.hpp"

#include "wo_source_file_manager.hpp"
#include "wo_compiler_parser.hpp"
#include "wo_stdlib_embedded.inc"
#include "wo_lang_grammar_loader.hpp"
#include "wo_crc_64.hpp"
#include "wo_ir_compiler.hpp"
#include "wo_builtin_lib_macro.hpp"
#include "wo_path_util.hpp"

[[noreturn]]
void _wo_assert(
    const char* file,
    uint32_t line,
    const char* function,
    const char* judgement,
    const char* reason)
{
    wo::wo_stderr << ANSI_HIR "Assert failed: " ANSI_RST << judgement << wo::wo_endl;
    if (reason)
        wo::wo_stderr << "\t" ANSI_HIY << reason << ANSI_RST << wo::wo_endl;

    wo::wo_stderr << "Function : " << function << wo::wo_endl;
    wo::wo_stderr << "File : " << file << wo::wo_endl;
    wo::wo_stderr << "Line : " << line << wo::wo_endl;
    abort();
}

void _wo_warning(
    const char* file,
    uint32_t line,
    const char* function,
    const char* judgement,
    const char* reason)
{
    wo::wo_stderr << ANSI_HIY "Warning: " ANSI_RST << judgement << wo::wo_endl;
    if (reason)
        wo::wo_stderr << "\t" ANSI_HIY << reason << ANSI_RST << wo::wo_endl;

    wo::wo_stderr << "Function : " << function << wo::wo_endl;
    wo::wo_stderr << "File : " << file << wo::wo_endl;
    wo::wo_stderr << "Line : " << line << wo::wo_endl;
}

void wo_handle_ctrl_c(/* OPTIONAL */ void(*handler)(int))
{
    signal(SIGINT, handler);
}

void _wo_ctrl_c_signal_handler(int)
{
    // CTRL + C
    wo::wo_stderr
        << ANSI_HIR "CTRL+C" ANSI_RST
        << ": Trying to breakdown all virtual-machine by default debuggee immediately."
        << wo::wo_endl;

    (void)woort_WAIPO_Debugger_attach();
    woort_VMRuntime_Debugger_breakdown_all_vm();

    static size_t _wo_ctrl_c_hit_count = 0;
    static time_t _wo_last_ctrl_c_time = 0;

    auto _ctrl_c_time = time(NULL);
    if (_ctrl_c_time - _wo_last_ctrl_c_time < 2)
    {
        if (_wo_ctrl_c_hit_count >= 4)
            wo_error("Panic termination.");
        else
        {
            wo::wo_stderr
                << ANSI_HIY "CTRL+C" ANSI_RST
                << ": Continue pressing Ctrl+C `" ANSI_HIG
                << 4 - _wo_ctrl_c_hit_count
                << ANSI_RST "` time(s) to trigger a panic termination"
                << wo::wo_endl;
        }
    }
    else
        _wo_ctrl_c_hit_count = 0;

    _wo_last_ctrl_c_time = _ctrl_c_time;
    ++_wo_ctrl_c_hit_count;

    wo_handle_ctrl_c(_wo_ctrl_c_signal_handler);
}

#undef wo_init

void wo_init(int argc, char** argv)
{
    // Start up WooRT (this also registers built-in native functions).
    woort_init(argc, argv);

    bool enable_std_package = true;
    bool enable_ctrl_c_to_debug = true;

    for (int command_idx = 0; command_idx + 1 < argc; command_idx++)
    {
        std::string current_arg = argv[command_idx];
        if (current_arg.size() >= 2 && current_arg[0] == '-' && current_arg[1] == '-')
        {
            current_arg = current_arg.substr(2);
            if ("enable-std" == current_arg)
                enable_std_package = atoi(argv[++command_idx]);
            else if ("enable-shell" == current_arg)
                wo::config::ENABLE_SHELL_PACKAGE = atoi(argv[++command_idx]);
            else if ("enable-ctrlc-debug" == current_arg)
                enable_ctrl_c_to_debug = atoi(argv[++command_idx]);
            else if ("enable-gc-thread-count" == current_arg)
                wo::config::GC_WORKER_THREAD_COUNT = (size_t)atoi(argv[++command_idx]);
            else if ("enable-ansi-color" == current_arg)
                wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL = atoi(argv[++command_idx]);
            else if ("enable-jit" == current_arg)
                wo::config::ENABLE_JUST_IN_TIME = (bool)atoi(argv[++command_idx]);
            else if ("mem-chunk-size" == current_arg)
                wo::config::MEMORY_CHUNK_SIZE = (size_t)atoll(argv[++command_idx]);
            else if ("enable-pdb" == current_arg)
                wo::config::ENABLE_PDB_INFORMATIONS = (bool)atoi(argv[++command_idx]);
            else if ("enable-halt-when-panic" == current_arg)
                wo::config::ENABLE_HALT_WHEN_PANIC = (bool)atoi(argv[++command_idx]);
            else if ("enable-runtime-checking-integer-division" == current_arg)
                wo::config::ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = (bool)atoi(argv[++command_idx]);
            else if ("update-grammar" == current_arg)
                wo::config::ENABLE_CHECK_GRAMMAR_AND_UPDATE = (bool)atoi(argv[++command_idx]);
            else if ("ignore-not-found-extern-func" == current_arg)
                wo::config::ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL = (bool)atoi(argv[++command_idx]);
            else
                wo::wo_stderr <<
                ANSI_HIR "Woolang: " << ANSI_RST << "unknown setting --" << current_arg << wo::wo_endl;
        }
    }

    if (enable_ctrl_c_to_debug)
        wo_handle_ctrl_c(_wo_ctrl_c_signal_handler);

    wo::wstring_pool::init_global_str_pool();

    if (wo::config::GC_WORKER_THREAD_COUNT == 0)
        wo::config::GC_WORKER_THREAD_COUNT = 1; // 1 GC-thread at least.

    if (enable_std_package)
    {
        for (size_t i = 0; i < woo_embedded_file_count; ++i)
            wo_assure(woort_vfs_create(
                woo_embedded_files[i].path,
                woo_embedded_files[i].data,
                strlen(woo_embedded_files[i].data),
                false));
    }

    wo::lexer::init_char_lookup_table();

#ifndef WO_DISABLE_COMPILER
    wo::init_woolang_grammar(); // Create grammar when init.
    wo::LangContext::init_lang_processers();
#endif

    // Cache commonly-used built-in function pointers for the compiler.
    wo::rslib_extern_symbols::cache_builtin_pointers();

    // Register the "woolang/compiler" fake library for macro extern functions.
    wo::builtin_macro_lib_bootup();
}


void wo_finish(void(*do_after_shutdown)(void*), void* custom_data)
{
    wo::builtin_macro_lib_shutdown();
    woort_shutdown();

    // Ready to shutdown.
    if (do_after_shutdown != nullptr)
        do_after_shutdown(custom_data);

    wo::wstring_pool::shutdown_global_str_pool();

#ifndef WO_DISABLE_COMPILER
    wo::LangContext::shutdown_lang_processers();
    wo::shutdown_woolang_grammar();
#endif

    wo_handle_ctrl_c(nullptr);
}

void wo_enable_jit(bool option)
{
    wo::config::ENABLE_JUST_IN_TIME = option;
}

wo::compile_result _wo_compile_impl(
    const char* virtual_src_path,
    /* OPTIONAL */ const void* src_may_null,
    size_t src_len,
    const std::optional<wo::lexer*>&
    append_macro_define_to_this_lexer,
    std::optional<woort_CodeEnv*>*
    out_env_if_success,
    std::optional<std::unique_ptr<wo::lexer>>*
    out_lexer_if_failed
#ifndef WO_DISABLE_COMPILER
    , std::optional<std::unique_ptr<wo::LangContext>>*
    out_langcontext_if_pass_grammar
#endif
)
{
    // 0. Try load binary
    // WOORT_CODEENV_BINARY_MAGIC = 0x30314345u ("EC10")
    wo::compile_result compile_result = wo::compile_result::PROCESS_FAILED;

    std::optional<woort_CodeEnv*> compile_env_result = std::nullopt;
    std::unique_ptr<wo::lexer> compile_lexer;

    std::optional<woort_VFile*> source_file_instance;
    std::string real_file_path;

    if (src_may_null != nullptr)
    {
        real_file_path = virtual_src_path;
        wo::normalize_path(&real_file_path);

        woort_VFile* vfile;
        if (woort_vfile_open_reader(src_may_null, src_len, &vfile))
            source_file_instance.emplace(vfile);
    }
    else
    {
        if (wo::check_virtual_file_path(
            virtual_src_path,
            std::nullopt,
            &real_file_path))
        {
            woort_VFile* vfile;
            if (woort_vfile_open(real_file_path.c_str(), &vfile))
                source_file_instance.emplace(vfile);
        }
    }

    // Try loading binary.
    std::optional<woort_CodeEnv_RestoreResult> binary_loading_failed = std::nullopt;
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
        std::optional<std::unique_ptr<std::istream>> source_stream;
        if (source_file_instance.has_value())
        {
            source_stream.emplace(
                std::unique_ptr<std::istream>(
                    std::make_unique<wo::vfile_istream>(source_file_instance.value())));
        }

        compile_lexer = std::make_unique<wo::lexer>(
            append_macro_define_to_this_lexer,
            wo::wstring_pool::get_pstr(real_file_path),
            std::move(source_stream));

        if (binary_loading_failed.has_value())
        {
            // Is Woolang format binary, but failed to load.
            // Failed to load binary, maybe broken or version missing.
            (void)compile_lexer->record_parser_error(
                wo::lexer::msglevel_t::error,
                woort_CodeEnv_restore_failed_desc(binary_loading_failed.value()));
        }
        else
        {
            // 1. Prepare lexer..
#ifndef WO_DISABLE_COMPILER
            if (!compile_lexer->has_error())
            {
                // 2. Lexer will create ast_tree;
                auto* result = wo::get_grammar_instance()->gen(*compile_lexer);
                if (result != nullptr)
                {
                    compile_result =
                        wo::compile_result::PROCESS_FAILED_BUT_GRAMMAR_OK;

                    std::unique_ptr<wo::LangContext> lang_context =
                        std::make_unique<wo::LangContext>();

                    compile_result = lang_context->process(*compile_lexer, result);
                    if (wo::compile_result::PROCESS_OK == compile_result)
                    {
                        // Finish!, finalize the compiler.
                        compile_env_result =
                            lang_context->m_ircontext.finalize();
                    }

                    if (out_langcontext_if_pass_grammar != nullptr)
                        *out_langcontext_if_pass_grammar = std::move(lang_context);
                }
            }
#else
            (void)compile_lexer->record_parser_error(
                wo::lexer::msglevel_t::error, WO_ERR_COMPILER_DISABLED);
#endif
        }
    }
    else
        // Load binary success. 
        compile_result = wo::compile_result::PROCESS_OK;

    // Compile finished.
    if (compile_env_result.has_value())
    {
        // Success
        wo_assert(compile_result == wo::compile_result::PROCESS_OK);

        if (out_env_if_success != nullptr)
            *out_env_if_success = std::move(compile_env_result.value());
    }
    else
    {
        // Failed
        wo_assert((bool)compile_lexer);
        wo_assert(compile_result != wo::compile_result::PROCESS_OK);

        if (out_lexer_if_failed != nullptr)
            *out_lexer_if_failed = std::move(compile_lexer);
    }
    return compile_result;
}

bool _wo_compile_entry(
    const char* virtual_src_path,
    /* OPTIONAL */ const void* src_may_null,
    size_t src_len,
    const std::optional<wo::lexer*>& append_macro_define_to_this_lexer,
    std::optional<woort_CodeEnv*>* out_env_if_success,
    std::optional<std::unique_ptr<wo::lexer>>* out_lexer_if_failed)
{
    wo::start_string_pool_guard sg;

#ifndef WO_DISABLE_COMPILER
    wo::ast::AstAllocator m_last_context;
    bool need_exchange_back =
        wo::ast::AstBase::exchange_this_thread_ast(m_last_context);
#endif

    const wo::compile_result compile_result =
        _wo_compile_impl(
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
    wo::ast::AstBase::clean_this_thread_ast();

    if (need_exchange_back)
        wo::ast::AstBase::exchange_this_thread_ast(
            m_last_context);
#endif

    return compile_result == wo::compile_result::PROCESS_OK;
}

struct _wo_CompileErrors
{
    std::optional<std::unique_ptr<wo::lexer>> m_lexer;
    size_t m_current_index;
    wo_CompileErrorInfo m_current_info;

    _wo_CompileErrors(std::optional<std::unique_ptr<wo::lexer>> lex)
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
    }

    _wo_CompileErrors(const _wo_CompileErrors&) = delete;
    _wo_CompileErrors(_wo_CompileErrors&&) = delete;
    _wo_CompileErrors& operator=(const _wo_CompileErrors&) = delete;
    _wo_CompileErrors& operator=(_wo_CompileErrors&&) = delete;
};

wo_CompileErrorInfo* wo_compile_errors_next(wo_CompileErrors* errors)
{
    if (errors == nullptr)
        return nullptr;

    auto& msg_list = errors->m_lexer.value()->get_current_error_frame();
    if (errors->m_current_index >= msg_list.size())
        return nullptr;

    auto& msg = msg_list[errors->m_current_index++];
    errors->m_current_info.m_file_name = msg.m_filename.c_str();
    errors->m_current_info.m_message = msg.m_describe.c_str();
    errors->m_current_info.m_begin_row = msg.m_range_begin[0];
    errors->m_current_info.m_begin_col = msg.m_range_begin[1];
    errors->m_current_info.m_end_row = msg.m_range_end[0];
    errors->m_current_info.m_end_col = msg.m_range_end[1];
    errors->m_current_info.m_is_error =
        (msg.m_level == wo::lexer::msglevel_t::error) ? 1 : 0;

    return &errors->m_current_info;
}

void wo_compile_errors_free(wo_CompileErrors* errors)
{
    delete errors;
}

static std::string _dump_src_info(
    const std::string& path,
    const wo::lexer::compiler_message_t& errmsg,
    size_t depth,
    size_t beginaimrow,
    size_t beginpointplace,
    size_t aimrow,
    size_t pointplace,
    wo_inform_style_t style)
{
    std::string src_full_path, result;

    if (wo::check_virtual_file_path(path, std::nullopt, &src_full_path))
    {
        auto content_stream = wo::open_virtual_file_stream(src_full_path);
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
                    result += std::string(depth == 0 ? 0 : depth + 1, ' ') + buf;
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
                std::string append_result = buf;

                if (style == WO_NEED_COLOR)
                    append_result += errmsg.m_level == wo::lexer::msglevel_t::error
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
                        std::string("~\\");

                    if (style == WO_NEED_COLOR)
                        append_result += ANSI_UNDERLNE;

                    append_result +=
                        " " WO_MSG_HERE;

                    if (style == WO_NEED_COLOR)
                        append_result += ANSI_NUNDERLNE;

                    append_result += "_";

                    if (depth != 0)
                        append_result += ": " + errmsg.m_describe;
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

                if (style == WO_NEED_COLOR)
                    append_result += ANSI_RST;

                result += std::string(depth == 0 ? 0 : depth + 1, ' ') + append_result;
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
                        content_stream_ptr->clear(content_stream_ptr->rdstate() & ~std::ios_base::failbit);
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

static std::string _wo_dump_lexer_context_error(wo::lexer* lex, wo_inform_style_t style)
{
    std::string src_file_path;
    std::string _vm_compile_errors;

    size_t last_depth = 0;

    for (auto& err_info : lex->get_current_error_frame())
    {
        if (err_info.m_layer != 0)
        {
            auto see_also = last_depth >= err_info.m_layer ? WO_MSG_SEE_ALSO : WO_MSG_SEE_HERE;
            if (style == WO_NEED_COLOR)
                _vm_compile_errors
                += std::string(err_info.m_layer, ' ')
                + ANSI_HIY + see_also + ANSI_RST
                + ":\n";
            else
                _vm_compile_errors
                += std::string(err_info.m_layer, ' ')
                + see_also
                + ":\n";
        }

        last_depth = err_info.m_layer;

        if (src_file_path != err_info.m_filename)
        {
            if (style == WO_NEED_COLOR)
                _vm_compile_errors +=
                ANSI_HIR "In file: '" ANSI_RST
                + (src_file_path = err_info.m_filename)
                + ANSI_HIR "'" ANSI_RST "\n";
            else
                _vm_compile_errors +=
                "In file: '"
                + (src_file_path = err_info.m_filename)
                + "'\n";
        }

        if (err_info.m_layer == 0)
            _vm_compile_errors += err_info.to_string(style & WO_NEED_COLOR) + "\n";

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

WO_API const char* wo_get_compile_error(
    wo_CompileErrors* errors,
    wo_inform_style_t style)
{
    if (style == WO_DEFAULT)
        style = wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL ? WO_NEED_COLOR : WO_NOTHING;

    thread_local std::string _vm_compile_errors;
    _vm_compile_errors.clear();

    if (errors != nullptr)
    {
        auto* impl = reinterpret_cast<_wo_CompileErrors*>(errors);
        if (impl->m_lexer.has_value())
        {
            _vm_compile_errors += _wo_dump_lexer_context_error(
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

    static std::atomic_size_t vcount = 0;
    std::string vpath;
    if (virtual_src_path == nullptr)
        vpath = "/woolang/__runtime_script_" + std::to_string(++vcount) + "__";
    else
    {
        vpath = virtual_src_path;
        wo::normalize_path(&vpath);
    }

    if (!woort_vfs_create(vpath.c_str(), buffer, length, true))
        return nullptr;

    std::optional<woort_CodeEnv*> code_env;
    std::optional<std::unique_ptr<wo::lexer>> failed_lexer;

    bool ok = _wo_compile_entry(
        vpath.c_str(),
        buffer,
        length,
        std::nullopt,
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

    std::string vpath = virtual_src_path;
    wo::normalize_path(&vpath);

    std::optional<woort_CodeEnv*> code_env;
    std::optional<std::unique_ptr<wo::lexer>> failed_lexer;

    const bool ok = _wo_compile_entry(
        vpath.c_str(),
        nullptr,
        0,
        std::nullopt,
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
