#include "wo_afx.hpp"

#include "wo_ast_base.hpp"

static constexpr size_t WO_PARSER_AST_ALLOCATE_PAGE_SIZE = 1024 * 1024;

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    ast::AstAllocator::AstAllocator()
        // Make sure new page create when first time alloc node.
        : m_allocated_offset_in_page(WO_PARSER_AST_ALLOCATE_PAGE_SIZE)
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
        wo_assert(sz <= WO_PARSER_AST_ALLOCATE_PAGE_SIZE);

        size_t alligned_next_offset = (m_allocated_offset_in_page + (al_sz - 1)) & (~(al_sz - 1));
        if (alligned_next_offset + sz > WO_PARSER_AST_ALLOCATE_PAGE_SIZE)
        {
            char* new_page = reinterpret_cast<char*>(malloc(WO_PARSER_AST_ALLOCATE_PAGE_SIZE));
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
