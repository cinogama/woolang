#pragma once
#include "wo_compiler_parser.hpp"
#include "wo_meta.hpp"
#include "wo_basic_type.hpp"
#include "wo_env_locale.hpp"
#include "wo_lang_extern_symbol_loader.hpp"
#include "wo_source_file_manager.hpp"
#include "wo_utf8.hpp"
#include "wo_memory.hpp"
#include "wo_const_string_pool.hpp"
#include "wo_crc_64.hpp"

#include <type_traits>
#include <cmath>
#include <unordered_map>
#include <algorithm>

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    namespace ast
    {
        struct astnode_builder
        {
            using ast_basic = wo::ast::AstBase;
            using inputs_t = std::vector<grammar::produce>;
            using builder_func_t = std::function<grammar::produce(lexer&, inputs_t&)>;

            virtual ~astnode_builder() = default;
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(false, "");
                return nullptr;
            }
        };

        inline std::unordered_map<size_t, size_t> _registed_builder_function_id_list;
        inline std::vector<astnode_builder::builder_func_t> _registed_builder_function;

        template <typename T>
        size_t _register_builder()
        {
            static_assert(std::is_base_of<astnode_builder, T>::value);
            _registed_builder_function.push_back(T::build);
            return _registed_builder_function.size();
        }

        template <typename T>
        size_t index()
        {
            size_t idx = _registed_builder_function_id_list[meta::type_hash<T>];
            wo_assert(idx != 0);
            return idx;
        }

        inline astnode_builder::builder_func_t get_builder(size_t idx)
        {
            wo_assert(idx != 0);
            return _registed_builder_function[idx - 1];
        }
    }

    namespace ast
    {
        void init_builder();
    }

    grammar::rule operator >> (grammar::rule ost, size_t builder_index);
#endif
}
