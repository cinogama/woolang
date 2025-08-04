#pragma once
/*
In order to speed up compile and use less memory,
RS will using 'hand-work' parser, there is not yacc/bison..


* P.S. Maybe we need a LR(N) Hand work?
*/

#include "wo_compiler_lexer.hpp"
#include "wo_lang_compiler_information.hpp"
#include "wo_meta.hpp"
#include "wo_const_string_pool.hpp"

#include <variant>
#include <functional>
#include <stack>
#include <forward_list>
#include <unordered_map>
#include <map>
#include <list>

#define WOOLANG_LR1_OPTIMIZE_LR1_TABLE 1

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    namespace ast
    {
        class AstBase
        {
        public:
            enum node_type_t: uint8_t
            {
                AST_BASE,
                AST_LIST,
                AST_EMPTY,

                AST_TOKEN,
                AST_IDENTIFIER,

                AST_TEMPLATE_PARAM,
                AST_TEMPLATE_ARGUMENT,

                AST_ENUM_ITEM,
                AST_ENUM_DECLARE,
                AST_UNION_ITEM,
                AST_UNION_DECLARE,

                AST_USING_NAMESPACE,

                AST_DECLARE_ATTRIBUTE,
                AST_STRUCT_FIELD_DEFINE,

                AST_USING_TYPE_DECLARE,
                AST_ALIAS_TYPE_DECLARE,

                AST_WHERE_CONSTRAINTS,

                AST_TYPE_HOLDER,

                AST_PATTERN_begin,
                AST_PATTERN_TAKEPLACE = AST_PATTERN_begin,
                AST_PATTERN_SINGLE,
                AST_PATTERN_TUPLE,
                AST_PATTERN_UNION,  // Only used in match
                AST_PATTERN_VARIABLE, // Only used in assign
                AST_PATTERN_INDEX,  // Only used in assign
                AST_PATTERN_end,

                AST_VARIABLE_DEFINE_ITEM,
                AST_VARIABLE_DEFINES,
                AST_KEY_VALUE_PAIR,
                AST_STRUCT_FIELD_VALUE_PAIR,
                AST_FUNCTION_PARAMETER_DECLARE,

                AST_VALUE_begin,
                AST_VALUE_NOTHING = AST_VALUE_begin,
                AST_VALUE_MARK_AS_MUTABLE,
                AST_VALUE_MARK_AS_IMMUTABLE,
                AST_VALUE_LITERAL,
                AST_VALUE_TYPEID,
                AST_VALUE_TYPE_CAST,
                AST_VALUE_DO_AS_VOID,
                AST_VALUE_TYPE_CHECK_IS,
                AST_VALUE_TYPE_CHECK_AS,
                AST_VALUE_VARIABLE,
                AST_VALUE_FUNCTION_CALL,
                AST_VALUE_BINARY_OPERATOR,
                AST_VALUE_UNARY_OPERATOR,
                AST_VALUE_TRIBLE_OPERATOR,
                AST_VALUE_INDEX,
                AST_VALUE_FUNCTION,
                AST_VALUE_ARRAY_OR_VEC,
                AST_VALUE_DICT_OR_MAP,
                AST_VALUE_TUPLE,
                AST_VALUE_STRUCT,
                AST_VALUE_ASSIGN,
                AST_VALUE_MAKE_UNION,
                AST_VALUE_VARIADIC_ARGUMENTS_PACK,
                AST_VALUE_IR_OPNUM,
                AST_FAKE_VALUE_UNPACK,
                AST_VALUE_end,

                AST_NAMESPACE,
                AST_SCOPE,

                AST_MATCH,
                AST_MATCH_CASE,
                AST_IF,
                AST_WHILE,
                AST_FOR,
                AST_FOREACH,
                AST_DEFER,

                AST_BREAK,
                AST_CONTINUE,
                AST_RETURN,
                AST_LABELED,

                AST_EXTERN_INFORMATION,

                AST_NOP,

                //////////////////////////////////////////////////////

                AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_A,
                AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_B,

                AST_TEMPLATE_CONSTANT_TYPE_CHECK_IN_PASS1,
            };
        private:
            inline thread_local static std::forward_list<AstBase*>* list = nullptr;
        public:
            static void clean_this_thread_ast()
            {
                if (nullptr == list)
                    return;

                for (auto astnode : *list)
                    delete astnode;

                delete list;
                list = nullptr;
            }
            static bool exchange_this_thread_ast(std::forward_list<AstBase*>& out_list)
            {
                wo_assert(out_list.empty() || nullptr == list || list->empty());

                bool need_swap_back = true;
                if (!list)
                    list = new std::forward_list<AstBase*>;

                out_list.swap(*list);
                return !out_list.empty();
            }

        public:
            struct location_t
            {
                size_t row;
                size_t column;

                bool operator < (const location_t& another_loc) const
                {
                    if (row < another_loc.row)
                        return true;
                    else if (row == another_loc.row)
                        return column < another_loc.column;
                    else
                        return false;
                }
            };
            struct source_location_t
            {
                location_t begin_at;
                location_t end_at;
                wo_pstring_t source_file;
            };

            const node_type_t node_type;
            uint8_t           finished_state;
            bool              duplicated_node;
            source_location_t source_location;

            AstBase(node_type_t ty)
                : node_type(ty)
                , finished_state(0)
                , duplicated_node(false)
                , source_location{}
            {
                if (!list)
                    list = new std::forward_list<AstBase*>;

                list->push_front(this);
            }
            AstBase(const AstBase&) = delete;
            AstBase(AstBase&&) = delete;
            AstBase& operator = (const AstBase&) = delete;
            AstBase& operator = (AstBase&&) = delete;

            virtual ~AstBase() = default;

        public:
            class _AstSafeHolderBase
            {
            public:
                virtual ~_AstSafeHolderBase() = default;
                virtual const AstBase* get() = 0;
                virtual void set(AstBase* ast) = 0;
            };
            template<typename T>
            class SafeHolder : public _AstSafeHolderBase
            {
                static_assert(std::is_base_of<AstBase, T>::value);

                T** m_ptr;
            public:
                explicit SafeHolder(T** location)
                    : m_ptr(location)
                {
                }
                virtual const AstBase* get() override
                {
                    return *m_ptr;
                }
                virtual void set(AstBase* ast) override
                {
                    *m_ptr = static_cast<T*>(ast);
                }
            };

            template<typename T>
            static std::unique_ptr<SafeHolder<T>> make_holder(T** location)
            {
                wo_assert(location != nullptr && *location != nullptr);
                return std::make_unique<SafeHolder<T>>(location);
            }

            using ContinuesList = std::list<std::unique_ptr<_AstSafeHolderBase>>;
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const = 0;

            AstBase* clone() const
            {
                ContinuesList continues;

                AstBase* result = make_dup(std::nullopt, continues);
                result->source_location = source_location;
                result->duplicated_node = true;

                while (!continues.empty())
                {
                    auto holder = std::move(continues.front());
                    continues.pop_front();

                    auto* origin_holder_ast = holder->get();
                    auto* duped_holder_ast = origin_holder_ast->make_dup(std::nullopt, continues);
                    duped_holder_ast->source_location = origin_holder_ast->source_location;
                    duped_holder_ast->duplicated_node = true;
                    holder->set(duped_holder_ast);
                }

                return result;
            }
        };
        class AstList : public AstBase
        {
        public:
            std::list<AstBase*> m_list;
            AstList() : AstBase(AST_LIST)
            {
            }
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override
            {
                auto* new_instance = exist_instance ? static_cast<AstList*>(exist_instance.value()) : new AstList();
                
                new_instance->m_list = m_list;

                for (auto*& dup_child : new_instance->m_list)
                    out_continues.push_back(make_holder(&dup_child));

                return new_instance;
            }
        };
        class AstNop : public AstBase
        {
        public:
            AstNop()
                : AstBase(AST_NOP)
            {
            }
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override
            {
                auto* new_instance = exist_instance
                    ? static_cast<AstNop*>(exist_instance.value())
                    : new AstNop();

                return new_instance;
            }
        };
    }

    struct token
    {
        lex_type type;
        std::string identifier;
    };

    inline std::ostream& operator << (std::ostream& os, const token& tk)
    {
        os 
            << "{ token: " 
            << (lex_type_base_t)tk.type 
            << "    , \"" 
            << tk.identifier
            << "\"";

        if (tk.type == lex_type::l_error)
            os << "(error)";

        os << " }";
        return os;
    }

    // Store this ORGIN_P, LR1_TABLE and FOLLOW_SET after compile.
    struct grammar
    {
        struct terminal
        {
            lex_type t_type;
            std::string t_name;

            terminal(lex_type type, const std::string& name = "") :
                t_type(type),
                t_name(name)
            {

            }

            bool operator <(const terminal& n)const
            {
                if (t_name == "" || n.t_name == "")
                    return t_type < n.t_type;
                if (t_type != n.t_type)
                    return t_type < n.t_type;
                return t_name < n.t_name;
            }
            bool operator ==(const terminal& n)const
            {
                if (t_name == "" || n.t_name == "")
                    return t_type == n.t_type;
                return t_type == n.t_type && t_name == n.t_name;
            }
        };

        using te = terminal;
        struct nonterminal;
        using nt = nonterminal;
        using symbol = std::variant<te, nt>;
        using sym = symbol;
        using rule = std::pair< nonterminal, std::vector<sym>>;
        using symlist = std::vector<sym>;
        using ttype = lex_type;//just token

        struct hash_symbol
        {
            size_t operator ()(const sym& smb) const noexcept
            {
                if (std::holds_alternative<te>(smb))
                {
                    return ((size_t)std::get<te>(smb).t_type) * 2;
                }
                else
                {
                    const static auto wstrhasher = std::hash<std::string>();
                    return wstrhasher(std::get<nt>(smb).nt_name) * 2 + 1;
                }
            }
        };

        struct produce
        {
            std::variant<ast::AstBase*, token> m_token_or_ast;
            produce(ast::AstBase* ast)
            {
                m_token_or_ast = ast;
            }
            produce(const token& token)
            {
                m_token_or_ast = token;
            }

            produce() = default;
            produce(const produce&) = default;
            produce(produce&&) = default;
            produce& operator = (const produce&) = default;
            produce& operator = (produce&&) = default;

            bool is_ast() const
            {
                return std::holds_alternative<ast::AstBase*>(m_token_or_ast);
            }
            bool is_token() const
            {
                return std::holds_alternative<token>(m_token_or_ast);
            }
            const token& read_token() const
            {
                if (! is_token())
                    wo_error("read_token() called on an ast node.");
                return std::get<token>(m_token_or_ast);
            }
            ast::AstBase* read_ast() const
            {
                if (! is_ast())
                    wo_error("read_ast() called on a token.");

                return std::get<ast::AstBase*>(m_token_or_ast);
            }
            ast::AstBase* read_ast(ast::AstBase::node_type_t required) const
            {
                auto* ast_node = read_ast();
                if (required != ast_node->node_type)
                    wo_error("read_ast() called on a wrong type of ast node.");

                return ast_node;
            }
            ast::AstBase* read_ast_value() const
            {
                auto* ast_node = read_ast();
                if (ast_node->node_type < ast::AstBase::AST_VALUE_begin || ast_node->node_type >= ast::AstBase::AST_VALUE_end)
                    wo_error("read_ast_value() called on a wrong type of ast node.");
                
                return ast_node;
            }
            ast::AstBase* read_ast_pattern() const
            {
                auto* ast_node = read_ast();
                if (ast_node->node_type < ast::AstBase::AST_PATTERN_begin || ast_node->node_type >= ast::AstBase::AST_PATTERN_end)
                    wo_error("read_ast_pattern() called on a wrong type of ast node.");

                return ast_node;
            }
        };

        struct AstEmpty : public ast::AstBase
        {
            // used for stand fro l_empty
            // some passer will ignore this xx

            AstEmpty() : AstBase(AST_EMPTY)
            {
            }

            static bool is_empty(const produce& p)
            {
                if (p.is_ast())
                {
                    if (p.read_ast()->node_type == AST_EMPTY)
                        return true;
                }
                if (p.is_token())
                {
                    if (p.read_token().type == lex_type::l_empty)
                        return true;
                }

                return false;
            }

            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final
            {
                // Immutable and no state to be modified.
                return const_cast<AstEmpty*>(this);
            }
        };

        struct nonterminal
        {
            std::string nt_name;

            size_t builder_index = 0;
            std::function<produce(lexer&, std::vector<produce>&)> ast_create_func;

            nonterminal(const std::string& name = "", size_t _builder_index = 0)
                : nt_name(name)
                , builder_index(_builder_index)
            {
            }
            bool operator <(const nonterminal& n)const
            {
                return nt_name < n.nt_name;
            }
            bool operator ==(const nonterminal& n)const
            {
                return nt_name == n.nt_name;
            }

            template<typename T>
            rule operator >>(const std::vector<T>& plist)
            {
                return std::make_pair(*this, plist);
            }
        };

        std::map< nonterminal, std::vector<symlist>> P;//produce rules
        std::vector<rule> ORGIN_P;//origin produce rules
        nonterminal S;//begin nt, or final nt

        struct sym_less {
            constexpr bool operator()(const sym& _Left, const sym& _Right) const {

                bool left_is_te = std::holds_alternative<te>(_Left);
                if (left_is_te == std::holds_alternative<te>(_Right))
                {
                    // both te or non-te
                    if (left_is_te)
                    {
                        return  std::get<te>(_Left) < std::get<te>(_Right);
                    }
                    else
                    {
                        return  std::get<nt>(_Left) < std::get<nt>(_Right);
                    }
                }

                //te < nt
                return !left_is_te;
            }
        };
        using sym_nts_t = std::map< sym, std::set< te>, sym_less>;

        sym_nts_t FIRST_SET;
        sym_nts_t FOLLOW_SET;

        struct lr_item
        {
            rule item_rule;
            size_t next_sign = 0;//		

            te prospect;

            bool operator <(const lr_item& another) const
            {
                if (item_rule.first == another.item_rule.first)
                {
                    if (item_rule.second == another.item_rule.second)
                    {
                        if (next_sign == another.next_sign)
                        {
                            return prospect < another.prospect;
                        }
                        else
                        {
                            return next_sign < another.next_sign;
                        }
                    }
                    else
                    {
                        return item_rule.second < another.item_rule.second;
                    }
                }
                else
                {
                    return item_rule.first < another.item_rule.first;
                }
            }
            bool operator ==(const lr_item& another) const
            {
                return
                    item_rule.first == another.item_rule.first &&
                    item_rule.second == another.item_rule.second &&
                    next_sign == another.next_sign &&
                    prospect == another.prospect;
            }
            bool operator !=(const lr_item& another) const
            {
                return !(*this == another);
            }
        };

        struct lr0_item
        {
            rule item_rule;
            size_t next_sign = 0;//		

            bool operator <(const lr0_item& another) const
            {
                if (item_rule.first == another.item_rule.first)
                {
                    if (item_rule.second == another.item_rule.second)
                    {
                        return next_sign < another.next_sign;
                    }
                    else
                    {
                        return item_rule.second < another.item_rule.second;
                    }
                }
                else
                {
                    return item_rule.first < another.item_rule.first;
                }
            }
            bool operator ==(const lr0_item& another) const
            {
                return
                    item_rule.first == another.item_rule.first &&
                    item_rule.second == another.item_rule.second &&
                    next_sign == another.next_sign;
            }
            bool operator !=(const lr0_item& another) const
            {
                return !(*this == another);
            }


        };

        std::map<std::set<lr_item>, std::set<lr_item>>CLOSURE_SET;
        std::vector<std::set<lr_item>> C_SET;

        using te_nt_index_t = signed int;

        std::set<te>& FIRST(const sym& _sym);
        std::set<te>& FOLLOW(const nt& noterim);
        const std::set<te>& FOLLOW_READ(const nt& noterim)const;
        const std::set<te>& FOLLOW_READ(te_nt_index_t ntidx)const;

        std::set<lr_item>& CLOSURE(const std::set<lr_item>& i);
        std::set<lr_item>& GOTO(const std::set<lr_item>& i, const sym& X);

        std::set<te> P_TERMINAL_SET;
        std::set<nt> P_NOTERMINAL_SET;

        std::unordered_map<std::string, te_nt_index_t> NONTERM_MAP;
        std::unordered_map<lex_type, te_nt_index_t> TERM_MAP;

        struct action/*_lr1*/
        {
            enum class act_type
            {
                error,
                state_goto,
                push_stack,
                reduction,
                accept,

            }act;
            te prospect;
            size_t state;

            action(act_type _type = act_type::error,
                te _prospect = { lex_type::l_error },
                size_t _state = (size_t)(-1))
                :act(_type)
                , prospect(_prospect)
                , state(_state)
            {

            }

            bool operator<(const action& another)const
            {
                if (act == another.act)
                {
                    if (state == another.state)
                    {
                        return prospect < another.prospect;
                    }
                    else
                    {
                        return state < another.state;
                    }
                }
                return act < another.act;
            }
        };
        struct no_prospect_action
        {
            action::act_type act;
            size_t state;
        };
        using lr1table_t = std::unordered_map<size_t, std::unordered_map<sym, std::set<action>, hash_symbol>>;
        struct rt_rule
        {
            te_nt_index_t production_aim;
            size_t rule_right_count;
            std::string rule_left_name;
            std::function<produce(lexer&, std::vector<produce>&)> ast_create_func;
        };
#if 0
        lr1table_t LR0_TABLE;
#endif
        lr1table_t LR1_TABLE;

        std::vector<std::vector<action>> RT_LR1_TABLE; // te_nt_index_t
        std::vector<rt_rule> RT_PRODUCTION;

#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE

        const int* LR1_GOTO_CACHE = nullptr;
        const int* LR1_R_S_CACHE = nullptr;

        const int(*LR1_GOTO_RS_MAP)[2] = nullptr;

        size_t LR1_GOTO_CACHE_SZ = 0;
        size_t LR1_R_S_CACHE_SZ = 0;

        size_t LR1_ACCEPT_STATE = 0;
        te_nt_index_t LR1_ACCEPT_TERM = 0;

        const lex_type* LR1_TERM_LIST = nullptr;
        const char** LR1_NONTERM_LIST = nullptr;

        bool lr1_fast_cache_enabled() const noexcept;
#endif
        void LR1_TABLE_READ(size_t a, te_nt_index_t b, no_prospect_action* write_act) const noexcept;

        grammar();
        grammar(const std::vector<rule>& p);

        bool check_lr1(std::ostream& ostrm = std::cout);
        void finish_rt();
        ast::AstBase* gen(lexer& tkr) const;
    };
    std::ostream& operator<<(std::ostream& ost, const  grammar::lr_item& lri);
    std::ostream& operator<<(std::ostream& ost, const  grammar::terminal& ter);
    std::ostream& operator<<(std::ostream& ost, const  grammar::nonterminal& noter);
#endif
}