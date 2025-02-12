#define WO_NEED_LSP_API 1

#include "wo.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    wo_init(argc, argv);

    int ret = 0;

    auto* meta = wo_lspv2_compile_to_meta("test.wo", nullptr);
    {
        auto* iter = wo_lspv2_meta_expr_collection_iter(meta);
        for (;;)
        {
            auto* expr_collect = wo_lspv2_expr_collection_next(iter);
            if (expr_collect == nullptr)
                break;

            auto* expr_collect_info = wo_lspv2_expr_collection_get_info(expr_collect);
            {
                printf("%s\n", expr_collect_info->m_file_name);

                if (strcmp("D:/SELF/woolang/msvc/driver/test.wo", expr_collect_info->m_file_name) == 0)
                {
                    auto* expr_iter = wo_lspv2_expr_collection_get_by_range(expr_collect, 9, 15, 9, 18);
                    for (;;)
                    {
                        auto* expr = wo_lspv2_expr_next(expr_iter);
                        if (expr == nullptr)
                            break;

                        auto* expr_info = wo_lspv2_expr_get_info(expr);
                        {
                            auto* type_info = wo_lspv2_type_get_info(expr_info->m_type, meta);
                            {
                                printf("  %s\n", type_info->m_name);
                            }
                            wo_lspv2_type_info_free(type_info);
                        }
                        wo_lspv2_expr_info_free(expr_info);

                        wo_lspv2_expr_free(expr);
                    }
                }
            }
            wo_lspv2_expr_collection_info_free(expr_collect_info);

            wo_lspv2_expr_collection_free(expr_collect);
        }
    }
    wo_lspv2_free_meta(meta);

    wo_finish(nullptr, nullptr);

    return ret;
}
