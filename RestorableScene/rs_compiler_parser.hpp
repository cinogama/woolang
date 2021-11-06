#pragma once
/*
In order to speed up compile and use less memory,
RS will using 'hand-work' parser, there is not yacc/bison..


* P.S. Maybe we need a LR(N) Hand work?
*/

#include "rs_compiler_lexer.hpp"

#include <variant>
#include <functional>
#include <queue>
#include <stack>
#include <sstream>

namespace rs
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

        using te = terminal;//终结符
        struct nonterminal;
        using nt = nonterminal;//非终结符
        using symbol = std::variant<te, nt>;
        using sym = symbol;
        using rule = std::pair< nonterminal, std::vector<sym>>;
        using symlist = std::vector<sym>;
        using ttype = lex_type;//token类型 简记法

        struct ASTBase;
        struct ASTNode
        {
            token terminal = { lex_type::l_error };
            std::shared_ptr<ASTBase> nonterminal = nullptr;

            bool is_terminal()const
            {
                return !nonterminal;
            }
            ASTNode(const token& te) :
                terminal(te)
            {

            }
            ASTNode(const std::shared_ptr<ASTBase>& astb) :
                nonterminal(astb)
            {

            }


        };

        struct ASTBase
        {
            using nodes = std::vector<ASTNode>;

            /*virtual void build(const nodes&) = 0;*/

            std::wstring name;

            static void space(std::wostream& os, size_t layer)
            {
                for (size_t i = 0; i < layer; i++)
                {
                    os << "  ";
                }
            }
            virtual void display(std::wostream& os = std::wcout, size_t lay = 0)
            {
                space(os, lay); os << "<" << (name) << ">" << std::endl;
            }

        };

        struct ASTDefault :public ASTBase
        {
            nodes childs;

            /*void build(const nodes& n)override
            {
                childs = n;
            }*/
            void display(std::wostream& os = std::wcout, size_t lay = 0)override
            {
                space(os, lay); os << "<ASTDefault: " << (name) << ">" << std::endl;
                for (auto& n : childs)
                {
                    if (n.is_terminal() == false)
                        n.nonterminal->display(os, lay + 1);
                    else
                    {
                        space(os, lay + 1); os << n.terminal << std::endl;
                    }
                }
            }
        };
        struct ASTError :public ASTBase
        {
            std::wstring what;

            ASTError(const std::wstring& errmsg) :
                what(errmsg)
            {

            }
        };

        struct nonterminal
        {
            std::wstring nt_name;

            std::function<std::shared_ptr<ASTBase>(const std::wstring&, const ASTBase::nodes&)> ast_create_func =
                [](const std::wstring& name, const ASTBase::nodes& chs) {
                auto defaultAST = std::make_shared<grammar::ASTDefault>();
                defaultAST->name = name;
                defaultAST->childs = chs;
                return defaultAST;
            };

            nonterminal(const std::wstring& name = L"") :
                nt_name(name)

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

        std::map< nonterminal, std::vector<symlist>>P;//产生式规则
        std::vector<rule>ORGIN_P;//原始形式的产生式规则
        nonterminal S;//起始符号

        struct sym_less {
            constexpr bool operator()(const sym& _Left, const sym& _Right) const {

                bool left_is_te = std::holds_alternative<te>(_Left);
                if (left_is_te == std::holds_alternative<te>(_Right))
                {
                    //两者符号相同
                    if (left_is_te)
                    {
                        return  std::get<te>(_Left) < std::get<te>(_Right);
                    }
                    else
                    {
                        return  std::get<nt>(_Left) < std::get<nt>(_Right);
                    }
                }

                //无聊的规定 认定te<nt


                return !left_is_te;
            }
        };
        using sym_nts_t = std::map< sym, std::set< te>, sym_less>;

        sym_nts_t FIRST_SET;
        sym_nts_t FOLLOW_SET;

        struct lr_item//lr分析项目
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

        struct lr0_item//lr分析项目
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

        std::set< te>& FIRST(const sym& _sym)
        {
            if (std::holds_alternative<te>(_sym))//_sym是终结符
            {
                if (FIRST_SET.find(_sym) == FIRST_SET.end())
                {
                    //te首次被获取 应当给予设置
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
                            //非终结符才继续
                            auto& ntv = std::get<nt>(item.item_rule.second[item.next_sign]);
                            auto& prules = P[ntv];
                            for (auto& prule : prules)
                            {
                                if (item.next_sign + 1 < item.item_rule.second.size())
                                {
                                    auto& beta_set = FIRST(item.item_rule.second[item.next_sign + 1]);
                                    for (auto& beta : beta_set)
                                    {
                                        if (beta.t_type == +ttype::l_empty)
                                        {
                                            add_ietm.insert(lr_item{ rule{ntv,prule}, 0 , item.prospect });
                                        }
                                        else
                                        {
                                            add_ietm.insert(lr_item{ rule{ntv,prule}, 0 , beta });
                                        }
                                    }
                                }
                                else
                                {
                                    add_ietm.insert(lr_item{ rule{ntv,prule}, 0 , item.prospect });
                                }


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

        /*
                struct action
        {
            enum class act_type
            {
                error,
                state_goto,
                push_stack,
                reduction,
                accept,

            }act = act_type::error;

            size_t state = 0;


            bool operator<(const action& another)const
            {
                if (act == another.act)
                {
                    return state < another.state;
                }
                return act < another.act;
            }

            friend std::wostream& operator<<(std::wostream& ost, const  grammar::action& act);
        };
        */
        struct action/*_lr1*/
        {
            enum class act_type
            {
                error,
                state_goto,
                push_stack,
                reduction,
                accept,

            }act = act_type::error;
            te prospect;
            size_t state = 0;




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

        using lr1table_t = std::map<size_t, std::map<sym, std::set<action>, sym_less>>;
        std::map<size_t, std::map<sym, std::set<action>, sym_less>> LR1_TABLE;
        // Store this ORGIN_P, LR1_TABLE and FOLLOW_SET after compile.

        const std::set<action>& LR1_TABLE_READ(size_t a, const sym& b) const
        {
            static std::set<action> empty_action;

            if (LR1_TABLE.find(a) == LR1_TABLE.end())
            {
                return empty_action;
            }
            if (LR1_TABLE.at(a).find(b) == LR1_TABLE.at(a).end())
            {
                return empty_action;
            }
            return LR1_TABLE.at(a).at(b);
        }

        friend static std::wostream& operator<<(std::wostream& ost, const  grammar::lr_item& lri);

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
            //计算FIRST集合


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
                        /*FIRST[single_rule.first].insert(cFIRST.begin(), cFIRST.end());*///由于包含E 所以不能直接加入集合

                        bool e_prd = false;

                        for (auto& sy : cFIRST)
                        {
                            if (sy.t_type != +ttype::l_empty)
                                FIRST_SET[single_rule.first].insert(sy);//只要不是空产生式 就把符号塞进FIRST集合中
                            else
                                e_prd = true;
                        }

                        if (!e_prd)//如果当前项目没有空产生式 则结束
                            break;


                        if (pindex + 1 == single_rule.second.size())
                        {
                            //如果整个符号串全都可推导空产生式，那么空产生式被添加到FIRST
                            FIRST_SET[single_rule.first].insert(te(ttype::l_empty));
                        }
                    }
                    /*get_FIRST(single_rule.second);
                    FIRST[single_rule.first].insert();*/
                }

                FIRST_SIZE = CALC_FIRST_SET_COUNT();
            } while (LAST_FIRST_SIZE != FIRST_SIZE);


            //计算FOLLOW集合

            //
            FOLLOW_SET[S].insert(te(ttype::l_eof));//开始符号的FOLLOW集中添加eof
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
                            if (pindex == single_rule.second.size() - 1)
                            {
                                auto& follow_set = FOLLOW_SET[single_rule.first];
                                FOLLOW_SET[single_rule.second[pindex]].insert(follow_set.begin(), follow_set.end());
                            }
                            else
                            {
                                //非终结符才有计算FOLLOW集合的意义
                                auto& first_set = FIRST(single_rule.second[pindex + 1]);

                                bool have_e_prud = false;
                                for (auto& token_in_first : first_set)
                                {
                                    if (token_in_first.t_type != +ttype::l_empty)
                                        FOLLOW_SET[single_rule.second[pindex]].insert(token_in_first);
                                    else
                                        have_e_prud = true;
                                }
                                if (have_e_prud && pindex == single_rule.second.size() - 2)
                                {
                                    auto& follow_set = FOLLOW_SET[single_rule.first];
                                    FOLLOW_SET[single_rule.second[pindex]].insert(follow_set.begin(), follow_set.end());
                                }
                                //first_set.begin(), first_set.end());

                            }
                        }
                    }
                }

                FOLLOW_SIZE = CALC_FOLLOW_SET_COUNT();
            } while (LAST_FOLLOW_SIZE != FOLLOW_SIZE);


            //LR0分析表构建


            //1.构建LR 项集族
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

            //2. 构建LR0 语法分析动作表
            /*

            std::map<size_t, std::map<sym, std::set<action>, sym_less>>& lr0_table = LR0_TABLE;

            for (size_t statei = 0; statei < C.size(); statei++)
            {
                for (auto& prod : C[statei])//状态i
                {

                    if (prod.next_sign >= prod.item_rule.second.size())
                    {
                        if (prod.item_rule.first == p[0].first)
                        {
                            //收工
                            lr0_table[statei][te(ttype::l_eof)].insert(action{ action::act_type::accept });
                        }
                        else
                        {
                            //此时是归约项目
                            size_t orgin_index = (size_t)(std::find(ORGIN_P.begin(), ORGIN_P.end(), prod.item_rule) - ORGIN_P.begin());
                            for (auto& p_te : P_TERMINAL_SET)
                                lr0_table[statei][p_te].insert(action{ action::act_type::reduction,orgin_index });

                        }


                    }
                    else if (std::holds_alternative<nt>(prod.item_rule.second[prod.next_sign]))
                    {
                        //GOTO 表
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
                        //SATCK 表
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


            //3. 构建LR1 语法分析动作表

            std::map<size_t, std::map<sym, std::set<action>, sym_less>>& lr1_table = LR1_TABLE;
            for (size_t statei = 0; statei < C.size(); statei++)
            {
                std::set<sym>next_set;

                for (auto& CSTATE_I : C[statei])//状态i
                {
                    if (CSTATE_I.next_sign >= CSTATE_I.item_rule.second.size())
                    {
                        //归约或者接收项目
                        if (CSTATE_I.item_rule.first == p[0].first)
                        {
                            //收工
                            lr1_table[statei][te(ttype::l_eof)].insert(action{ action::act_type::accept ,CSTATE_I.prospect });
                        }
                        else
                        {
                            //此时是归约项目
                            size_t orgin_index = (size_t)(std::find(ORGIN_P.begin(), ORGIN_P.end(), CSTATE_I.item_rule) - ORGIN_P.begin());
                            for (auto& p_te : P_TERMINAL_SET)
                            {
                                if (p_te == CSTATE_I.prospect)
                                    lr1_table[statei][p_te].insert(action{ action::act_type::reduction,CSTATE_I.prospect,orgin_index });
                            }
                        }
                    }
                    else
                        next_set.insert(CSTATE_I.item_rule.second[CSTATE_I.next_sign]);//进入GOTO 或者STACK
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
            /*std::vector<std::
            for ()
            {

            }*/
            /*std::wcout << "ID" << "\t";
            std::wcout << "|\t";
            for (auto& NT_I : P_TERMINAL_SET)
            {
                std::wcout << NT_I << "\t";
            }
            std::wcout << "|\t";
            for (auto& NT_I : P_NOTERMINAL_SET)
            {
                std::wcout << NT_I << "\t";
            }
            std::wcout << std::endl;
            for (auto& LR0_INFO : lr1_table)
            {
                size_t state_max_count = 0;
                for (auto& act_set : LR0_INFO.second)
                {
                    if (act_set.second.size() > state_max_count)
                        state_max_count = act_set.second.size();
                }
                std::wcout << LR0_INFO.first;


                for (size_t item_index = 0; item_index < state_max_count; item_index++)
                {
                    std::wcout << "\t|\t";

                    for (auto& TOK : P_TERMINAL_SET)
                    {
                        if (item_index < LR0_INFO.second[TOK].size())
                        {
                            auto index = LR0_INFO.second[TOK].begin();
                            for (size_t i = 0; i < item_index; i++)
                            {
                                index++;
                            }
                            std::wcout << (*index) << "\t";
                        }
                        else
                            std::wcout << " \t";
                    }
                    std::wcout << "|\t";
                    for (auto& TOK : P_NOTERMINAL_SET)
                    {
                        if (item_index < LR0_INFO.second[TOK].size())
                        {
                            auto index = LR0_INFO.second[TOK].begin();
                            for (size_t i = 0; i < item_index; i++)
                            {
                                index++;
                            }
                            std::wcout << (*index) << "\t";
                        }
                        else
                            std::wcout << "\t";
                    }
                    std::wcout << std::endl;
                }
            }

            size_t CI_COUNT = 0;

            for (auto& CI : C)
            {
                std::wcout << "Item: " << CI_COUNT << std::endl;
                CI_COUNT++;
                for (auto item : CI)
                {
                    std::wcout << item << std::endl;
                }
                std::wcout << "===================" << std::endl;
            }


            std::wcout << "fff";*/
        }

        bool check_lr1(std::wostream& ostrm = std::wcout)
        {
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
                        ostrm << L"LR1 has a grammatical conflict:" << std::endl;
                        ostrm << L"State: " << LR1.first << L" Symbol: ";
                        if (std::holds_alternative<te>(LR.first))
                            ostrm << std::get<te>(LR.first) << std::endl;
                        else
                            ostrm << std::get<nt>(LR.first) << std::endl;

                        for (auto& act : LR.second)
                        {
                            if (act.act == action::act_type::reduction)
                                ostrm << "R" << act.state << ": " << lr_item{ ORGIN_P[act.state],size_t(-1),te{ttype::l_eof} } << std::endl;
                            else if (act.act == action::act_type::push_stack)
                                ostrm << "S" << act.state << std::endl;
                        }

                        ostrm << std::endl;

                        return true;
                    }
                }
            }

            return false;
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
            //读取token_reader，检查是否符合语法

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
                            size_t index = i - 1;

                            //需要验证？
                            /*if (sym_stack.top() != red_rule.second[index])
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
                    return false;//没有找到
                }

            } while (true);

            std::wcout << "fail: unknown" << std::endl;
            return false;
        }



        std::shared_ptr<ASTBase> gen(lexer& tkr) const
        {
            //build_list.resize(ORGIN_P.size(), []()->std::shared_ptr<ASTBase> {return std::make_shared<ASTDefault>(); });

            //读取token_reader，检查是否符合语法

            std::stack<size_t> state_stack;
            std::stack<sym> sym_stack;
            std::stack<ASTNode> node_stack;

            state_stack.push(0);
            sym_stack.push(grammar::te{ lex_type::l_eof });

            auto NOW_STACK_STATE = [&]()->size_t& {return state_stack.top(); };
            auto NOW_STACK_SYMBO = [&]()->sym& {return sym_stack.top(); };

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
                    (state_stack.size() == sym_stack.size() ?
                        grammar::te{ type, out_indentifier }
                        :
                        NOW_STACK_SYMBO());


                const auto& actions = LR1_TABLE_READ(NOW_STACK_STATE(), top_symbo);// .at().at();
                auto& e_actions = LR1_TABLE_READ(NOW_STACK_STATE(), grammar::te{ grammar::ttype::l_empty });// LR1_TABLE.at(NOW_STACK_STATE()).at();

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
                            node_stack.push(token{ grammar::ttype::l_empty });
                            sym_stack.push(grammar::te{ grammar::ttype::l_empty });
                        }
                        else
                        {
                            node_stack.push(token{ type, out_indentifier });
                            sym_stack.push(grammar::te{ type,out_indentifier });
                            tkr.next(nullptr);
                        }

                    }
                    else if (take_action.act == grammar::action::act_type::reduction)
                    {
                        auto& red_rule = ORGIN_P[take_action.state];

                        ASTBase::nodes bnodes;

                        for (size_t i = red_rule.second.size(); i > 0; i--)
                        {
                            size_t index = i - 1;

                            //需要验证？
                            /*if (sym_stack.top() != red_rule.second[index])
                            {
                                return false;
                            }*/

                            state_stack.pop();
                            sym_stack.pop();

                            bnodes.push_back(node_stack.top());
                            node_stack.pop();
                        }

                        std::reverse(bnodes.begin(), bnodes.end());

                        sym_stack.push(red_rule.first);

                        if (std::find_if(bnodes.begin(), bnodes.end(), [](const ASTNode& astn) {
                            return astn.is_terminal() && astn.terminal.type == +lex_type::l_error;
                            }) != bnodes.end())//bnodes包含拒绝表达式
                        {
                            node_stack.push(ASTNode(token{ +lex_type::l_error }));
                        }
                        else
                        {
                            auto astnode = red_rule.first.ast_create_func(red_rule.first.nt_name, bnodes);
                            //astnode->build(bnodes);

                            if (auto err_node = dynamic_cast<ASTError*>(astnode.get()))
                            {
                                // 如果节点生成器返回的是ASTError节点，那么说明节点生成时发生了错误

                                // TODO: HERE NEED FIX
                                tkr.parser_error(0x0000, err_node->what.c_str());
                            }

                            node_stack.push(astnode);
                        }

                        //std::wcout << "reduce: " << grammar::lr_item{ red_rule,size_t(-1) ,{grammar::ttype::l_eof} } << std::endl;
                    }
                    else if (take_action.act == grammar::action::act_type::accept)
                    {
                        //std::wcout << "acc!" << std::endl;
                        if (!tkr.lex_error_list.empty())
                            return nullptr;
                        return node_stack.top().nonterminal;
                    }
                    else if (take_action.act == grammar::action::act_type::state_goto)
                    {
                        //std::wcout << "goto: " << take_action.state << std::endl;
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

                    std::wstring advise = L"";

                    if (should_be.size())
                    {
                        advise = L", excepted ";
                        for (auto& excepted_te : should_be)
                        {
                            if (excepted_te.t_name != L"")
                            {
                                advise += L"'" + excepted_te.t_name + L"' ";
                            }
                            else
                            {
                                const wchar_t* spfy_op_key_str = lexer::lex_is_keyword_type(excepted_te.t_type);
                                spfy_op_key_str = spfy_op_key_str? spfy_op_key_str: lexer::lex_is_operate_type(excepted_te.t_type);
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
                                        advise += L"')'"; break;
                                    case lex_type::l_left_curly_braces:
                                        advise += L"'('"; break;
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
                        err_info = L"Unexcepted end of file" + advise;
                    }
                    else
                    {
                        err_info = L"Unexcepted symbol: " +
                            (L"'" + (out_indentifier)+L"'") + advise;
                    }
                    

                    // 如果刚刚发生了相同的错误， 结束处理
                    if (tkr.just_have_err)
                        goto error_handle_fail;

                    tkr.parser_error(0x0000, err_info.c_str());

                    //tokens_queue.front();//当前错误符号
                    //恐慌模式，查找可能的状态A

                    while (!state_stack.empty())
                    {
                        size_t stateid = state_stack.top();
                        if (LR1_TABLE.find(stateid) != LR1_TABLE.end())
                            for (auto act : LR1_TABLE.at(stateid))
                            {
                                if (std::holds_alternative<nt>(act.first) && act.second.size() && act.second.begin()->act == action::act_type::reduction)
                                {
                                    //找到对应的非终结符
                                    //找到这个非终结符的FOLLOW集

                                    auto& follow_set = FOLLOW_READ(std::get<nt>(act.first));

                                    rs::lex_type out_lex = lex_type::l_empty;
                                    std::wstring out_str;
                                    while ((out_lex = tkr.peek(&out_str)) != +lex_type::l_eof)
                                    {
                                        out_lex = tkr.next(&out_str);

                                        auto place = std::find_if(follow_set.begin(), follow_set.end(), [&](const te& t) {

                                            return t.t_name == L"" ?
                                                t.t_type == out_lex
                                                :
                                                t.t_name == out_str &&
                                                t.t_type == out_lex;

                                            });
                                        if (place != follow_set.end())
                                        {
                                            //开始处理归约

                                            auto& red_rule = ORGIN_P[act.second.begin()->state];
                                            //node_stack.push(tokens_queue.front());

                                            //for (size_t i = red_rule.second.size(); i > 0; i--)
                                            //{
                                            //	size_t index = i - 1;

                                            //	state_stack.pop();
                                            //	sym_stack.pop();

                                            //	//bnodes.push_back(node_stack.top());
                                            //	node_stack.pop();
                                            //}
                                            state_stack.push(act.second.begin()->state);
                                            sym_stack.push(red_rule.first);
                                            node_stack.push(ASTNode(token{ +lex_type::l_error }));
                                            /*	state_stack.pop();

                                                state_stack.push(act.second.begin()->state);

                                                while (sym_stack.size() >= state_stack.size())
                                                {
                                                    sym_stack.pop();
                                                }
                                                sym_stack.push(act.first);*/
                                            goto error_progress_end;
                                            //完成假归约
                                        }
                                    }

                                    goto error_handle_fail;
                                }
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

                    //此处进行错误例程，恢复错误状态，检测可能的错误可能，然后继续
                error_handle_fail:
                    return nullptr;

                error_progress_end:
                    0;
                }

            } while (true);

            tkr.parser_error(0x0000, L"Unexcepted end of file.");

            return nullptr;
        }
    };

    inline static std::wostream& operator<<(std::wostream& ost, const  grammar::lr_item& lri)
    {
        ost << (lri.item_rule.first.nt_name) << "->";
        size_t index = 0;
        for (auto& s : lri.item_rule.second)
        {
            if (index == lri.next_sign)
            {
                ost << L"·";
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
            ost << "·";
        }
        ost << "," << lri.prospect;
        return ost;
    }

    inline static std::wostream& operator<<(std::wostream& ost, const  grammar::terminal& ter)
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
                ost << std::string(ter.t_type._to_string()).substr(0, 4).c_str() << L"..";
        }
        else
            ost << (ter.t_name);
        return ost;
    }
    inline static std::wostream& operator<<(std::wostream& ost, const  grammar::nonterminal& noter)
    {
        ost << (noter.nt_name);
        return ost;
    }

    inline static std::wostream& operator<<(std::wostream& ost, const  grammar::action& act)
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

    class parser
    {
        class ast_base
        {
        private:
            inline static std::vector<ast_base*> list;
            ast_base* parent;
            ast_base* children;
            ast_base* sibling;
        public:
            ast_base()
                : parent(nullptr)
                , children(nullptr)
                , sibling(nullptr)
            {
                list.push_back(this);
            }
            void add_child(ast_base* ast_node)
            {
                rs_test(ast_node->parent == nullptr);
                rs_test(ast_node->sibling == nullptr);

                ast_node->parent = this;
                ast_base* childs = children;
                while (childs)
                {
                    if (childs->sibling == nullptr)
                    {
                        childs->sibling = ast_node;
                        return;
                    }
                }
                children = childs;
            }
            void remove_child(ast_base* ast_node)
            {
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

                            return;
                        }
                    }

                    last_childs = childs;
                    childs = childs->sibling;
                }

                rs_error("There is no such a child node.");
            }
        public:
            virtual std::wstring to_wstring() = 0;
        };

        ///////////////////////////////////////////////

        lexer* lex;

    public:
        parser(lexer& rs_lex)
            :lex(&rs_lex)
        {

        }

        void LEFT_VALUE_handler()
        {
            // LEFT_VALUE   >>  l_identifier
            //                  LEFT_VALUE . l_identifier
            //                  LEFT_VALUE ( ARG_DEFINE )
            //                  LEFT_VALUE :: l_identifier
        }

        void ASSIGNMENT_handler()
        {
            // ASSIGNMENT   >>  LEFT_VALUE = RIGHT_VALUE
            //                  LEFT_VALUE += RIGHT_VALUE
            //                  LEFT_VALUE -= RIGHT_VALUE
            //                  LEFT_VALUE *= RIGHT_VALUE
            //                  LEFT_VALUE /= RIGHT_VALUE
            //                  LEFT_VALUE %= RIGHT_VALUE
        }

        void RIGHT_VALUE_handler()
        {
            // RIGHT_VALUE  >>  ASSIGNMENT
            //                  func( ARG_DEFINE ) RETURN_TYPE_DECLEAR SENTENCE
            //                  LOGICAL_OR
        }

        void EXPRESSION_handler()
        {
            // This function will product a expr, if failed return nullptr;
            // 
            // EXPRESSION   >>  RIGHT_VALUE

            return RIGHT_VALUE_handler();
        }

        void build_ast()
        {
            //
        }
    };
}