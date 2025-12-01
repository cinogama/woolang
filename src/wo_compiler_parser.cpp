#include "wo_afx.hpp"

#include "wo_compiler_parser.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    ast::AstAllocator::AstAllocator()
        // Make sure new page create when first time alloc node.
        : m_allocated_offset_in_page(PAGE_SIZE)
    {
    }
    ast::AstAllocator::~AstAllocator()
    {
        for (auto astnode : m_created_ast_nodes)
            delete astnode;

        for (auto page : m_allocated_pages)
            free(page);
    }
    void* ast::AstAllocator::allocate_space_for_ast_node(size_t sz)
    {
        return allocate_space_with_aligh_for_ast_node(
            sz, std::align_val_t{ alignof(std::max_align_t) });
    }
    void* ast::AstAllocator::allocate_space_with_aligh_for_ast_node(size_t sz, std::align_val_t al)
    {
        size_t al_sz = static_cast<size_t>(al);
        wo_assert((al_sz & (al_sz - 1)) == 0); // al_sz is power of 2

        size_t alligned_next_offset = (m_allocated_offset_in_page + (al_sz - 1)) & (~(al_sz - 1));
        if (alligned_next_offset + sz > PAGE_SIZE)
        {
            char* new_page = reinterpret_cast<char*>(malloc(PAGE_SIZE));
            wo_assert(new_page != nullptr);

            m_allocated_pages.push_back(new_page);
            m_allocated_offset_in_page = 0;
            alligned_next_offset = 0;
        }
        void* result = m_allocated_pages.back() + alligned_next_offset;
        m_allocated_offset_in_page = alligned_next_offset + sz;
        m_created_ast_nodes.push_back(reinterpret_cast<AstBase*>(result));
        return result;
    }
    void ast::AstAllocator::swap(AstAllocator& another)
    {
        m_allocated_pages.swap(another.m_allocated_pages);
        m_created_ast_nodes.swap(another.m_created_ast_nodes);
        std::swap(
            m_allocated_offset_in_page,
            another.m_allocated_offset_in_page);
    }
    const std::vector<ast::AstBase*>& ast::AstAllocator::get_allocated_nodes_for_lspv2() const
    {
        return m_created_ast_nodes;
    }
    bool ast::AstAllocator::empty() const
    {
        return m_allocated_pages.empty() && m_created_ast_nodes.empty();
    }

    std::set<grammar::te>& grammar::FIRST(const sym& _sym)
    {
        if (std::holds_alternative<te>(_sym)) //_sym is te
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
    std::set<grammar::te>& grammar::FOLLOW(const nt& noterim)
    {
        return FOLLOW_SET[noterim];
    }
    const std::set<grammar::te>& grammar::FOLLOW_READ(const nt& noterim) const
    {
        static std::set<te> empty_set;

        if (FOLLOW_SET.find(noterim) == FOLLOW_SET.end())
            return empty_set;
        return FOLLOW_SET.at(noterim);
    }
    const std::set<grammar::te>& grammar::FOLLOW_READ(te_nt_index_t ntidx) const
    {
        for (auto& [ntname, idx] : NONTERM_MAP)
        {
            if (idx == ntidx)
                return FOLLOW_READ(nt(ntname));
        }
        // Should not be here.
        abort();
    }

    std::set<grammar::lr_item>& grammar::CLOSURE(const std::set<lr_item>& i)
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
                        // only for non-te
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
                                        if (beta.t_type == ttype::l_empty)
                                            has_e_prod = true;
                                        else
                                        {
                                            wo_assert(beta_follow.find(beta) != beta_follow.end());
                                            add_ietm.insert(lr_item{ rule{ntv, prule}, 0, beta });
                                        }
                                    }
                                }
                                else
                                    add_ietm.insert(lr_item{ rule{ntv, prule}, 0, item.prospect });
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
    std::set<grammar::lr_item>& grammar::GOTO(const std::set<lr_item>& i, const sym& X)
    {
        std::set<lr_item> j;
        for (auto& item : i)
        {
            if (item.next_sign < item.item_rule.second.size() &&
                item.item_rule.second[item.next_sign] == X)
            {
                j.insert(lr_item{ item.item_rule, item.next_sign + 1, item.prospect });
            }
        }

        return CLOSURE(j);
    }

    bool grammar::lr1_fast_cache_enabled() const noexcept
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

    void grammar::LR1_TABLE_READ(size_t a, te_nt_index_t b, no_prospect_action* write_act) const noexcept
    {
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
        if (lr1_fast_cache_enabled())
        {
            // Cache the goto/rs map state for this state
            const auto* const lr1_goto_rs_map_state = LR1_GOTO_RS_MAP[a];

            // Handle terminal symbols (b >= 0)
            if (b >= 0)
            {
                // Fast path: check for accept state first (common case)
                if (a == LR1_ACCEPT_STATE && b == LR1_ACCEPT_TERM)
                {
                    write_act->act = action::act_type::accept;
                    write_act->state = 0;
                    return;
                }

                // Check if there's a valid reduce/shift cache entry for this state
                const int rs_index = lr1_goto_rs_map_state[1];
                if (rs_index == -1)
                {
                    write_act->act = action::act_type::error;
                    write_act->state = SIZE_MAX;
                    return;
                }

                // Calculate cache index once
                const size_t cache_index =
                    static_cast<size_t>(rs_index) * LR1_R_S_CACHE_SZ + static_cast<size_t>(b);

                const int state = LR1_R_S_CACHE[cache_index];
                if (state > 0)
                {
                    // Shift action
                    write_act->act = action::act_type::push_stack;
                    write_act->state = static_cast<size_t>(state) - 1;
                }
                else if (state < 0)
                {
                    // Reduce action
                    write_act->act = action::act_type::reduction;
                    write_act->state = static_cast<size_t>(-state) - 1;
                }
                else
                {
                    // No action for this state & terminal
                    write_act->act = action::act_type::error;
                    write_act->state = SIZE_MAX;
                }
                return;
            }
            else
            {
                // Handle non-terminal symbols (b < 0)
                const int goto_index = lr1_goto_rs_map_state[0];
                if (goto_index == -1)
                {
                    write_act->act = action::act_type::error;
                    write_act->state = SIZE_MAX;
                    return;
                }

                // Calculate cache index once
                const size_t cache_index =
                    static_cast<size_t>(goto_index) * LR1_GOTO_CACHE_SZ + static_cast<size_t>(-b);
                const int state = LR1_GOTO_CACHE[cache_index];

                if (state == -1)
                {
                    // No goto action for this state & non-terminal
                    write_act->act = action::act_type::error;
                    write_act->state = SIZE_MAX;
                    return;
                }

                write_act->act = action::act_type::state_goto;
                write_act->state = static_cast<size_t>(state);
                return;
            }
        }
#endif
        // Fallback to regular table lookup
        const auto& lr1_item = RT_LR1_TABLE.at(a).at(b);
        write_act->act = lr1_item.act;
        write_act->state = lr1_item.state;
    }

    grammar::grammar()
    {
        // DONOTHING FOR READING FROM AUTO GENN
    }
    grammar::grammar(const std::vector<rule>& p)
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
        // FIRST SET

        auto CALC_FIRST_SET_COUNT = [this]
            {
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
                    /*FIRST[single_rule.first].insert(cFIRST.begin(), cFIRST.end());*/ // HAS E-PRODUCER DONOT ADD IT TO SETS

                    bool e_prd = false;

                    for (auto& sy : cFIRST)
                    {
                        if (sy.t_type != ttype::l_empty)
                            FIRST_SET[single_rule.first].insert(sy); // NON-E-PRODUCER, ADD IT
                        else
                            e_prd = true;
                    }

                    if (!e_prd) // NO E-PRODUCER END
                        break;

                    if (pindex + 1 == single_rule.second.size())
                    {
                        // IF ALL PRODUCER-RULES CAN BEGIN WITH E-PRODUCER, ADD E TO FIRST-SET
                        FIRST_SET[single_rule.first].insert(te(ttype::l_empty));
                    }
                }
                /*get_FIRST(single_rule.second);
                FIRST[single_rule.first].insert();*/
            }

            FIRST_SIZE = CALC_FIRST_SET_COUNT();
        } while (LAST_FIRST_SIZE != FIRST_SIZE);

        // GET FOLLOW-SET
        FOLLOW_SET[S].insert(te(ttype::l_eof)); // ADD EOF TO FINAL-AIM
        auto CALC_FOLLOW_SET_COUNT = [this]
            {
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
                            // NON-TE CAN HAVE FOLLOW SET
                            auto& first_set = FIRST(single_rule.second[pindex + 1]);

                            bool have_e_prud = false;
                            for (auto& token_in_first : first_set)
                            {
                                if (token_in_first.t_type != ttype::l_empty)
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
                            // first_set.begin(), first_set.end());
                        }
                    }
                }
            }

            FOLLOW_SIZE = CALC_FOLLOW_SET_COUNT();
        } while (LAST_FOLLOW_SIZE != FOLLOW_SIZE);

        // LR0
        // 1. BUILD LR-ITEM-COLLECTION
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

        std::vector<std::set<lr_item>> C = { CLOSURE({lr_item{p[0], 0, {ttype::l_eof}}}) };
        size_t LAST_C_SIZE = C.size();
        do
        {
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

                    if (iset.size() && std::find_if(C.begin(), C.end(), [&](std::set<lr_item>& aim)
                        { return LR_ITEM_SET_EQULE(aim, iset); }) == C.end())
                    {
                        NewAddToC.push_back(iset);
                    }
                }

                C.insert(C.end(), NewAddToC.begin(), NewAddToC.end());
            }

        } while (LAST_C_SIZE != C.size());
        C_SET = C;

        // 2. BUILD LR0
#if 0
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
#endif

        // 3. BUILD LR1
        lr1table_t& lr1_table = LR1_TABLE;
        for (size_t statei = 0; statei < C.size(); statei++)
        {
            std::set<sym> next_set;

            for (auto& CSTATE_I : C[statei]) // STATEi
            {
                if (CSTATE_I.next_sign >= CSTATE_I.item_rule.second.size())
                {
                    // REDUCE OR FINISH~
                    if (CSTATE_I.item_rule.first == p[0].first)
                    {
                        // DONE~
                        lr1_table[statei][te(ttype::l_eof)].insert(action{ action::act_type::accept, CSTATE_I.prospect });
                    }
                    else
                    {
                        // REDUCE
                        size_t orgin_index = (size_t)(std::find(ORGIN_P.begin(), ORGIN_P.end(), CSTATE_I.item_rule) - ORGIN_P.begin());
                        for (auto& p_te : P_TERMINAL_SET)
                        {
                            if (p_te == CSTATE_I.prospect)
                                lr1_table[statei][p_te].insert(action{ action::act_type::reduction, CSTATE_I.prospect, orgin_index });
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
                    // GOTO SET
                    for (size_t j = 0; j < C.size(); j++)
                    {
                        auto& IJ = C[j];
                        if (LR_ITEM_SET_EQULE(IJ, next_state))
                        {
                            // for (auto secprod : prod_pair.second)
                            lr1_table[statei][next_symb].insert(action{ action::act_type::state_goto, te(ttype::l_empty), j });
                        }
                    }
                }
                else
                {
                    // STACK SET
                    for (size_t j = 0; j < C.size(); j++)
                    {
                        auto& IJ = C[j];
                        if (LR_ITEM_SET_EQULE(IJ, next_state))
                        {
                            // for (auto secprod : prod_pair.second)
                            lr1_table[statei][next_symb].insert(action{ action::act_type::push_stack, te(ttype::l_empty), j });
                        }
                    }
                }
            }
        }
    }

    bool grammar::check_lr1(std::ostream& ostrm)
    {
        bool result = false;
        for (size_t i = 0; i < ORGIN_P.size(); i++)
        {
            ostrm << i << "\t" << lr_item{ ORGIN_P[i], size_t(-1), te(ttype::l_eof) } << std::endl;
            ;
        }

        for (auto& LR1 : LR1_TABLE)
        {
            for (auto& LR : LR1.second)
            {
                if (LR.second.size() >= 2)
                {
                    if (!result)
                        ostrm << "LR1 has a grammatical conflict:" << std::endl;
                    ostrm << "In state: " << LR1.first << " Symbol: ";
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
                            ostrm << "R" << act.state << ": " << lr_item{ ORGIN_P[act.state], size_t(-1), te{ttype::l_eof} } << std::endl;
                        else if (act.act == action::act_type::push_stack)
                            ostrm << "S" << act.state << std::endl;
                    }

                    ostrm << std::endl
                        << std::endl;

                    result = true;
                }
            }
        }

        return result;
    }
    void grammar::finish_rt()
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
                        auto iresult = TERM_MAP.insert(std::make_pair(std::get<te>(symb).t_type, 0));
                        if (iresult.second)
                            iresult.first->second = ++nt_te_index;
                    }
                    else
                    {
                        auto iresult = NONTERM_MAP.insert(std::make_pair(std::get<nt>(symb).nt_name, 0));
                        if (iresult.second)
                            iresult.first->second = ++nt_te_index;
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
                auto& rt_lr1_item = RT_LR1_TABLE[_state];

                rt_lr1_item.resize(nt_te_index + 1);

                for (auto& [symb, _action] : act_list)
                {
                    if (!_action.empty())
                    {
                        if (std::holds_alternative<te>(symb))
                            rt_lr1_item[TERM_MAP.at(std::get<te>(symb).t_type)] = *_action.begin();
                        else
                            rt_lr1_item[NONTERM_MAP.at(std::get<nt>(symb).nt_name)] = *_action.begin();
                    }
                }
            }
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
        }
#endif

        // OK Then RT_PRODUCTION
        RT_PRODUCTION.resize(ORGIN_P.size());

        auto rt_pi = RT_PRODUCTION.begin();
        auto origin_p = ORGIN_P.begin();

        for (; origin_p != ORGIN_P.end(); ++origin_p, ++rt_pi)
        {
            rt_pi->production_aim = NONTERM_MAP[origin_p->first.nt_name];
            rt_pi->rule_right_count = origin_p->second.size();
            rt_pi->ast_create_func = origin_p->first.ast_create_func;
            rt_pi->rule_left_name = origin_p->first.nt_name;

            wo_assert(rt_pi->rule_right_count != 0);
        }

        // Update TERM_MAP_FAST_LOOKUP
        for (const auto& [lexer_type, idex] : TERM_MAP)
        {
            TERM_MAP_FAST_LOOKUP[
                static_cast<lex_type_base_t>(lexer_type) + 1] = idex;
        }

        // OK!
    }
    ast::AstBase* grammar::gen(lexer& tkr) const
    {
        size_t last_error_rowno = 0;
        size_t last_error_colno = 0;
        size_t try_recover_count = 0;

        struct source_info
        {
            size_t row_no;
            size_t col_no;
        };

        using state_stack_t = std::vector<size_t>;
        using symbol_stack_t = std::vector<te_nt_index_t>;
        using node_stack_t = std::vector<std::pair<source_info, produce>>;

        state_stack_t state_stack;
        symbol_stack_t sym_stack;
        node_stack_t node_stack;

        state_stack.reserve(16);
        sym_stack.reserve(16);
        node_stack.reserve(16);

        const auto* const RT_PRODUCTION_P = RT_PRODUCTION.data();

        const te_nt_index_t te_leof_index =
            TERM_MAP_FAST_LOOKUP[static_cast<lex_type_base_t>(lex_type::l_eof) + 1];
        const te_nt_index_t te_lempty_index =
            TERM_MAP_FAST_LOOKUP[static_cast<lex_type_base_t>(lex_type::l_empty) + 1];

        state_stack.push_back(0);
        sym_stack.push_back(te_leof_index);

        auto NOW_STACK_STATE = [&]() -> size_t&
            { return state_stack.back(); };
        auto NOW_STACK_SYMBO = [&]() -> te_nt_index_t&
            { return sym_stack.back(); };

        std::vector<source_info> srcinfo_bnodes;
        std::vector<produce> te_or_nt_bnodes;

        srcinfo_bnodes.reserve(16);
        te_or_nt_bnodes.reserve(16);

        bool has_error_node = false;

        const lexer::peeked_token_t* peeked_token_instance = tkr.peek(true);

        // Reused temporary containers to avoid repeated allocations in error recovery path.
        std::vector<te_nt_index_t> reduceables;
        reduceables.reserve(16);
        std::string out_str;

        do
        {
            if (lex_type::l_error == peeked_token_instance->m_lex_type)
            {
                // Move forward;
                tkr.move_forward(true);
                peeked_token_instance = tkr.peek(true);
                continue;
            }

            // compute top symbol once per iteration
            auto top_symbo =
                (state_stack.size() == sym_stack.size()
                    ? TERM_MAP_FAST_LOOKUP[
                        static_cast<lex_type_base_t>(peeked_token_instance->m_lex_type) + 1]
                    : NOW_STACK_SYMBO());

            no_prospect_action actions;

            bool e_rule = false;
            LR1_TABLE_READ(NOW_STACK_STATE(), top_symbo, &actions);
            if (actions.act == action::act_type::error)
            {
                e_rule = true;

                LR1_TABLE_READ(NOW_STACK_STATE(), te_lempty_index, &actions);
                if (actions.act == action::act_type::error)
                {
                error_handle:
                    // IF SAME ERROR HAPPEND, JUST STOP COMPILE
                    if (last_error_rowno == peeked_token_instance->m_token_end[0] &&
                        last_error_colno == peeked_token_instance->m_token_end[1])
                    {
                        try_recover_count++;

                        if (try_recover_count >= 3)
                            goto error_handle_fail;
                    }
                    else
                    {
                        if (peeked_token_instance->m_lex_type == lex_type::l_macro)
                        {
                            tkr.record_format(
                                lexer::msglevel_t::error,
                                peeked_token_instance->m_token_begin[0],
                                peeked_token_instance->m_token_begin[1],
                                peeked_token_instance->m_token_end[0],
                                peeked_token_instance->m_token_end[1],
                                *tkr.get_source_path(),
                                WO_ERR_UNKNOWN_MACRO_NAMED,
                                peeked_token_instance->m_token_text.c_str());
                        }
                        else
                        {
                            std::string err_info;

                            if (peeked_token_instance->m_lex_type == lex_type::l_eof)
                                err_info = WO_ERR_UNEXCEPT_EOF;
                            else
                                err_info = WO_ERR_UNEXCEPT_TOKEN + ("'" + peeked_token_instance->m_token_text + "'");

                            (void)tkr.record_parser_error(lexer::msglevel_t::error, err_info.c_str());
                        }

                        try_recover_count = 0;
                        last_error_rowno = peeked_token_instance->m_token_end[0];
                        last_error_colno = peeked_token_instance->m_token_end[1];
                    }

                    // FIND USABLE STATE A TO REDUCE.
                    while (!state_stack.empty())
                    {
                        size_t stateid = state_stack.back();

                        // Collect reduceables efficiently using LR1_TABLE iterator to avoid double lookup.
                        reduceables.clear();
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
                        if (lr1_fast_cache_enabled())
                        {
                            for (size_t i = 1 /* 0 is l_error/state, skip it */; i < LR1_R_S_CACHE_SZ; i++)
                            {
                                int state = LR1_R_S_CACHE[LR1_GOTO_RS_MAP[stateid][1] * LR1_R_S_CACHE_SZ + i];
                                if (state < 0)
                                {
                                    te_nt_index_t cand = RT_PRODUCTION_P[(-state) - 1].production_aim;
                                    if (std::find(reduceables.begin(), reduceables.end(), cand) == reduceables.end())
                                        reduceables.push_back(cand);
                                }
                            }
                        }
                        else
#endif
                        {
                            auto it = LR1_TABLE.find(stateid);
                            if (it != LR1_TABLE.end())
                            {
                                // iterate once and collect reduction targets
                                for (auto& act : it->second)
                                {
                                    if (!act.second.empty())
                                    {
                                        const auto& first_act = act.second.begin()->act;
                                        if (first_act == action::act_type::reduction)
                                        {
                                            te_nt_index_t cand = RT_PRODUCTION_P[act.second.begin()->state].production_aim;
                                            if (std::find(reduceables.begin(), reduceables.end(), cand) == reduceables.end())
                                                reduceables.push_back(cand);
                                        }
                                    }
                                }
                            }
                        }

                        if (!reduceables.empty())
                        {
                            // Cache the peeked token locally to minimize calls into lexer peek().
                            wo::lex_type out_lex = lex_type::l_empty;
                            const lexer::peeked_token_t* local_peek = tkr.peek(true);
                            while ((out_lex = local_peek->m_lex_type) != lex_type::l_eof)
                            {
                                bool matched = false;
                                // Cache token text once per lookahead iteration to avoid repeated copies.
                                out_str = local_peek->m_token_text;
                                for (te_nt_index_t fr : reduceables)
                                {
                                    // FIND NON-TE AND IT'S FOLLOW
                                    auto& follow_set = FOLLOW_READ(fr);

                                    // linear scan; usually follow sets are small.
                                    for (const auto& t : follow_set)
                                    {
                                        // first compare lex_type (cheap), only then compare string
                                        if (t.t_type != out_lex)
                                            continue;

                                        if (t.t_name.empty() || t.t_name == out_str)
                                        {
                                            matched = true;
                                            break;
                                        }
                                    }
                                    if (matched)
                                        break;
                                }
                                if (matched)
                                    break;

                                tkr.move_forward(true);
                                local_peek = tkr.peek(true);
                            }
                            // if we exhausted to EOF without match, cannot recover from this state
                            if (local_peek->m_lex_type == lex_type::l_eof)
                                goto error_handle_fail;

                            // update the external peeked instance to current lexer state before continue outer loop
                            peeked_token_instance = local_peek;
                            goto error_progress_end;
                        }

                        if (node_stack.size())
                        {
                            state_stack.pop_back();
                            sym_stack.pop_back();
                            node_stack.pop_back();
                        }
                        else
                            goto error_handle_fail;
                    }
                error_handle_fail:
                    (void)tkr.record_parser_error(lexer::msglevel_t::error, WO_ERR_UNABLE_RECOVER_FROM_ERR);
                    return nullptr;

                error_progress_end:
                    continue;
                }
            }

            wo_assert(actions.act != action::act_type::error);

            if (actions.act == grammar::action::act_type::push_stack)
            {
                state_stack.push_back(actions.state);
                if (e_rule)
                {
                    node_stack.emplace_back(
                        source_info{
                            peeked_token_instance->m_token_begin[0],
                            peeked_token_instance->m_token_begin[1]
                        },
                        token{ grammar::ttype::l_empty });
                    sym_stack.push_back(te_lempty_index);
                }
                else
                {
                    node_stack.emplace_back(
                        source_info{
                            peeked_token_instance->m_token_begin[0],
                            peeked_token_instance->m_token_begin[1]
                        },
                        token{
                            peeked_token_instance->m_lex_type,
                            peeked_token_instance->m_token_text
                        });

                    switch (peeked_token_instance->m_lex_type)
                    {
                    case lex_type::l_macro:
                    {
                        tkr.record_format(
                            lexer::msglevel_t::error,
                            peeked_token_instance->m_token_begin[0],
                            peeked_token_instance->m_token_begin[1],
                            peeked_token_instance->m_token_end[0],
                            peeked_token_instance->m_token_end[1],
                            *tkr.get_source_path(),
                            WO_ERR_UNKNOWN_MACRO_NAMED,
                            peeked_token_instance->m_token_text.c_str());
                        break;
                    }
                    case lex_type::l_error:
                        has_error_node = true;
                        break;
                    default:
                        break;
                    }

                    sym_stack.push_back(
                        TERM_MAP_FAST_LOOKUP[
                            static_cast<lex_type_base_t>(peeked_token_instance->m_lex_type) + 1]);
                    tkr.move_forward(true);
                    peeked_token_instance = tkr.peek(true);
                }
            }
            else if (actions.act == grammar::action::act_type::reduction)
            {
                auto& red_rule = RT_PRODUCTION_P[actions.state];

                // Only te_or_nt_bnodes needs special resizing; reuse srcinfo_bnodes buffer to avoid reallocations.
                if (srcinfo_bnodes.size() < red_rule.rule_right_count)
                    srcinfo_bnodes.resize(red_rule.rule_right_count);
                te_or_nt_bnodes.resize(red_rule.rule_right_count);

                for (size_t i = red_rule.rule_right_count; i > 0; i--)
                {
                    state_stack.pop_back();
                    sym_stack.pop_back();
                    srcinfo_bnodes[i - 1] = std::move(node_stack.back().first);
                    te_or_nt_bnodes[i - 1] = std::move(node_stack.back().second);
                    node_stack.pop_back();
                }
                sym_stack.push_back(red_rule.production_aim);

                if (has_error_node)
                {
                    node_stack.emplace_back(
                        source_info{
                            peeked_token_instance->m_token_end[0],
                            peeked_token_instance->m_token_end[1],
                        },
                        token{ lex_type::l_error });
                }
                else
                {
                    auto astnode = red_rule.ast_create_func(tkr, te_or_nt_bnodes);
                    if (astnode.is_ast())
                    {
                        auto* ast_node_ = astnode.read_ast();
                        wo_assert(!te_or_nt_bnodes.empty());

                        if (ast_node_->source_location.source_file == nullptr)
                        {
                            wo_assert(red_rule.rule_right_count != 0);

                            ast_node_->source_location.begin_at =
                                ast::AstBase::location_t{
                                    srcinfo_bnodes.front().row_no,
                                    srcinfo_bnodes.front().col_no };
                            ast_node_->source_location.end_at =
                                ast::AstBase::location_t{
                                    peeked_token_instance->m_token_begin[2],
                                    peeked_token_instance->m_token_begin[3] };

                            ast_node_->source_location.source_file = tkr.get_source_path();
                        }
                    }
                    else if (astnode.read_token().type == lex_type::l_error)
                        has_error_node = true;

                    node_stack.emplace_back(srcinfo_bnodes.front(), astnode);
                }
            }
            else if (actions.act == grammar::action::act_type::accept)
            {
                if (tkr.has_error())
                    return nullptr;

                auto& node = node_stack.back().second;
                if (node.is_ast())
                {
                    // Append imported ast node front of specify ast-node;
                    return tkr.merge_imported_ast_trees(node.read_ast());
                }
                else
                {
                    wo_assert(node.read_token().type == lex_type::l_empty);

                    (void)tkr.record_parser_error(
                        lexer::msglevel_t::error,
                        WO_ERR_SOURCE_CANNOT_BE_EMPTY);
                    return nullptr;
                }
            }
            else if (actions.act == grammar::action::act_type::state_goto)
                state_stack.push_back(actions.state);
            else
                goto error_handle;

        } while (true);

        (void)tkr.record_parser_error(lexer::msglevel_t::error, WO_ERR_UNEXCEPT_EOF);

        return nullptr;
    }
    std::ostream& operator<<(std::ostream& ost, const grammar::lr_item& lri)
    {
        ost << (lri.item_rule.first.nt_name) << "->";
        size_t index = 0;
        for (auto& s : lri.item_rule.second)
        {
            if (index == lri.next_sign)
                ost << " * ";

            if (std::holds_alternative<grammar::te>(s))
            {
                const grammar::te& v = std::get<grammar::te>(s);
                if (v.t_name == "")
                    ost << " token: " << v.t_type << " ";
                else
                    ost << v.t_name;
            }
            else
                ost << (std::get<grammar::nt>(s).nt_name);
            index++;
        }
        if (index == lri.next_sign)
            ost << " * ";

        ost << "," << lri.prospect;
        return ost;
    }
    std::ostream& operator<<(std::ostream& ost, const grammar::terminal& ter)
    {
        if (ter.t_name == "")
        {
            if (ter.t_type == lex_type::l_eof)
                ost << "$";
            else if (ter.t_type == lex_type::l_semicolon)
                ost << ";";
            else if (ter.t_type == lex_type::l_comma)
                ost << ",";
            else
                ost << "token: " << (lex_type_base_t)ter.t_type;
        }
        else
            ost << ter.t_name;
        return ost;
    }
    std::ostream& operator<<(std::ostream& ost, const grammar::nonterminal& noter)
    {
        ost << noter.nt_name;
        return ost;
    }

    ast::AstBase* lexer::merge_imported_ast_trees(ast::AstBase* node)
    {
        wo_assert(node != nullptr);

        if (!m_imported_ast_tree_list.empty())
        {
            auto* merged_list = new ast::AstList();

            for (auto* imported_ast : m_imported_ast_tree_list)
            {
                merged_list->m_list.push_back(imported_ast);

                // NOTE: Generate an `nop` for debug info gen, avoid ip/cr conflict
                merged_list->m_list.push_back(new ast::AstNop());
            }
            merged_list->m_list.push_back(node);

            return merged_list;
        }
        return node;
    }
#endif
}
