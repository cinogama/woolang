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

            bool completed_in_pass2 = false;

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

            ast_base* parent;
            ast_base* children;
            ast_base* sibling;
            ast_base* last;

            size_t row_begin_no, row_end_no;
            size_t col_begin_no, col_end_no;
            wo_pstring_t source_file = nullptr;

            wo_pstring_t marking_label = nullptr;

            virtual ~ast_base() = default;
            ast_base(const ast_base&) = default;
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

        struct ast_default :public ast_base
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
                        return lex.parser_error(0x0000, WO_ERR_UNEXCEPT_AST_NODE_TYPE);
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

        std::set< te>& FIRST(const sym& _sym)
        {
            if (std::holds_alternative<te>(_sym))//_sym is te
            {
                if (FIRST_SET.find(_sym) == FIRST_SET.end())
                {
                    // first time for getting te, record it
                    FIRST_SET[_sym] = { std::get<te>(_sym) };
                }
                return FIRST_SET[_sym];
            }
            return FIRST_SET[_sym];
        }
        std::set< te>& FOLLOW(const nt& noterim)
        {
            return FOLLOW_SET[noterim];
        }

        const std::set< te>& FOLLOW_READ(const nt& noterim)const
        {
            static std::set< te> empty_set;

            if (FOLLOW_SET.find(noterim) == FOLLOW_SET.end())
                return empty_set;
            return FOLLOW_SET.at(noterim);
        }
        const std::set< te>& FOLLOW_READ(te_nt_index_t ntidx)const
        {
            for (auto& [ntname, idx] : NONTERM_MAP)
            {
                if (idx == ntidx)
                    return FOLLOW_READ(nt(ntname));
            }
            // Should not be here.
            abort();
        }

        std::set<lr_item>& CLOSURE(const std::set<lr_item>& i)
        {
            auto find_result = CLOSURE_SET.find(i);
            if (find_result != CLOSURE_SET.end())
                return find_result->second;

            std::set<lr_item> j = i;

            size_t last_size = 0;
            size_t j_size = 0;
            do
            {
                last_size = j_size;

                std::set<lr_item> add_ietm;
                for (auto& item : j)
                {
                    if (item.next_sign < item.item_rule.second.size())
                    {
                        if (std::holds_alternative<nt>(item.item_rule.second[item.next_sign]))
                        {
                            //only for non-te
                            auto& ntv = std::get<nt>(item.item_rule.second[item.next_sign]);
                            auto& prules = P[ntv];

                            for (auto& prule : prules)
                            {
                                auto current_next_sign = item.next_sign;
                                bool has_e_prod = false;
                                do
                                {
                                    has_e_prod = false;
                                    ++current_next_sign;
                                    if (current_next_sign < item.item_rule.second.size())
                                    {
                                        auto& beta_set = FIRST(item.item_rule.second[current_next_sign]);
                                        auto& beta_follow = FOLLOW_READ(ntv);

                                        for (auto& beta : beta_set)
                                        {
                                            if (beta.t_type == +ttype::l_empty)
                                                has_e_prod = true;
                                            else
                                            {
                                                wo_assert(beta_follow.find(beta) != beta_follow.end());
                                                add_ietm.insert(lr_item{ rule{ntv,prule}, 0 , beta });
                                            }
                                        }
                                    }
                                    else
                                        add_ietm.insert(lr_item{ rule{ntv,prule}, 0 , item.prospect });
                                } while (has_e_prod);
                            }
                        }

                    }
                }
                j.insert(add_ietm.begin(), add_ietm.end());

                j_size = j.size();
            } while (j_size != last_size);

            return CLOSURE_SET[i] = j;
        }
        std::set<lr_item>& GOTO(const std::set<lr_item>& i, const sym& X)
        {
            std::set<lr_item> j;
            for (auto& item : i)
            {
                if (item.next_sign < item.item_rule.second.size() &&
                    item.item_rule.second[item.next_sign] == X)
                {
                    j.insert(lr_item{ item.item_rule, item.next_sign + 1,item.prospect });
                }
            }

            return CLOSURE(j);
        }


        std::set<te> P_TERMINAL_SET;
        std::set<nt> P_NOTERMINAL_SET;

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
        lr1table_t LR1_TABLE;

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

        bool lr1_fast_cache_enabled() const noexcept
        {
            if (LR1_GOTO_CACHE)
            {
                wo_assert(LR1_R_S_CACHE);
                wo_assert(LR1_GOTO_RS_MAP);
                wo_assert(LR1_GOTO_CACHE_SZ);
                wo_assert(LR1_R_S_CACHE_SZ);
                wo_assert(!NONTERM_MAP.empty());
                wo_assert(!TERM_MAP.empty());
                return true;
            }
            return false;
        }
#endif

        struct rt_rule
        {
            te_nt_index_t production_aim;
            size_t rule_right_count;
            std::wstring rule_left_name;
            std::function<std::any(lexer&, const std::wstring&, std::vector<std::any>&)> ast_create_func;
        };
        std::vector<std::vector<action>> RT_LR1_TABLE; // te_nt_index_t
        std::vector<rt_rule> RT_PRODUCTION;

        // Store this ORGIN_P, LR1_TABLE and FOLLOW_SET after compile.

        void LR1_TABLE_READ(size_t a, te_nt_index_t b, no_prospect_action* write_act) const
        {
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
            if (lr1_fast_cache_enabled())
            {
                if (b >= 0)
                {
                    // Is Terminate, b did'nt need add 1, l_error is 0 and not able to exist in cache.

                    if (a == LR1_ACCEPT_STATE && b == LR1_ACCEPT_TERM)
                    {
                        write_act->act = action::act_type::accept;
                        write_act->state = 0;
                        return;
                    }

                    if (LR1_GOTO_RS_MAP[a][1] == -1)
                    {
                        write_act->act = action::act_type::error;
                        write_act->state = -1;
                        return;
                    }

                    auto state = LR1_R_S_CACHE[LR1_GOTO_RS_MAP[a][1] * LR1_R_S_CACHE_SZ + b];
                    if (state == 0)
                    {
                        // No action for this state&te, return error.
                        write_act->act = action::act_type::error;
                        write_act->state = -1;
                        return;
                    }
                    else if (state > 0)
                    {
                        // Push!
                        write_act->act = action::act_type::push_stack;
                        write_act->state = state - 1;
                        return;
                    }
                    else
                    {
                        // Reduce!
                        write_act->act = action::act_type::reduction;
                        write_act->state = (-state) - 1;
                        return;
                    }
                }
                else
                {
                    // Is NonTerminate
                    if (LR1_GOTO_RS_MAP[a][0] == -1)
                    {
                        write_act->act = action::act_type::error;
                        write_act->state = -1;
                        return;
                    }

                    auto state = LR1_GOTO_CACHE[LR1_GOTO_RS_MAP[a][0] * LR1_GOTO_CACHE_SZ + (-b)];
                    if (state == -1)
                    {
                        // No action for this state&nt, return error.
                        write_act->act = action::act_type::error;
                        write_act->state = -1;
                        return;
                    }

                    write_act->act = action::act_type::state_goto;
                    write_act->state = state;
                    return;
                }

            }
#endif
            auto& action = RT_LR1_TABLE[a][b];
            write_act->act = action.act;
            write_act->state = action.state;
            return;
        }

        grammar()
        {
            // DONOTHING FOR READING FROM AUTO GENN
        }
        grammar(const std::vector<rule>& p)
        {
            ORGIN_P = p;
            if (p.size())
            {
                S = p[0].first;
                for (auto& pl : p)
                {
                    P[pl.first].emplace_back(pl.second);

                    for (auto& symb : pl.second)
                    {
                        if (std::holds_alternative<te>(symb))
                            P_TERMINAL_SET.insert(std::get<te>(symb));
                        else
                            P_NOTERMINAL_SET.insert(std::get<nt>(symb));
                    }
                }


            }
            P_TERMINAL_SET.insert(te(ttype::l_eof));
            //FIRST SET


            auto CALC_FIRST_SET_COUNT = [this] {

                size_t SUM = 0;
                for (auto& fpair : FIRST_SET)
                {
                    SUM += fpair.second.size();
                }
                return SUM;
            };
            size_t LAST_FIRST_SIZE = 0;
            size_t FIRST_SIZE = 0;
            do
            {
                LAST_FIRST_SIZE = FIRST_SIZE;

                for (auto& single_rule : p)
                {

                    for (size_t pindex = 0; pindex < single_rule.second.size(); pindex++)
                    {
                        auto& cFIRST = FIRST(single_rule.second[pindex]);
                        /*FIRST[single_rule.first].insert(cFIRST.begin(), cFIRST.end());*///HAS E-PRODUCER DONOT ADD IT TO SETS

                        bool e_prd = false;

                        for (auto& sy : cFIRST)
                        {
                            if (sy.t_type != +ttype::l_empty)
                                FIRST_SET[single_rule.first].insert(sy);//NON-E-PRODUCER, ADD IT 
                            else
                                e_prd = true;
                        }

                        if (!e_prd)// NO E-PRODUCER END
                            break;


                        if (pindex + 1 == single_rule.second.size())
                        {
                            //IF ALL PRODUCER-RULES CAN BEGIN WITH E-PRODUCER, ADD E TO FIRST-SET
                            FIRST_SET[single_rule.first].insert(te(ttype::l_empty));
                        }
                    }
                    /*get_FIRST(single_rule.second);
                    FIRST[single_rule.first].insert();*/
                }

                FIRST_SIZE = CALC_FIRST_SET_COUNT();
            } while (LAST_FIRST_SIZE != FIRST_SIZE);

            //GET FOLLOW-SET
            FOLLOW_SET[S].insert(te(ttype::l_eof));//ADD EOF TO FINAL-AIM
            auto CALC_FOLLOW_SET_COUNT = [this] {

                size_t SUM = 0;
                for (auto& fpair : FOLLOW_SET)
                {
                    SUM += fpair.second.size();
                }
                return SUM;
            };
            size_t LAST_FOLLOW_SIZE = 0;
            size_t FOLLOW_SIZE = 0;

            do
            {
                LAST_FOLLOW_SIZE = FOLLOW_SIZE;

                for (auto& single_rule : p)
                {
                    for (size_t pindex = 0; pindex < single_rule.second.size(); pindex++)
                    {
                        if (std::holds_alternative<nt>(single_rule.second[pindex]))
                        {
                            nt nxt = std::get<nt>(single_rule.second[pindex]);

                            if (pindex == single_rule.second.size() - 1)
                            {
                                auto& follow_set = FOLLOW_SET[single_rule.first];
                                FOLLOW_SET[single_rule.second[pindex]].insert(follow_set.begin(), follow_set.end());
                            }
                            else
                            {
                                //NON-TE CAN HAVE FOLLOW SET
                                auto& first_set = FIRST(single_rule.second[pindex + 1]);

                                bool have_e_prud = false;
                                for (auto& token_in_first : first_set)
                                {
                                    if (token_in_first.t_type != +ttype::l_empty)
                                        FOLLOW_SET[single_rule.second[pindex]].insert(token_in_first);
                                    else
                                        have_e_prud = true;
                                }
                                if (have_e_prud)
                                {
                                    if (pindex == single_rule.second.size() - 2)
                                    {
                                        auto& follow_set = FOLLOW_SET[single_rule.first];
                                        FOLLOW_SET[single_rule.second[pindex]].insert(follow_set.begin(), follow_set.end());
                                    }
                                    else
                                    {
                                        auto& next_symb_follow_set = FIRST(single_rule.second[pindex + 2]);
                                        FOLLOW_SET[single_rule.second[pindex]].insert(next_symb_follow_set.begin(), next_symb_follow_set.end());
                                    }
                                }
                                //first_set.begin(), first_set.end());
                            }
                        }
                    }
                }

                FOLLOW_SIZE = CALC_FOLLOW_SET_COUNT();
            } while (LAST_FOLLOW_SIZE != FOLLOW_SIZE);

            //LR0
            //1. BUILD LR-ITEM-COLLECTION
            auto LR_ITEM_SET_EQULE = [](const std::set<lr_item>& A, const std::set<lr_item>& B)
            {
                if (A.size() == B.size())
                {
                    for (auto aim_ind = A.begin(), iset_ind = B.begin();
                        aim_ind != A.end();
                        aim_ind++, iset_ind++)
                    {
                        if ((*aim_ind) != (*iset_ind))
                        {
                            return false;
                        }
                    }

                    return true;
                }
                return false;
            };

            std::vector<std::set<lr_item>> C = { CLOSURE({ lr_item{p[0] ,0,{ttype::l_eof}} }) };
            size_t LAST_C_SIZE = C.size();
            do
            {
                //std::vector<std::set<lr_item>> NewAddC;

                LAST_C_SIZE = C.size();

                for (size_t C_INDEX = 0; C_INDEX < C.size(); C_INDEX++)
                {
                    auto& I = C[C_INDEX];
                    std::vector<std::set<lr_item>> NewAddToC;
                    std::set<sym> NEXT_SYM;
                    for (auto item : I)
                    {
                        if (item.next_sign < item.item_rule.second.size())
                        {
                            NEXT_SYM.insert(item.item_rule.second[item.next_sign]);
                        }
                    }
                    for (auto& X : NEXT_SYM)
                    {
                        auto& iset = GOTO(I, X);

                        if (iset.size() && std::find_if(C.begin(), C.end(), [&](std::set<lr_item>& aim) {
                            return LR_ITEM_SET_EQULE(aim, iset);
                            }) == C.end())
                        {
                            NewAddToC.push_back(iset);
                        }
                    }

                    C.insert(C.end(), NewAddToC.begin(), NewAddToC.end());
                }


            } while (LAST_C_SIZE != C.size());
            C_SET = C;

            //2. BUILD LR0
            /*

            std::map<size_t, std::map<sym, std::set<action>, sym_less>>& lr0_table = LR0_TABLE;

            for (size_t statei = 0; statei < C.size(); statei++)
            {
                for (auto& prod : C[statei])//STATE i
                {

                    if (prod.next_sign >= prod.item_rule.second.size())
                    {
                        if (prod.item_rule.first == p[0].first)
                        {
                            //JOB DONE~
                            lr0_table[statei][te(ttype::l_eof)].insert(action{ action::act_type::accept });
                        }
                        else
                        {
                            //USING THE RULE TO GENERATE NON-TE
                            size_t orgin_index = (size_t)(std::find(ORGIN_P.begin(), ORGIN_P.end(), prod.item_rule) - ORGIN_P.begin());
                            for (auto& p_te : P_TERMINAL_SET)
                                lr0_table[statei][p_te].insert(action{ action::act_type::reduction,orgin_index });

                        }


                    }
                    else if (std::holds_alternative<nt>(prod.item_rule.second[prod.next_sign]))
                    {
                        //GOTO
                        auto& next_state = GOTO({ prod }, prod.item_rule.second[prod.next_sign]);
                        for (size_t j = 0; j < C.size(); j++)
                        {
                            auto& IJ = C[j];
                            if (LR_ITEM_SET_EQULE(IJ, next_state))
                            {
                                lr0_table[statei][prod.item_rule.second[prod.next_sign]].insert(action{ action::act_type::state_goto,j });
                            }
                        }
                    }
                    else
                    {
                        //SATCK
                        auto& next_state = GOTO({ prod }, prod.item_rule.second[prod.next_sign]);
                        for (size_t j = 0; j < C.size(); j++)
                        {
                            auto& IJ = C[j];
                            if (LR_ITEM_SET_EQULE(IJ, next_state))
                            {
                                lr0_table[statei][prod.item_rule.second[prod.next_sign]].insert(action{ action::act_type::push_stack,j });
                            }
                        }
                    }
                }
            }
            */


            //3. BUILD LR1

            lr1table_t& lr1_table = LR1_TABLE;
            for (size_t statei = 0; statei < C.size(); statei++)
            {
                std::set<sym>next_set;

                for (auto& CSTATE_I : C[statei])//STATEi
                {
                    if (CSTATE_I.next_sign >= CSTATE_I.item_rule.second.size())
                    {
                        // REDUCE OR FINISH~
                        if (CSTATE_I.item_rule.first == p[0].first)
                        {
                            // DONE~
                            lr1_table[statei][te(ttype::l_eof)].insert(action{ action::act_type::accept ,CSTATE_I.prospect });
                        }
                        else
                        {
                            // REDUCE
                            size_t orgin_index = (size_t)(std::find(ORGIN_P.begin(), ORGIN_P.end(), CSTATE_I.item_rule) - ORGIN_P.begin());
                            for (auto& p_te : P_TERMINAL_SET)
                            {
                                if (p_te == CSTATE_I.prospect)
                                    lr1_table[statei][p_te].insert(action{ action::act_type::reduction,CSTATE_I.prospect,orgin_index });
                            }
                        }
                    }
                    else
                        next_set.insert(CSTATE_I.item_rule.second[CSTATE_I.next_sign]); // GOTO/STACK
                }

                for (auto& next_symb : next_set)
                {
                    auto& next_state = GOTO(C[statei], next_symb);
                    if (std::holds_alternative<nt>(next_symb))
                    {
                        //GOTO SET
                        for (size_t j = 0; j < C.size(); j++)
                        {
                            auto& IJ = C[j];
                            if (LR_ITEM_SET_EQULE(IJ, next_state))
                            {
                                //for (auto secprod : prod_pair.second)
                                lr1_table[statei][next_symb].insert(action{ action::act_type::state_goto,te(ttype::l_empty),j });
                            }
                        }
                    }
                    else
                    {
                        //STACK SET
                        for (size_t j = 0; j < C.size(); j++)
                        {
                            auto& IJ = C[j];
                            if (LR_ITEM_SET_EQULE(IJ, next_state))
                            {
                                //for (auto secprod : prod_pair.second)
                                lr1_table[statei][next_symb].insert(action{ action::act_type::push_stack,te(ttype::l_empty),j });
                            }
                        }
                    }
                }

            }

        }

        bool check_lr1(std::wostream& ostrm = std::wcout)
        {
            bool result = false;
            for (size_t i = 0; i < ORGIN_P.size(); i++)
            {
                ostrm << i << L"\t" << lr_item{ ORGIN_P[i],size_t(-1),te(ttype::l_eof) } << std::endl;;
            }

            for (auto& LR1 : LR1_TABLE)
            {
                for (auto& LR : LR1.second)
                {
                    if (LR.second.size() >= 2)
                    {
                        if (!result)
                            ostrm << L"LR1 has a grammatical conflict:" << std::endl;
                        ostrm << L"In state: " << LR1.first << L" Symbol: ";
                        if (std::holds_alternative<te>(LR.first))
                            ostrm << std::get<te>(LR.first) << std::endl;
                        else
                            ostrm << std::get<nt>(LR.first) << std::endl;

                        ostrm << "========================" << std::endl;

                        for (auto& lr1_ : C_SET[LR1.first])
                            ostrm << lr1_ << std::endl;

                        ostrm << "========================" << std::endl;

                        for (auto& act : LR.second)
                        {
                            if (act.act == action::act_type::reduction)
                                ostrm << "R" << act.state << ": " << lr_item{ ORGIN_P[act.state],size_t(-1),te{ttype::l_eof} } << std::endl;
                            else if (act.act == action::act_type::push_stack)
                                ostrm << "S" << act.state << std::endl;
                        }

                        ostrm << std::endl << std::endl;

                        result = true;
                    }
                }
            }

            return result;
        }

        std::map<std::wstring, te_nt_index_t> NONTERM_MAP;
        std::map<lex_type, te_nt_index_t> TERM_MAP;
        void finish_rt()
        {
            size_t maxim_state_count = 0;
            te_nt_index_t nt_te_index = 0;

#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
            if (!lr1_fast_cache_enabled())
            {
#endif
                lex_type leof = lex_type::l_eof;
                TERM_MAP[leof] = ++nt_te_index;

                for (auto& [_state, act_list] : LR1_TABLE)
                {
                    for (auto& [symb, _action] : act_list)
                    {
                        if (std::holds_alternative<te>(symb))
                        {
                            if (TERM_MAP.find(std::get<te>(symb).t_type) == TERM_MAP.end())
                                TERM_MAP[std::get<te>(symb).t_type] = ++nt_te_index;
                        }
                        else
                        {
                            if (NONTERM_MAP.find(std::get<nt>(symb).nt_name) == NONTERM_MAP.end())
                                NONTERM_MAP[std::get<nt>(symb).nt_name] = ++nt_te_index;
                        }

                        // In face, we can just LR1_TABLE.end()-1).first to get this value
                        // but i just want to do so~~
                        if (_state > maxim_state_count)
                            maxim_state_count = _state;
                    }
                }

                RT_LR1_TABLE.resize(maxim_state_count + 1);
                for (auto& [_state, act_list] : LR1_TABLE)
                {
                    RT_LR1_TABLE[_state].resize(nt_te_index + 1);
                    for (auto& [symb, _action] : act_list)
                    {
                        if (!_action.empty())
                        {
                            if (std::holds_alternative<te>(symb))
                                RT_LR1_TABLE[_state][TERM_MAP[std::get<te>(symb).t_type]] = *_action.begin();
                            else
                                RT_LR1_TABLE[_state][NONTERM_MAP[std::get<nt>(symb).nt_name]] = *_action.begin();
                        }
                    }
                }
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
            }
#endif

            // OK Then RT_PRODUCTION 
            RT_PRODUCTION.resize(ORGIN_P.size());
            for (size_t rt_pi = 0; rt_pi < RT_PRODUCTION.size(); rt_pi++)
            {
                RT_PRODUCTION[rt_pi].production_aim = NONTERM_MAP[ORGIN_P[rt_pi].first.nt_name];
                RT_PRODUCTION[rt_pi].rule_right_count = ORGIN_P[rt_pi].second.size();
                RT_PRODUCTION[rt_pi].ast_create_func = ORGIN_P[rt_pi].first.ast_create_func;
                RT_PRODUCTION[rt_pi].rule_left_name = ORGIN_P[rt_pi].first.nt_name;
            }

            // OK!
        }

        void display(std::wostream& ostrm = std::wcout)
        {
            auto ABSTRACT = [](const std::wstring& str) {
                if (str.size() > 4)
                    return (str.substr(2, 4)) + L"..";
                return (str);
            };

            ostrm << L"ID" << L"\t";
            ostrm << L"|\t";
            for (auto& NT_I : P_TERMINAL_SET)
            {
                ostrm << NT_I << L"\t";
            }
            ostrm << L"|\t";
            for (auto& NT_I : P_NOTERMINAL_SET)
            {
                ostrm << ABSTRACT(NT_I.nt_name) << L"\t";
            }
            ostrm << std::endl;
            for (auto& LR1_INFO : LR1_TABLE)
            {
                size_t state_max_count = 0;
                for (auto& act_set : LR1_INFO.second)
                {
                    if (act_set.second.size() > state_max_count)
                        state_max_count = act_set.second.size();
                }
                ostrm << LR1_INFO.first;


                for (size_t item_index = 0; item_index < state_max_count; item_index++)
                {
                    ostrm << L"\t|\t";

                    for (auto& TOK : P_TERMINAL_SET)
                    {
                        if (item_index < LR1_INFO.second[TOK].size())
                        {
                            auto index = LR1_INFO.second[TOK].begin();
                            for (size_t i = 0; i < item_index; i++)
                            {
                                index++;
                            }
                            ostrm << (*index) << L"\t";
                        }
                        else
                            ostrm << L"-\t";
                    }
                    ostrm << L"|\t";
                    for (auto& TOK : P_NOTERMINAL_SET)
                    {
                        if (item_index < LR1_INFO.second[TOK].size())
                        {
                            auto index = LR1_INFO.second[TOK].begin();
                            for (size_t i = 0; i < item_index; i++)
                            {
                                index++;
                            }
                            ostrm << (*index) << L"\t";
                        }
                        else
                            ostrm << L"-\t";
                    }
                    ostrm << std::endl;
                }
            }

            /*ostrm << "===================" << std::endl;
            size_t CI_COUNT = 0;

            for (auto& CI : C_SET)
            {
                ostrm << "Item: " << CI_COUNT << std::endl;
                CI_COUNT++;
                for (auto item : CI)
                {
                    ostrm << item << std::endl;
                }
                ostrm << "===================" << std::endl;
            }*/

        }

        bool check(lexer& tkr)
        {
            // READ FROM token_reader，CHECK IT

            std::stack<size_t> state_stack;
            std::stack<sym> sym_stack;

            state_stack.push(0);
            sym_stack.push(grammar::te{ lex_type::l_eof });

            auto NOW_STACK_STATE = [&]()->size_t& {return state_stack.top(); };
            auto NOW_STACK_SYMBO = [&]()->sym& {return sym_stack.top(); };

            bool success_flag = true;

            do
            {
                std::wstring out_indentifier;
                lex_type type = tkr.peek(&out_indentifier);

                if (type == +lex_type::l_error)
                {
                    // have a lex error, skip this error.
                    std::wcout << "fail: lex_error, skip it." << std::endl;
                    type = tkr.next(&out_indentifier);
                    success_flag = false;
                    continue;
                }

                auto top_symbo =
                    (state_stack.size() == sym_stack.size() ?
                        grammar::te{ type, out_indentifier }
                        :
                        NOW_STACK_SYMBO());

                auto& actions = LR1_TABLE[NOW_STACK_STATE()][top_symbo];
                auto& e_actions = LR1_TABLE[NOW_STACK_STATE()][grammar::te{ grammar::ttype::l_empty }];

                if (actions.size() || e_actions.size())
                {
                    bool e_rule = false;
                    if (!actions.size())
                    {
                        e_rule = true;
                    }
                    auto& take_action = actions.size() ? *actions.begin() : *e_actions.begin();

                    if (take_action.act == grammar::action::act_type::push_stack)
                    {

                        state_stack.push(take_action.state);
                        if (e_rule)
                        {
                            sym_stack.push(grammar::te{ grammar::ttype::l_empty });
                        }
                        else
                        {
                            sym_stack.push(grammar::te{ type,out_indentifier });
                            tkr.next(nullptr);
                        }

                        if (std::holds_alternative<grammar::te>(sym_stack.top()))
                        {
                            std::wcout << "stack_in: " << std::get<grammar::te>(sym_stack.top()) << std::endl;
                        }
                        else
                        {
                            std::wcout << "stack_in: " << std::get<grammar::nt>(sym_stack.top()) << std::endl;
                        }
                    }
                    else if (take_action.act == grammar::action::act_type::reduction)
                    {
                        auto& red_rule = ORGIN_P[take_action.state];
                        for (size_t i = red_rule.second.size(); i > 0; i--)
                        {
                            //NEED VERIFY?
                            /*
                            size_t index = i - 1;
                            if (sym_stack.top() != red_rule.second[index])
                            {
                                return false;
                            }*/

                            state_stack.pop();
                            sym_stack.pop();
                        }

                        sym_stack.push(red_rule.first);

                        std::wcout << "reduce: " << grammar::lr_item{ red_rule,size_t(-1) ,{grammar::ttype::l_eof} } << std::endl;
                    }
                    else if (take_action.act == grammar::action::act_type::accept)
                    {
                        if (success_flag)
                            std::wcout << "acc!" << std::endl;
                        else
                            std::wcout << "acc, but there are something fail." << std::endl;
                        return success_flag;
                    }
                    else if (take_action.act == grammar::action::act_type::state_goto)
                    {
                        std::wcout << "goto: " << take_action.state << std::endl;
                        state_stack.push(take_action.state);
                    }
                    else
                    {
                        std::wcout << "fail: err action." << std::endl;
                        return false;
                    }
                }
                else
                {
                    std::wcout << "fail: no action can be take." << std::endl;
                    return false;//NOT FOUND
                }

            } while (true);

            std::wcout << "fail: unknown" << std::endl;
            return false;
        }

        ast_base* gen(lexer& tkr) const
        {
            size_t last_error_rowno = 0;
            size_t last_error_colno = 0;
            size_t try_recover_count = 0;

            struct source_info
            {
                size_t row_no;
                size_t col_no;
            };

            std::stack<size_t> state_stack;
            std::stack<te_nt_index_t> sym_stack;
            std::stack<std::pair<source_info, std::any>> node_stack;

            const te_nt_index_t te_leof_index = TERM_MAP.at(+lex_type::l_eof);
            const te_nt_index_t te_lempty_index = TERM_MAP.at(+lex_type::l_empty);

            state_stack.push(0);
            sym_stack.push(te_leof_index);

            auto NOW_STACK_STATE = [&]()->size_t& {return state_stack.top(); };
            auto NOW_STACK_SYMBO = [&]()->te_nt_index_t& {return sym_stack.top(); };

            do
            {
                std::wstring out_indentifier;
                lex_type type = tkr.peek(&out_indentifier);

                if (type == +lex_type::l_error)
                {
                    // have a lex error, skip this error.
                    type = tkr.next(&out_indentifier);
                    continue;
                }

                auto top_symbo =
                    (state_stack.size() == sym_stack.size()
                        ? TERM_MAP.at(type)
                        : NOW_STACK_SYMBO());

                no_prospect_action actions, e_actions;
                LR1_TABLE_READ(NOW_STACK_STATE(), top_symbo, &actions);// .at().at();
                LR1_TABLE_READ(NOW_STACK_STATE(), te_lempty_index, &e_actions);// LR1_TABLE.at(NOW_STACK_STATE()).at();

                if (actions.act != action::act_type::error || e_actions.act != action::act_type::error)
                {
                    bool e_rule = false;
                    if (actions.act == action::act_type::error)
                    {
                        e_rule = true;
                    }
                    auto& take_action = actions.act != action::act_type::error ? actions : e_actions;

                    if (take_action.act == grammar::action::act_type::push_stack)
                    {
                        state_stack.push(take_action.state);
                        if (e_rule)
                        {
                            node_stack.push(std::make_pair(source_info{ tkr.after_pick_next_file_rowno, tkr.after_pick_next_file_colno }, token{ grammar::ttype::l_empty }));
                            sym_stack.push(te_lempty_index);
                        }
                        else
                        {
                            node_stack.push(std::make_pair(source_info{ tkr.after_pick_next_file_rowno, tkr.after_pick_next_file_colno }, token{ type, out_indentifier }));
                            sym_stack.push(TERM_MAP.at(type));
                            tkr.next(nullptr);

                            // std::wcout << "stackin: " << type._to_string() << std::endl;
                        }

                    }
                    else if (take_action.act == grammar::action::act_type::reduction)
                    {
                        auto& red_rule = RT_PRODUCTION[take_action.state];

                        std::vector<source_info> srcinfo_bnodes(red_rule.rule_right_count);
                        std::vector<std::any> te_or_nt_bnodes(red_rule.rule_right_count);

                        for (size_t i = red_rule.rule_right_count; i > 0; i--)
                        {
                            state_stack.pop();
                            sym_stack.pop();
                            srcinfo_bnodes[i - 1] = std::move(node_stack.top().first);
                            te_or_nt_bnodes[i - 1] = std::move(node_stack.top().second);
                            node_stack.pop();
                        }
                        sym_stack.push(red_rule.production_aim);

                        if (std::find_if(te_or_nt_bnodes.begin(), te_or_nt_bnodes.end(), [](std::any& astn) {

                            if (astn.type() == typeid(token))
                            {
                                if (std::any_cast<token>(astn).type == +lex_type::l_error)
                                {
                                    return true;
                                }
                            }
                            else if (astn.type() == typeid(lex_type))
                            {
                                if (std::any_cast<lex_type>(astn) == +lex_type::l_error)
                                {
                                    return true;
                                }
                            }

                            return false;
                            }) != te_or_nt_bnodes.end())//bnodes CONTAIN L_ERROR
                        {
                            node_stack.push(std::make_pair(source_info{ tkr.next_file_rowno, tkr.next_file_colno }, token{ +lex_type::l_error }));
                            // std::wcout << ANSI_HIR "reduce: err happend, just go.." ANSI_RST << std::endl;
                        }
                        else
                        {
                            auto astnode = red_rule.ast_create_func(tkr, red_rule.rule_left_name, te_or_nt_bnodes);

                            if (ast_base* ast_node_; cast_any_to<ast_base*>(astnode, ast_node_))
                            {
                                wo_assert(!te_or_nt_bnodes.empty());
                                if (te_or_nt_bnodes.size() == 1 && ast_empty::is_empty(te_or_nt_bnodes[0]))
                                {
                                    ast_node_->row_end_no = tkr.after_pick_next_file_rowno;
                                    ast_node_->col_end_no = tkr.after_pick_next_file_colno;
                                    ast_node_->row_begin_no = tkr.after_pick_next_file_rowno;
                                    ast_node_->col_begin_no = tkr.after_pick_next_file_colno;
                                }
                                else
                                {
                                    if (!srcinfo_bnodes.empty())
                                    {
                                        ast_node_->row_end_no = tkr.now_file_rowno;
                                        ast_node_->col_end_no = tkr.now_file_colno;
                                        ast_node_->row_begin_no = srcinfo_bnodes.front().row_no;
                                        ast_node_->col_begin_no = srcinfo_bnodes.front().col_no;
                                        goto apply_src_info_end;
                                    }
                                    ast_node_->row_end_no = tkr.after_pick_next_file_rowno;
                                    ast_node_->col_end_no = tkr.after_pick_next_file_colno;
                                    ast_node_->row_begin_no = tkr.after_pick_next_file_rowno;
                                    ast_node_->col_begin_no = tkr.after_pick_next_file_colno;
                                apply_src_info_end:;
                                }
                                ast_node_->source_file = wstring_pool::get_pstr(wo::str_to_wstr(tkr.source_file));
                            }
                            node_stack.push(std::make_pair(srcinfo_bnodes.front(), astnode));
                        }

                        // std::wcout << "reduce: " << grammar::lr_item{ ORGIN_P[take_action.state] ,size_t(-1) ,{grammar::ttype::l_eof} } << std::endl;
                    }
                    else if (take_action.act == grammar::action::act_type::accept)
                    {
                        //std::wcout << "acc!" << std::endl;
                        if (!tkr.lex_error_list.empty())
                            return nullptr;

                        ast_base* result;
                        if (cast_any_to<ast_base*>(node_stack.top().second, result))
                        {
                            return result;
                        }
                        else
                        {
                            tkr.parser_error(0x0000, WO_ERR_SHOULD_BE_AST_BASE);
                            return nullptr;
                        }
                    }
                    else if (take_action.act == grammar::action::act_type::state_goto)
                    {
                        // std::wcout << "goto: " << take_action.state << std::endl;
                        state_stack.push(take_action.state);
                    }
                    else
                    {
                        goto error_handle;
                    }
                }
                else
                {
                error_handle:
                    std::vector<te> should_be;
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
                    if (lr1_fast_cache_enabled())
                    {
                        auto cur_state = NOW_STACK_STATE();

                        for (size_t i = 1/* 0 is l_error/state, skip it */; i < LR1_R_S_CACHE_SZ; i++)
                            if (LR1_R_S_CACHE[LR1_GOTO_RS_MAP[cur_state][1] * LR1_R_S_CACHE_SZ + i] != 0)
                                should_be.push_back(LR1_TERM_LIST[i]);
                    }
                    else
#endif
                    {
                        if (LR1_TABLE.find(NOW_STACK_STATE()) != LR1_TABLE.end())
                            for (auto& actslist : LR1_TABLE.at(NOW_STACK_STATE()))
                            {
                                if (std::holds_alternative<te>(actslist.first) &&
                                    actslist.second.size() &&
                                    (actslist.second.begin()->act != action::act_type::error && actslist.second.begin()->act != action::act_type::state_goto))
                                {
                                    should_be.push_back(std::get<te>(actslist.first));
                                }
                            }
                    }

                    std::wstring advise = L"";

                    if (should_be.size())
                    {
                        advise = L", " WO_TERM_EXCEPTED L" ";
                        for (auto& excepted_te : should_be)
                        {
                            if (excepted_te.t_name != L"")
                            {
                                advise += L"'" + excepted_te.t_name + L"' ";
                            }
                            else
                            {
                                const wchar_t* spfy_op_key_str = lexer::lex_is_keyword_type(excepted_te.t_type);
                                spfy_op_key_str = spfy_op_key_str ? spfy_op_key_str : lexer::lex_is_operate_type(excepted_te.t_type);
                                if (spfy_op_key_str)
                                {
                                    advise += std::wstring(L"'") + spfy_op_key_str + L"'";
                                }
                                else
                                {
                                    switch (excepted_te.t_type)
                                    {
                                    case lex_type::l_eof:
                                        advise += L"<EOF>"; break;
                                    case lex_type::l_semicolon:
                                        advise += L"';'"; break;
                                    case lex_type::l_right_brackets:
                                        advise += L"')'"; break;
                                    case lex_type::l_left_brackets:
                                        advise += L"'('"; break;
                                    case lex_type::l_right_curly_braces:
                                        advise += L"'}'"; break;
                                    case lex_type::l_left_curly_braces:
                                        advise += L"'{'"; break;
                                    default:
                                    {
                                        std::wstringstream used_for_enum_to_wstr;
                                        used_for_enum_to_wstr << excepted_te.t_type;
                                        advise += used_for_enum_to_wstr.str() + L" ";

                                        break;
                                    }
                                    }
                                }
                            }


                        }
                    }

                    std::wstring err_info;

                    if (type == +lex_type::l_eof)
                    {
                        err_info = WO_ERR_UNEXCEPT_EOF + advise;
                    }
                    else
                    {
                        err_info = WO_ERR_UNEXCEPT_TOKEN +
                            (L"'" + (out_indentifier)+L"'") + advise;
                    }

                    // IF SAME ERROR HAPPEND, JUST STOP COMPILE
                    if (tkr.just_have_err ||
                        (last_error_rowno == tkr.now_file_rowno &&
                            last_error_colno == tkr.now_file_colno))
                    {
                        try_recover_count++;

                        if (try_recover_count >= 3)
                            goto error_handle_fail;

                    }
                    else
                    {
                        try_recover_count = 0;
                        tkr.parser_error(0x0000, err_info.c_str());
                        last_error_rowno = tkr.now_file_rowno;
                        last_error_colno = tkr.now_file_colno;

                    }

                    //tokens_queue.front();// CURRENT TOKEN ERROR HAPPEND
                    // FIND USABLE STATE A TO REDUCE.
                    while (!state_stack.empty())
                    {
                        size_t stateid = state_stack.top();

                        std::unordered_set<te_nt_index_t> reduceables;
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
                        if (lr1_fast_cache_enabled())
                        {
                            for (size_t i = 1/* 0 is l_error/state, skip it */; i < LR1_R_S_CACHE_SZ; i++)
                            {
                                int state = LR1_R_S_CACHE[LR1_GOTO_RS_MAP[stateid][1] * LR1_R_S_CACHE_SZ + i];
                                if (state < 0)
                                    reduceables.insert(RT_PRODUCTION[(-state) - 1].production_aim);
                            }
                        }
                        else
#endif
                        {
                            if (LR1_TABLE.find(stateid) != LR1_TABLE.end())
                                for (auto act : LR1_TABLE.at(stateid))
                                    if (act.second.size() && act.second.begin()->act == action::act_type::reduction)
                                        reduceables.insert(RT_PRODUCTION[act.second.begin()->state].production_aim);
                        }

                        if (!reduceables.empty())
                        {
                            wo::lex_type out_lex = lex_type::l_empty;
                            std::wstring out_str;
                            while ((out_lex = tkr.peek(&out_str)) != +lex_type::l_eof)
                            {
                                for (te_nt_index_t fr : reduceables)
                                {
                                    // FIND NON-TE AND IT'S FOLLOW
                                    auto& follow_set = FOLLOW_READ(fr);

                                    auto place = std::find_if(follow_set.begin(), follow_set.end(), [&](const te& t) {

                                        return t.t_name == L"" ?
                                            t.t_type == out_lex
                                            :
                                            t.t_name == out_str &&
                                            t.t_type == out_lex;

                                        });
                                    if (place != follow_set.end())
                                        goto error_progress_end;
                                }
                                tkr.next(nullptr);
                            }
                            goto error_handle_fail;

#if 0
                            std::vector<te> _termi_want_to_inserts;

#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
                            if (lr1_fast_cache_enabled())
                            {
                                for (size_t i = 1/* 0 is l_error/state, skip it */; i < LR1_R_S_CACHE_SZ; i++)
                                    if (LR1_R_S_CACHE[LR1_GOTO_RS_MAP[stateid][1] * LR1_R_S_CACHE_SZ + i] != 0)
                                        _termi_want_to_inserts.push_back(LR1_TERM_LIST[i]);
                            }
                            else
#endif
                            {
                                for (auto act : LR1_TABLE.at(stateid))
                                {
                                    if (act.second.size())
                                    {
                                        if (std::holds_alternative<te>(act.first))
                                        {
                                            auto& tk_type = std::get<te>(act.first).t_type;
                                            _termi_want_to_inserts.push_back(tk_type);
                                        }
                                    }
                                }
                            }

                            if (try_recover_count == 0 && std::find(_termi_want_to_inserts.begin(), _termi_want_to_inserts.end(),
                                +lex_type::l_semicolon)
                                != _termi_want_to_inserts.end())
                            {
                                tkr.push_temp_for_error_recover(lex_type::l_semicolon, L"");
                                goto error_progress_end;
                            }
                            if (try_recover_count == 1 && std::find(_termi_want_to_inserts.begin(), _termi_want_to_inserts.end(),
                                +lex_type::l_right_brackets)
                                != _termi_want_to_inserts.end())
                            {
                                tkr.push_temp_for_error_recover(lex_type::l_right_brackets, L"");
                                goto error_progress_end;
                            }
                            if (try_recover_count == 2 && std::find(_termi_want_to_inserts.begin(), _termi_want_to_inserts.end(),
                                +lex_type::l_right_curly_braces)
                                != _termi_want_to_inserts.end())
                            {
                                tkr.push_temp_for_error_recover(lex_type::l_right_curly_braces, L"");
                                goto error_progress_end;
                            }
#endif
                        }

                        if (node_stack.size())
                        {
                            state_stack.pop();
                            sym_stack.pop();
                            node_stack.pop();
                        }
                        else
                        {
                            goto error_handle_fail;
                        }
                    }
                error_handle_fail:
                    tkr.parser_error(0x0000, WO_ERR_UNABLE_RECOVER_FROM_ERR);
                    return nullptr;

                error_progress_end:;
                }

            } while (true);

            tkr.parser_error(0x0000, WO_ERR_UNEXCEPT_EOF);

            return nullptr;
        }
    };

    inline std::wostream& operator<<(std::wostream& ost, const  grammar::lr_item& lri)
    {
        ost << (lri.item_rule.first.nt_name) << "->";
        size_t index = 0;
        for (auto& s : lri.item_rule.second)
        {
            if (index == lri.next_sign)
            {
                ost << L" * ";
            }
            if (std::holds_alternative<grammar::te>(s))
            {
                const grammar::te& v = std::get<grammar::te>(s);
                if (v.t_name == L"")
                {
                    ost << L" " << v.t_type._to_string() << L" ";
                }
                else
                {
                    ost << (v.t_name);
                }
            }
            else
            {
                const grammar::nt& v = std::get<grammar::nt>(s);
                ost << (v.nt_name);
            }
            index++;
        }
        if (index == lri.next_sign)
        {
            ost << L" * ";
        }
        ost << "," << lri.prospect;
        return ost;
    }

    inline std::wostream& operator<<(std::wostream& ost, const  grammar::terminal& ter)
    {
        if (ter.t_name == L"")
        {
            if (ter.t_type == +lex_type::l_eof)
                ost << L"$";
            else if (ter.t_type == +lex_type::l_semicolon)
                ost << L";";
            else if (ter.t_type == +lex_type::l_comma)
                ost << L",";
            else
                ost << ter.t_type._to_string();
        }
        else
            ost << (ter.t_name);
        return ost;
    }
    inline std::wostream& operator<<(std::wostream& ost, const  grammar::nonterminal& noter)
    {
        ost << (noter.nt_name);
        return ost;
    }

    inline std::wostream& operator<<(std::wostream& ost, const  grammar::action& act)
    {
        switch (act.act)
        {
            /*case grammar::action::act_type::error:
                ost << "err";
                break;*/
        case grammar::action::act_type::accept:
            ost << "acc";
            break;
        case grammar::action::act_type::push_stack:
            ost << "s" << act.state;
            break;
        case grammar::action::act_type::reduction:
            ost << "r" << act.state /*<< "," << act.prospect*/;
            break;
        case grammar::action::act_type::state_goto:
            ost << "g" << act.state;
            break;
        default:
            ost << "-";
            break;
        }

        return ost;
    }
}