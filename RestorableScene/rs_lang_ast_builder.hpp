#pragma once
#include "rs_compiler_parser.hpp"
#include "rs_meta.hpp"

#include <any>
#include <type_traits>

namespace rs
{
    namespace ast
    {
        struct astnode_builder
        {
            using ast_basic = rs::grammar::ast_base;
            using inputs_t = std::vector<std::any>;
            using builder_func_t = std::function<std::any(const std::wstring&, inputs_t&)>;

            virtual ~astnode_builder() = default;
            static std::any build(const std::wstring& name, inputs_t& input)
            {
                rs_test(false, "");
                return nullptr;
            }
        };

        inline static std::unordered_map<size_t, size_t> _registed_builder_function_id_list;
        inline static std::vector<astnode_builder::builder_func_t> _registed_builder_function;

        template<typename T>
        size_t _register_builder()
        {
            static_assert(std::is_base_of<astnode_builder, T>::value);

            _registed_builder_function.push_back(T::build);

            return _registed_builder_function.size();

        }

        template<typename T>
        size_t index()
        {
            size_t idx = _registed_builder_function_id_list[meta::type_hash<T>];
            rs_test(idx != 0);

            return idx;
        }

        inline astnode_builder::builder_func_t get_builder(size_t idx)
        {
            rs_test(idx != 0);

            return _registed_builder_function[idx - 1];
        }

        /////////////////////////////////////////////////////////////////////////////////

        template<size_t pass_idx>
        struct pass_direct :public astnode_builder
        {
            static std::any build(const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() > pass_idx);
                return input[pass_idx];
            }
        };

        /////////////////////////////////////////////////////////////////////////////////

        inline void init_builder()
        {
            _registed_builder_function_id_list[meta::type_hash<pass_direct<0>>]
                = _register_builder<pass_direct<0>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<1>>]
                = _register_builder<pass_direct<1>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<2>>]
                = _register_builder<pass_direct<2>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<3>>]
                = _register_builder<pass_direct<3>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<4>>]
                = _register_builder<pass_direct<4>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<5>>]
                = _register_builder<pass_direct<5>>();

        }

    }

    inline static grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }


#define RS_ASTBUILDER_INDEX(T) ast::index<T>()
}
