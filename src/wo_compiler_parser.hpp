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
#include <any>
#include <forward_list>
#include <unordered_map>
#include <unordered_set>
#include <map>

#define WOOLANG_LR1_OPTIMIZE_LR1_TABLE 1

namespace wo
{
    struct token
    {
        lex_type type;
        std::wstring identifier;
    };

    inline std::wostream& operator << (std::wostream& os, const token& tk)
    {
        os << "{ " << tk.type._to_string() << "    , \"" << (tk.identifier) << "\"";
        if (tk.type == +lex_type::l_error)
            os << "(error)";
        os << " }";
        return os;
    }

    template<typename T>
    inline bool cast_any_to(std::any& any_val, T& out_value)
    {
        if (any_val.type() == typeid(T))
        {
            out_value = std::any_cast<T>(any_val);
            return true;
        }
        else
        {
            return false;
        }
    }

    // Store this ORGIN_P, LR1_TABLE and FOLLOW_SET after compile.
    struct grammar
    {
        struct terminal
        {
            lex_type t_type;
            std::wstring t_name;

            friend std::wostream& operator<<(std::wostream& ost, const  grammar::terminal& ter);

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

        class ast_base
        {
        private:
            inline thread_local static std::forward_list<ast_base*>* list = nullptr;
        public:

            ast_base& operator = (const ast_base& another)
            {
                // ATTENTION: DO NOTHING HERE, DATA COPY WILL BE FINISHED
                //            IN 'MAKE_INSTANCE'
                return *this;
            }
            ast_base& operator = (ast_base&& another) = delete;

            static void clean_this_thread_ast()
            {
                if (nullptr == list)
                    return;

                for (auto astnode : *list)
                    delete astnode;

                delete list;
                list = nullptr;
            }
            static bool exchange_this_thread_ast(std::forward_list<ast_base*>& out_list)
            {
                wo_assert(out_list.empty() || nullptr == list || list->empty());

                if (!list)
                    list = new std::forward_list<ast_base*>;

                out_list.swap(*list);
                return true;

                return false;
            }

            bool completed_in_pass2 = false;

            ast_base* parent;
            ast_base* children;
            ast_base* sibling;
            ast_base* last;

            size_t row_begin_no, row_end_no;
            size_t col_begin_no, col_end_no;
            wo_pstring_t source_file = nullptr;

            wo_pstring_t marking_label = nullptr;

            virtual ~ast_base()
            {

            }
            ast_base(const ast_base& another) = delete;
            ast_base(ast_base&& another) = delete;
            ast_base()
                : parent(nullptr)
                , children(nullptr)
                , sibling(nullptr)
                , last(nullptr)
                , row_begin_no(0)
                , row_end_no(0)
                , col_begin_no(0)
                , col_end_no(0)
            {
                if (!list)
                    list = new std::forward_list<ast_base*>;
                list->push_front(this);
            }
            void remove_allnode()
            {
                last = nullptr;
                while (children)
                {
                    children->parent = nullptr;

                    // NOTE: DO NOT CLEAN CHILD'S SIB HERE, THIS FUNCTION JUST FOR
                    //       PICKING OUT ALL CHILD NODES.
                    children = children->sibling;
                }
            }
            void add_child(ast_base* ast_node)
            {
                if (ast_node->parent == this)
                    return;

                wo_test(ast_node->parent == nullptr);
                wo_test(ast_node->sibling == nullptr);

                ast_node->parent = this;
                if (!children)last = nullptr;
                if (!last)
                {
                    children = last = ast_node;
                    return;
                }

                last->sibling = ast_node;
                last = ast_node;

            }
            void remove_child(ast_base* ast_node)
            {
                // WARNING: THIS FUNCTION HAS NOT BEEN TEST.
                ast_base* last_childs = nullptr;
                ast_base* childs = children;
                while (childs)
                {
                    if (ast_node == childs)
                    {
                        if (last_childs)
                        {
                            last_childs->sibling = childs->sibling;
                            ast_node->parent = nullptr;
                            ast_node->sibling = nullptr;
                        }
                        else
                        {
                            if (children == ast_node)
                                children = ast_node->sibling;

                            childs = last_childs;
                        }

                        if (last == ast_node)
                            last = last_childs;

                        return;
                    }

                    last_childs = childs;
                    childs = childs->sibling;
                }

                wo_error("There is no such a child node.");
            }
            void copy_source_info(const ast_base* ast_node)
            {
                if (ast_node->row_begin_no)
                {
                    row_begin_no = ast_node->row_begin_no;
                    row_end_no = ast_node->row_end_no;
                    col_begin_no = ast_node->col_begin_no;
                    col_end_no = ast_node->col_end_no;
                    source_file = ast_node->source_file;
                }
                else if (ast_node->parent)
                    copy_source_info(ast_node->parent);
                else
                    wo_assert(0, "Failed to copy source info.");
            }
        public:
            static void space(std::wostream& os, size_t layer)
            {
                for (size_t i = 0; i < layer; i++)
                {
                    os << "  ";
                }
            }
            virtual void display(std::wostream& os = std::wcout, size_t lay = 0) const
            {
                space(os, lay); os << L"<ast_base from " << typeid(*this).name() << L">" << std::endl;
            }

            template<typename T, typename ... Args>
            static T* MAKE_INSTANCE(const T* datfrom, Args && ... args)
            {
                auto* instance = new T(args...);

                instance->row_begin_no = datfrom->row_begin_no;
                instance->col_begin_no = datfrom->col_begin_no;
                instance->row_end_no = datfrom->row_end_no;
                instance->col_end_no = datfrom->col_end_no;
                instance->source_file = datfrom->source_file;
                instance->marking_label = datfrom->marking_label;

                auto* fromchild = datfrom->children;
                while (fromchild)
                {
                    auto* child = fromchild->instance();

                    child->last = child->parent = nullptr;

                    instance->add_child(child);

                    fromchild = fromchild->sibling;
                }

                return instance;
            }

            virtual ast_base* instance(ast_base* child_instance = nullptr) const = 0;
        };

        struct ast_default :virtual public ast_base
        {
            bool stores_terminal = false;

            std::wstring nonterminal_name;
            token        terminal_token = { lex_type::l_error };

            ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_base::instance(dumm);

                // Write self copy functions here..

                return dumm;
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                if (stores_terminal)
                {
                    space(os, lay); os << "<ast_default: " << (terminal_token) << ">" << std::endl;
                }
                else
                {
                    space(os, lay); os << "<ast_default: " << (nonterminal_name) << ">" << std::endl;
                }

                auto* mychild = children;
                while (mychild)
                {
                    mychild->display(os, lay + 1);

                    mychild = mychild->sibling;
                }

            }
        };

        struct ast_empty : virtual public ast_base
        {
            // used for stand fro l_empty
            // some passer will ignore this xx

            static bool is_empty(std::any& any)
            {
                if (grammar::ast_base* _node; cast_any_to<grammar::ast_base*>(any, _node))
                {
                    if (dynamic_cast<ast_empty*>(_node))
                    {
                        return true;
                    }
                }
                if (token _node = { lex_type::l_error }; cast_any_to<token>(any, _node))
                {
                    if (_node.type == +lex_type::l_empty)
                    {
                        return true;
                    }
                }

                return false;
            }
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                /*display nothing*/
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value_symbolable_base::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct nonterminal
        {
            std::wstring nt_name;

            size_t builder_index = 0;
            std::function<std::any(lexer&, const std::wstring&, std::vector<std::any>&)> ast_create_func =
                [](lexer& lex, const std::wstring& name, std::vector<std::any>& chs)->std::any
            {
                auto defaultAST = new ast_default;// <grammar::ASTDefault>();
                defaultAST->nonterminal_name = name;

                for (auto& any_value : chs)
                {
                    if (ast_base* child_ast; cast_any_to<ast_base*>(any_value, child_ast))
                    {
                        defaultAST->add_child(child_ast);
                    }
                    else if (token child_token = { lex_type::l_error }; cast_any_to<token>(any_value, child_token))
                    {
                        auto teAST = new ast_default;// <grammar::ASTDefault>();
                        teAST->terminal_token = child_token;
                        teAST->stores_terminal = true;

                        defaultAST->add_child(teAST);
                    }
                    else
                    {
                        return lex.parser_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_AST_NODE_TYPE);
                    }
                }
                return (ast_base*)defaultAST;
            };

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

            friend std::wostream& operator<<(std::wostream& ost, const  grammar::nonterminal& noter);

            template<typename T>
            rule operator >>(const std::vector<T>& plist)
            {
                return std::make_pair(*this, plist);
            }
        };

        std::map< nonterminal, std::vector<symlist>>P;//produce rules
        std::vector<rule>ORGIN_P;//origin produce rules
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

            friend inline std::wostream& operator<<(std::wostream& ost, const  grammar::lr_item& lri);
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

            friend std::wostream& operator<<(std::wostream& ost, const  grammar::action& act);
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
            std::function<std::any(lexer&, const std::wstring&, std::vector<std::any>&)> ast_create_func;
        };

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
        void display(std::wostream& ostrm = std::wcout);
        bool check(lexer& tkr);
        ast_base* gen(lexer& tkr) const;
    };

    std::wostream& operator<<(std::wostream& ost, const  grammar::lr_item& lri);
    std::wostream& operator<<(std::wostream& ost, const  grammar::terminal& ter);
    std::wostream& operator<<(std::wostream& ost, const  grammar::nonterminal& noter);
    std::wostream& operator<<(std::wostream& ost, const  grammar::action& act);
}