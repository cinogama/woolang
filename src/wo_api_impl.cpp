// wo_api_impl.cpp
#include "wo_afx.hpp"

#include "wo_source_file_manager.hpp"
#include "wo_compiler_parser.hpp"
#include "wo_stdlib.hpp"
#include "wo_lang_grammar_loader.hpp"
#include "wo_crc_64.hpp"

#include "wo_ir_compiler.hpp"

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

#undef wo_init

void wo_finish(void(*do_after_shutdown)(void*), void* custom_data)
{
    woort_shutdown();

    // Ready to shutdown.
    if (do_after_shutdown != nullptr)
        do_after_shutdown(custom_data);

    wo::wstring_pool::shutdown_global_str_pool();
  
    wo::shutdown_virtual_binary();
    wo::wo_shutdown_locale_and_args();

#ifndef WO_DISABLE_COMPILER
    wo::LangContext::shutdown_lang_processers();
    wo::shutdown_woolang_grammar();
#endif
}

woort_api helloworld(void)
{
    printf("Helloworld: %s\n", woort_string(0));
    return woort_ret_void();
}

void wo_init(int argc, char** argv)
{
    bool enable_std_package = true;
    bool enable_ctrl_c_to_debug = true;
    bool enable_vm_pool = true;

    wo::wo_init_args(argc, argv);

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
            else if ("enable-vm-pool" == current_arg)
                enable_vm_pool = (bool)atoi(argv[++command_idx]);
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

    wo::wo_init_locale();
    wo::wstring_pool::init_global_str_pool();

    if (wo::config::GC_WORKER_THREAD_COUNT == 0)
        wo::config::GC_WORKER_THREAD_COUNT = 1; // 1 GC-thread at least.

    if (enable_std_package)
    {
        //wo_virtual_source(wo_stdlib_src_path, wo_stdlib_src_data, WO_FALSE);
        //wo_virtual_source(wo_stdlib_debug_src_path, wo_stdlib_debug_src_data, WO_FALSE);
        //wo_virtual_source(wo_stdlib_macro_src_path, wo_stdlib_macro_src_data, WO_FALSE);
        //if (wo::config::ENABLE_SHELL_PACKAGE)
        //    wo_virtual_source(wo_stdlib_shell_src_path, wo_stdlib_shell_src_data, WO_FALSE);
    }

    wo::lexer::init_char_lookup_table();

#ifndef WO_DISABLE_COMPILER
    wo::init_woolang_grammar(); // Create grammar when init.
    wo::LangContext::init_lang_processers();
#endif

    // Start up WooRT.
    woort_init();
}

wo_string_t wo_locale_name(void)
{
    thread_local static std::string buf;
    buf = wo::get_locale().name();
    return buf.c_str();
}
wo_string_t wo_exe_path()
{
    thread_local static std::string buf;
    buf = wo::exe_path();
    return buf.c_str();
}
void wo_set_exe_path(wo_string_t path)
{
    wo::set_exe_path(path);
}
wo_string_t wo_work_path()
{
    thread_local static std::string buf;
    buf = wo::work_path();
    return buf.c_str();
}

wo_bool_t wo_set_work_path(wo_string_t path)
{
    return WO_CBOOL(wo::set_work_path(path));
}

wo_wchar_t wo_str_get_char(wo_string_t str, wo_size_t index)
{
    return wo_strn_get_char(str, strlen(str), index);
}
const char16_t* wo_str_to_u16str(wo_string_t str)
{
    return wo_strn_to_u16str(str, strlen(str));
}
wo_string_t wo_u16str_to_str(const char16_t* str)
{
    return wo_u16strn_to_str(str, wo::u16strcount(str));
}
wo_wstring_t wo_str_to_u32str(wo_string_t str)
{
    return wo_strn_to_u32str(str, strlen(str));
}
wo_string_t wo_u32str_to_str(wo_wstring_t str)
{
    return wo_u32strn_to_str(str, wo::u32strcount(str));
}
wo_wchar_t wo_strn_get_char(wo_string_t str, wo_size_t size, wo_size_t index)
{
    size_t result_byte_len;
    const char* u8idx = wo::u8substr(
        str,
        size,
        static_cast<size_t>(index),
        &result_byte_len);

    wo_wchar_t ch;
    if (result_byte_len == 0
        || 0 == wo::u8combineu32(u8idx, result_byte_len, &ch))
    {
        wo_fail(WO_FAIL_INDEX_FAIL, "Index out of range.");
        return 0;
    }
    return ch;
}
const char16_t* wo_strn_to_u16str(wo_string_t str, wo_size_t size)
{
    static thread_local std::u16string wstr_buf;
    wstr_buf = wo::u8strtou16(str, size);

    return wstr_buf.c_str();
}
wo_string_t wo_u16strn_to_str(const char16_t* str, wo_size_t size)
{
    static thread_local std::string str_buf;
    str_buf = wo::u16strtou8(str, size);

    return str_buf.c_str();
}
wo_wstring_t wo_strn_to_u32str(wo_string_t str, wo_size_t size)
{
    static thread_local std::u32string wstr_buf;
    wstr_buf = wo::u8strtou32(str, size);

    return wstr_buf.c_str();
}
wo_string_t  wo_u32strn_to_str(wo_wstring_t str, wo_size_t size)
{
    static thread_local std::string str_buf;
    str_buf = wo::u32strtou8(str, size);

    return str_buf.c_str();
}

const wchar_t* wo_str_to_wstr(wo_string_t str)
{
    static_assert(
        sizeof(wchar_t) == sizeof(char16_t)
        || sizeof(wchar_t) == sizeof(char32_t));

    if constexpr (sizeof(wchar_t) == sizeof(char16_t))
        return reinterpret_cast<const wchar_t*>(wo_str_to_u16str(str));
    else
        return reinterpret_cast<const wchar_t*>(wo_str_to_u32str(str));
}
const wchar_t* wo_strn_to_wstr(wo_string_t str, wo_size_t size)
{
    static_assert(
        sizeof(wchar_t) == sizeof(char16_t)
        || sizeof(wchar_t) == sizeof(char32_t));

    if constexpr (sizeof(wchar_t) == sizeof(char16_t))
        return reinterpret_cast<const wchar_t*>(wo_strn_to_u16str(str, size));
    else
        return reinterpret_cast<const wchar_t*>(wo_strn_to_u32str(str, size));
}
wo_string_t wo_wstr_to_str(const wchar_t* str)
{
    static_assert(
        sizeof(wchar_t) == sizeof(char16_t)
        || sizeof(wchar_t) == sizeof(char32_t));

    if constexpr (sizeof(wchar_t) == sizeof(char16_t))
        return wo_u16str_to_str(reinterpret_cast<const char16_t*>(str));
    else
        return wo_u32str_to_str(reinterpret_cast<const char32_t*>(str));
}
wo_string_t wo_wstrn_to_str(const wchar_t* str, wo_size_t size)
{
    static_assert(
        sizeof(wchar_t) == sizeof(char16_t)
        || sizeof(wchar_t) == sizeof(char32_t));

    if constexpr (sizeof(wchar_t) == sizeof(char16_t))
        return wo_u16strn_to_str(reinterpret_cast<const char16_t*>(str), size);
    else
        return wo_u32strn_to_str(reinterpret_cast<const char32_t*>(str), size);
}

void wo_enable_jit(wo_bool_t option)
{
    wo::config::ENABLE_JUST_IN_TIME = (option != WO_FALSE);
}

wo_bool_t wo_virtual_binary(
    wo_string_t filepath,
    const void* data,
    wo_size_t len,
    wo_bool_t enable_modify)
{
    return WO_CBOOL(wo::create_virtual_binary(filepath, data, len, enable_modify != WO_FALSE));
}
wo_bool_t wo_virtual_source(wo_string_t filepath, wo_string_t data, wo_bool_t enable_modify)
{
    return wo_virtual_binary(filepath, data, strlen(data), enable_modify);
}

struct _wo_virtual_file
{
    std::string m_path;
    std::string m_buffer;

    _wo_virtual_file(const _wo_virtual_file&) = delete;
    _wo_virtual_file(_wo_virtual_file&&) = delete;
    _wo_virtual_file& operator = (const _wo_virtual_file&) = delete;
    _wo_virtual_file& operator = (_wo_virtual_file&&) = delete;

    ~_wo_virtual_file() = default;
    _wo_virtual_file() = default;
};

wo_virtual_file_t wo_open_virtual_file(wo_string_t filepath)
{
    _wo_virtual_file* vfinstance = new _wo_virtual_file;

    if (wo::check_and_read_virtual_source(
        filepath,
        std::nullopt,
        &vfinstance->m_path,
        &vfinstance->m_buffer))
    {
        return vfinstance;
    }

    delete vfinstance;
    return nullptr;
}
wo_string_t wo_virtual_file_path(wo_virtual_file_t file)
{
    return file->m_path.c_str();
}
const void* wo_virtual_file_data(wo_virtual_file_t file, size_t* len)
{
    *len = file->m_buffer.size();
    return file->m_buffer.c_str();
}
void wo_close_virtual_file(wo_virtual_file_t file)
{
    delete file;
}

wo_bool_t wo_remove_virtual_file(wo_string_t filepath)
{
    return WO_CBOOL(wo::remove_virtual_binary(filepath));
}

struct _wo_virtual_file_iter
{
    std::vector<std::string> m_paths;
    size_t m_index;
};
wo_virtual_file_iter_t wo_open_virtual_file_iter(void)
{
    auto* result = new _wo_virtual_file_iter();
    result->m_index = 0;

    auto paths = wo::get_all_virtual_file_path();
    for (auto& p : paths)
    {
        result->m_paths.push_back(p);
    }
    return result;
}
wo_string_t wo_next_virtual_file_iter(wo_virtual_file_iter_t iter)
{
    if (iter->m_index >= iter->m_paths.size())
    {
        wo_assert(iter->m_index == iter->m_paths.size());
        return nullptr;
    }
    return iter->m_paths[iter->m_index++].c_str();
}
void wo_close_virtual_file_iter(wo_virtual_file_iter_t iter)
{
    delete iter;
}

wo::compile_result _wo_compile_impl(
    wo_string_t virtual_src_path,
    const void* src,
    size_t      len,
    const std::optional<wo::lexer*>& parent_lexer,
    std::optional<woort_CodeEnv*>* out_env_if_success,
    std::optional<std::unique_ptr<wo::lexer>>* out_lexer_if_failed
#ifndef WO_DISABLE_COMPILER
    , std::optional<std::unique_ptr<wo::LangContext>>* out_langcontext_if_pass_grammar
#endif
)
{
    // 0. Try load binary
    const char* load_binary_failed_reason = nullptr;
    bool is_valid_binary = false;

    wo::compile_result compile_result = wo::compile_result::PROCESS_FAILED;

    std::optional<woort_CodeEnv*> compile_env_result = std::nullopt;
        //wo::runtime_env::load_create_env_from_binary(
        //    virtual_src_path, src, len,
        //    &load_binary_failed_reason,
        //    &is_valid_binary);
    std::unique_ptr<wo::lexer> compile_lexer;

    if (!compile_env_result.has_value())
    {
        std::string wvspath = virtual_src_path;
        if (is_valid_binary)
        {
            // Is Woolang format binary, but failed to load.
            // Failed to load binary, maybe broken or version missing.
            wo_assert(load_binary_failed_reason != nullptr);

            compile_lexer = std::make_unique<wo::lexer>(
                parent_lexer,
                wo::wstring_pool::get_pstr(wvspath),
                std::make_unique<std::istringstream>(std::string()));

            (void)compile_lexer->record_parser_error(
                wo::lexer::msglevel_t::error,
                load_binary_failed_reason);
        }
        else
        {
            // 1. Prepare lexer..
            if (src != nullptr)
            {
                // Load from virtual source.
                wo::normalize_path(&wvspath);

                compile_lexer = std::make_unique<wo::lexer>(
                    parent_lexer,
                    wo::wstring_pool::get_pstr(wvspath),
                    std::make_unique<std::istringstream>(std::string((const char*)src, len)));
            }
            else
            {
                // Load from real file.
                std::string real_file_path;

                std::optional<std::unique_ptr<std::istream>> content_stream =
                    std::nullopt;

                if (wo::check_virtual_file_path(
                    wvspath,
                    std::nullopt,
                    &real_file_path))
                {
                    content_stream =
                        wo::open_virtual_file_stream(real_file_path);
                }

                compile_lexer = std::make_unique<wo::lexer>(
                    parent_lexer,
                    wo::wstring_pool::get_pstr(real_file_path),
                    std::move(content_stream));
            }

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
                            lang_context->m_ircontext.c().finalize();
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
//
//wo_bool_t _wo_load_source(
//    wo_vm vm,
//    wo_string_t virtual_src_path,
//    const void* src,
//    size_t len,
//    const std::optional<wo::lexer*>& parent_lexer)
//{
//    wo::start_string_pool_guard sg;
//
//    std::optional<woort_CodeEnv*> _env_if_success;
//    std::optional<std::unique_ptr<wo::lexer>> _lexer_if_failed;
//
//#ifndef WO_DISABLE_COMPILER
//    wo::ast::AstAllocator m_last_context;
//    bool need_exchange_back =
//        wo::ast::AstBase::exchange_this_thread_ast(m_last_context);
//#endif
//
//    auto compile_result =
//        _wo_compile_impl(
//            virtual_src_path,
//            src,
//            len,
//            parent_lexer,
//            &_env_if_success,
//            &_lexer_if_failed
//#ifndef WO_DISABLE_COMPILER
//            , nullptr
//#endif
//        );
//
//#ifndef WO_DISABLE_COMPILER
//    wo::ast::AstBase::clean_this_thread_ast();
//
//    if (need_exchange_back)
//        wo::ast::AstBase::exchange_this_thread_ast(
//            m_last_context);
//#endif
//
//    if (compile_result == wo::compile_result::PROCESS_OK)
//    {
//        WO_VM(vm)->init_main_vm(_env_if_success.value());
//        return WO_TRUE;
//    }
//    else
//    {
//        wo_assert(_lexer_if_failed.has_value() && _lexer_if_failed.value()->has_error());
//
//        WO_VM(vm)->compile_failed_state = std::move(_lexer_if_failed);
//        return WO_FALSE;
//    }
//}
