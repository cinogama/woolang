#include "wo_afx.hpp"

#include "wo_ast_base.hpp"

namespace wo
{
    namespace
    {
        constexpr size_t PARSER_AST_ALLOCATE_PAGE_SIZE = 1024 * 1024;
    }

    using namespace ast;
    using namespace std;
#ifndef WO_DISABLE_COMPILER
    AstAllocator::AstAllocator()
        // Make sure new page create when first time alloc node.
        : m_allocated_offset_in_page(PARSER_AST_ALLOCATE_PAGE_SIZE)
    {
    }
    AstAllocator::~AstAllocator()
    {
        for (auto astnode : m_created_ast_nodes)
            delete astnode;

        for (auto page : m_allocated_pages)
            free(page);
    }
    void* AstAllocator::allocate_space_for_ast_node(size_t sz)
    {
        return allocate_space_with_aligh_for_ast_node(
            sz, align_val_t{ alignof(max_align_t) });
    }
    void* AstAllocator::allocate_space_with_aligh_for_ast_node(size_t sz, align_val_t al)
    {
        size_t al_sz = static_cast<size_t>(al);
        wo_assert((al_sz & (al_sz - 1)) == 0); // al_sz is power of 2
        wo_assert(sz <= PARSER_AST_ALLOCATE_PAGE_SIZE);

        size_t alligned_next_offset = (m_allocated_offset_in_page + (al_sz - 1)) & (~(al_sz - 1));
        if (alligned_next_offset + sz > PARSER_AST_ALLOCATE_PAGE_SIZE)
        {
            char* new_page = reinterpret_cast<char*>(malloc(PARSER_AST_ALLOCATE_PAGE_SIZE));
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
    void AstAllocator::swap(AstAllocator& another)
    {
        m_allocated_pages.swap(another.m_allocated_pages);
        m_created_ast_nodes.swap(another.m_created_ast_nodes);
        std::swap(
            m_allocated_offset_in_page,
            another.m_allocated_offset_in_page);
    }
    const vector<AstBase*>& AstAllocator::get_allocated_nodes_for_lspv2() const
    {
        return m_created_ast_nodes;
    }
    bool AstAllocator::empty() const
    {
        return m_allocated_pages.empty() && m_created_ast_nodes.empty();
    }

    AstBase* lexer::merge_imported_ast_trees(AstBase* node)
    {
        wo_assert(node != nullptr);

        if (!m_imported_ast_tree_list.empty())
        {
            auto* merged_list = new AstList();

            for (auto* imported_ast : m_imported_ast_tree_list)
            {
                merged_list->m_list.push_back(imported_ast);

                // NOTE: Generate an `nop` for debug info gen, avoid ip/cr conflict
                merged_list->m_list.push_back(new AstNop());
            }
            merged_list->m_list.push_back(node);

            return merged_list;
        }
        return node;
    }
#endif
}
