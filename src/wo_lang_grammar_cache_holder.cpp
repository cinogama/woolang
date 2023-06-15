#define WO_LANG_GRAMMAR_LR1_IMPL

#include "wo_lang_grammar_lr1_autogen.hpp"

#ifdef WO_LANG_GRAMMAR_LR1_AUTO_GENED

namespace wo
{
    void wo_read_lr1_cache(wo::grammar& gram)
    {
#if WOOLANG_LR1_OPTIMIZE_LR1_TABLE
        gram.LR1_GOTO_CACHE = (const int*)woolang_lr1_act_goto;
        gram.LR1_R_S_CACHE = (const int*)woolang_lr1_act_stack_reduce;
        gram.LR1_GOTO_CACHE_SZ = sizeof(woolang_lr1_act_goto[0]) / sizeof(woolang_lr1_act_goto[0][0]);
        gram.LR1_R_S_CACHE_SZ = sizeof(woolang_lr1_act_stack_reduce[0]) / sizeof(woolang_lr1_act_stack_reduce[0][0]);
        gram.LR1_GOTO_RS_MAP = woolang_lr1_goto_rs_map;

        gram.LR1_ACCEPT_STATE = (size_t)woolang_accept_state;
        gram.LR1_ACCEPT_TERM = (grammar::te_nt_index_t)woolang_accept_term;

        gram.LR1_TERM_LIST = woolang_id_term_list;
        gram.LR1_NONTERM_LIST = woolang_id_nonterm_list;

        for (int i = 1; i < (int)(sizeof(woolang_id_nonterm_list) / sizeof(woolang_id_nonterm_list[0])); i++)
        {
            wo_assert(woolang_id_nonterm_list[i] != nullptr);
            gram.NONTERM_MAP[woolang_id_nonterm_list[i]] = -i;

        }
        for (int i = 0; i < (int)(sizeof(woolang_id_term_list) / sizeof(woolang_id_term_list[0])); i++)
            gram.TERM_MAP[woolang_id_term_list[i]] = i;
#endif

    }
    void wo_read_lr1_to(wo::grammar::lr1table_t& out_lr1table)
    {
#if !WOOLANG_LR1_OPTIMIZE_LR1_TABLE
        // READ GOTO
        for (auto& goto_act : woolang_lr1_act_goto)
        {
            for (size_t i = 1; i < sizeof(goto_act) / sizeof(goto_act[0]); i++)
            {
                if (goto_act[i] != -1)
                    out_lr1table[goto_act[0]][grammar::nt(woolang_id_nonterm_list[i])]
                    .insert(grammar::action{ grammar::action::act_type::state_goto,
                        grammar::te{lex_type::l_eof},
                        (size_t)goto_act[i] });
            }
        }

        // READ R-S
        for (auto& red_sta_act : woolang_lr1_act_stack_reduce)
        {
            for (size_t i = 1; i < sizeof(red_sta_act) / sizeof(red_sta_act[0]); i++)
            {
                if (red_sta_act[i] != 0)
                {
                    if (red_sta_act[i] > 0)
                    {
                        //push
                        out_lr1table[red_sta_act[0]][grammar::te(woolang_id_term_list[i])]
                            .insert(grammar::action{ grammar::action::act_type::push_stack,
                        grammar::te{lex_type::l_eof},
                        (size_t)red_sta_act[i] - 1 });
                    }
                    else if (red_sta_act[i] < 0)
                    {
                        //redu
                        out_lr1table[red_sta_act[0]][grammar::te(woolang_id_term_list[i])]
                            .insert(grammar::action{ grammar::action::act_type::reduction,
                        grammar::te{lex_type::l_eof},
                        (size_t)(-red_sta_act[i]) - 1 });
                    }
                }
            }
        }

        // READ ACC
        out_lr1table[woolang_accept_state][grammar::te(woolang_id_term_list[woolang_accept_term])]
            .insert(grammar::action{ grammar::action::act_type::accept,
                        grammar::te{lex_type::l_eof},
                        (size_t)0 });
#endif
    }
    void wo_read_follow_set_to(wo::grammar::sym_nts_t& out_followset)
    {
        for (auto& followset : woolang_follow_sets)
        {
            for (size_t i = 1; i < sizeof(followset) / sizeof(followset[0]) && followset[i] != 0; i++)
            {
                out_followset[grammar::nt(woolang_id_nonterm_list[followset[0]])].insert(
                    grammar::te(woolang_id_term_list[followset[i]])
                );
            }
        }

    }
    void wo_read_origin_p_to(std::vector<wo::grammar::rule>& out_origin_p)
    {
        for (auto& origin_p : woolang_origin_p)
        {
            grammar::symlist rule_symlist;

            for (int i = 0; i < origin_p[2]; i++)
            {
                if (origin_p[2 + i] > 0)
                    rule_symlist.push_back(grammar::te(woolang_id_term_list[origin_p[2 + i]]));
                else
                    rule_symlist.push_back(grammar::nt(woolang_id_nonterm_list[-origin_p[2 + i]]));

            }

            out_origin_p.push_back(wo::grammar::rule{
                grammar::nt(woolang_id_nonterm_list[origin_p[0]],origin_p[1]),
                rule_symlist
                });
        }
    }
}

#endif
