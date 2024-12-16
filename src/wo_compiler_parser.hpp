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
#include <queue>
#include <stack>
#include <sstream>
#include <forward_list>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <list>

#define WOOLANG_LR1_OPTIMIZE_LR1_TABLE 1

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    struct lang_scope;
    namespace ast
    {
        class AstBase
        {
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
                {
                    list = new std::forward_list<AstBase*>;
                    need_swap_back = false;
                }
                out_list.swap(*list);
                return need_swap_back;
            }

        public:
            struct location_t
            {
                size_t row;
                size_t column;
            };

            location_t begin_at = {};
            location_t end_at = {};

            wo_pstring_t source_file = nullptr;

            AstBase()
            {
                if (!list)
                    list = new std::forward_list<AstBase*>;

                list->push_front(this);
            }
            AstBase(const AstBase&) = delete;
            AstBase(AstBase&&) = delete;
            AstBase& operator = (const AstBase&) = delete;
            AstBase& operator = (AstBase&&) = delete;

            virtual ~AstBase()
            {
            }

        public:

            template<typename T, typename ... Args>
            static T* MAKE_INSTANCE(const T* datfrom, Args && ... args)
            {
                auto* instance = new T(args...);

                instance->begin_at = datfrom->begin_at;
                instance->end_at = datfrom->end_at;
                instance->source_file = datfrom->source_file;

                return instance;
            }

            virtual AstBase* instance(AstBase* child_instance = nullptr) const = 0;
        };
        class AstList : public AstBase
        {
        public:
            std::list<AstBase*> m_list;

            virtual AstBase* instance(AstBase* child_instance = nullptr) const
            {
                auto* new_instance = child_instance
                    ? static_cast<decltype(MAKE_INSTANCE(this))>(child_instance)
                    : MAKE_INSTANCE(this);

                // ast::AstBase::instance(new_instance);

                for (auto* old_child : m_list)
                    new_instance->m_list.push_back(old_child->instance());

                return new_instance;
            }
        };
    }

    struct token
    {
        lex_type type;
        std::wstring identifier;
    };

    inline std::wostream& operator << (std::wostream& os, const token& tk)
    {
        os << "{ token: " << (lex_type_base_t)tk.type << "    , \"" << (tk.identifier) << "\"";
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
            std::wstring t_name;

            terminal(lex_type type, const std::wstring& name = L"") :
                t_type(type),
                t_name(name)
            {

            }

            bool operator <(const terminal& n)const
            {
                if (t_name == L"" || n.t_name == L"")
                    return t_type < n.t_type;
                if (t_type != n.t_type)
                    return t_type < n.t_type;
                return t_name < n.t_name;
            }
            bool operator ==(const terminal& n)const
            {
                if (t_name == L"" || n.t_name == L"")
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
                    const static auto wstrhasher = std::hash<std::wstring>();
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
                return std::get<token>(m_token_or_ast);
            }
            ast::AstBase* read_ast() const
            {
                return std::get<ast::AstBase*>(m_token_or_ast);
            }
        };

        struct AstDefault : public ast::AstBase
        {
            bool stores_terminal = false;

            token terminal_token = { lex_type::l_error };

            ast::AstBase* instance(ast::AstBase* child_instance = nullptr) const override
            {
                auto* new_instance = child_instance
                    ? static_cast<decltype(MAKE_INSTANCE(this))>(child_instance)
                    : MAKE_INSTANCE(this);

                // ast::AstBase::instance(new_instance);

                return new_instance;
            }
        };

        struct AstEmpty : public ast::AstBase
        {
            // used for stand fro l_empty
            // some passer will ignore this xx

            static bool is_empty(const produce& p)
            {
                if (p.is_ast())
                {
                    if (dynamic_cast<AstEmpty*>(p.read_ast()))
                        return true;
                }
                if (p.is_token())
                {
                    if (p.read_token().type == lex_type::l_empty)
                        return true;
                }

                return false;
            }
            ast::AstBase* instance(AstBase* child_instance = nullptr) const override
            {
                auto* new_instance = child_instance
                    ? static_cast<decltype(MAKE_INSTANCE(this))>(child_instance)
                    : MAKE_INSTANCE(this);

                // ast::AstBase::instance(new_instance);

                return new_instance;
            }
        };

        struct nonterminal
        {
            std::wstring nt_name;

            size_t builder_index = 0;
            std::function<produce(lexer&, std::vector<produce>&)> ast_create_func;

            nonterminal(const std::wstring& name = L"", size_t _builder_index = 0)
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

        std::map<std::wstring, te_nt_index_t> NONTERM_MAP;
        std::map<lex_type, te_nt_index_t> TERM_MAP;

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
            std::wstring rule_left_name;
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
        const wchar_t** LR1_NONTERM_LIST = nullptr;

        bool lr1_fast_cache_enabled() const noexcept;
#endif
        void LR1_TABLE_READ(size_t a, te_nt_index_t b, no_prospect_action* write_act) const noexcept;

        grammar();
        grammar(const std::vector<rule>& p);

        bool check_lr1(std::wostream& ostrm = std::wcout);
        void finish_rt();
        ast::AstBase* gen(lexer& tkr) const;
    };
    std::wostream& operator<<(std::wostream& ost, const  grammar::lr_item& lri);
    std::wostream& operator<<(std::wostream& ost, const  grammar::terminal& ter);
    std::wostream& operator<<(std::wostream& ost, const  grammar::nonterminal& noter);
#endif
}