#define WOMEM_IMPL

#include "wo_memory.hpp"

#include <cassert>
#include <cstdio>
#include <atomic>
#include <list>
#include <vector>
#include <mutex>
#include <algorithm>

#ifdef WIN32
#   include <Windows.h>
#   undef min
#else
#include <sys/mman.h>
#endif

void* _womem_reserve_mem(size_t sz)
{
#ifdef WIN32
    return VirtualAlloc(nullptr, sz, MEM_RESERVE, PAGE_NOACCESS);
#else
    return mmap(nullptr, sz, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
}
bool _womem_commit_mem(void* mem, size_t sz)
{
#ifdef WIN32
    return nullptr != VirtualAlloc(mem, sz, MEM_COMMIT, PAGE_READWRITE);
#else
    return 0 == mprotect(mem, sz, PROT_READ | PROT_WRITE);
#endif
}
bool _womem_decommit_mem(void* mem, size_t sz)
{
#ifdef WIN32
    return 0 != VirtualFree(mem, sz, MEM_DECOMMIT);
#else
    return 0 == mprotect(mem, sz, PROT_NONE);
#endif
}
bool _womem_release_mem(void* mem, size_t sz)
{
#ifdef WIN32
    return 0 != VirtualFree(mem, 0, MEM_RELEASE);
#else
    return 0 == munmap(mem, sz);
#endif
}
int _womem_get_last_error(void)
{
#ifdef WIN32
    return GetLastError();
#else
    return errno;
#endif
}

namespace womem
{
    template<typename NodeT>
    struct atomic_list
    {
        std::atomic<NodeT*> last_node = nullptr;

        void add_one(NodeT* node)
        {
            node->last = last_node.load();// .exchange(node);
            while (!last_node.compare_exchange_weak(node->last, node));
        }

        NodeT* pick_all()
        {
            NodeT* result = nullptr;
            result = last_node.exchange(nullptr);

            return result;
        }
        NodeT* peek_list() const
        {
            return last_node.load();
        }
    };

    inline constexpr size_t PAGE_SIZE = 4 * 1024;
    static_assert(PAGE_SIZE / 16 <= 256);

    struct PageUnitHead
    {
        std::atomic_uint8_t m_in_used_flag;

        uint8_t m_belong_chunk;
        uint8_t m_last_free_idx;
        womem_attrib_t m_attrib;

        uint32_t __m_reserved;
    };
    static_assert(sizeof(PageUnitHead) == 8);

    struct Page
    {
        std::atomic_uint8_t m_free_page;

        // Alloc size of unit in this page
        uint8_t m_page_unit_size;

        // Allocable unit offset
        uint8_t m_free_offset_idx;

        uint8_t _reserved;

        // Max unit
        uint8_t m_max_avliable_unit_count;

        // Alloc count, used for reuse page.
        std::atomic_uint8_t m_alloc_count;

        uint16_t __m_reserved2;

        Page* last;

        char m_chunkdata[PAGE_SIZE - 8 - sizeof(Page*)];

        // Init current page
        void init(uint8_t chunkid, uint8_t sz)
        {
            assert(sz >= 8);
            assert(m_page_unit_size == 0 || m_page_unit_size == sz);

            m_max_avliable_unit_count =
                (uint8_t)((PAGE_SIZE - 8 - sizeof(Page*)) / ((size_t)sz + sizeof(PageUnitHead)));

            m_free_offset_idx = m_max_avliable_unit_count;

            char* buf = m_chunkdata;
            uint8_t* p_last_free_idx = &m_free_offset_idx;
            for (uint8_t i = 0; i < m_max_avliable_unit_count; ++i)
            {
                auto* head = (PageUnitHead*)(buf + (size_t)i * ((size_t)sz + sizeof(PageUnitHead)));
                head->m_belong_chunk = chunkid;

                if (!head->m_in_used_flag || m_page_unit_size == 0)
                {
                    head->m_in_used_flag = 0;
                    *p_last_free_idx = i;
                    head->m_last_free_idx = *p_last_free_idx;
                    p_last_free_idx = &head->m_last_free_idx;
                }
            }
            *p_last_free_idx = m_max_avliable_unit_count;
            m_page_unit_size = sz;
            m_free_page = 0;

            assert(m_alloc_count < m_max_avliable_unit_count);
            assert(m_free_offset_idx < m_max_avliable_unit_count);
        }

        // Alloc
        void* alloc(womem_attrib_t attrib)
        {
            assert(m_alloc_count < m_max_avliable_unit_count);
            assert(m_free_offset_idx < m_max_avliable_unit_count);

            ++m_alloc_count;

            char* buf = m_chunkdata;
            auto* head = (PageUnitHead*)(buf + (size_t)m_free_offset_idx * ((size_t)m_page_unit_size + sizeof(PageUnitHead)));
            m_free_offset_idx = head->m_last_free_idx;

            assert(head->m_in_used_flag == 0);
            head->m_attrib = attrib;
            head->m_in_used_flag = 1;
            return head + 1; // Return!
        }
    };
    static_assert(sizeof(Page) == PAGE_SIZE);
    static_assert(offsetof(Page, m_chunkdata) % 8 == 0);

    class Chunk
    {
    public:
        enum AllocGroup :uint8_t
        {
            L8,
            L16,
            L24,
            L32,
            L48,
            L64,
            L96,
            L128,
            COUNT,
            FREEPAGE = COUNT,
            BAD,
        };

    private:
        Chunk(const Chunk&) = delete;
        Chunk(Chunk&&) = delete;
        Chunk& operator = (const Chunk&) = delete;
        Chunk operator = (Chunk&&) = delete;

        char* m_virtual_memory = nullptr;
        size_t m_commited_page_count = 0;

        const size_t m_chunk_size;
        const size_t m_max_page;
        const uint8_t m_chunk_id;

        atomic_list<Page> m_released_page;

        std::mutex m_free_pages_mx;
        std::list<Page*> m_free_pages[AllocGroup::COUNT + 1];
    public:

        static constexpr AllocGroup eval_alloc_group(uint8_t sz)
        {
#define WOMEM_CASE(N) if (sz <= N) return AllocGroup::L##N

            WOMEM_CASE(8);
            WOMEM_CASE(16);
            WOMEM_CASE(24);
            WOMEM_CASE(32);
            WOMEM_CASE(48);
            WOMEM_CASE(64);
            WOMEM_CASE(96);
            WOMEM_CASE(128);

#undef WOMEM_CASE

            return AllocGroup::BAD;
        }
        static constexpr uint8_t eval_alloc_size(uint8_t sz)
        {
#define WOMEM_CASE(N) if (sz <= N) return N

            WOMEM_CASE(8);
            WOMEM_CASE(16);
            WOMEM_CASE(24);
            WOMEM_CASE(32);
            WOMEM_CASE(48);
            WOMEM_CASE(64);
            WOMEM_CASE(96);
            WOMEM_CASE(128);

#undef WOMEM_CASE
            return 0;
        }

        Chunk(size_t chunk_size, uint8_t cid)
            : m_max_page(chunk_size / PAGE_SIZE)
            , m_chunk_id(cid)
            , m_chunk_size(chunk_size)
        {
            assert(chunk_size % PAGE_SIZE == 0);
            assert(m_max_page <= UINT32_MAX);


            m_virtual_memory = (char*)_womem_reserve_mem(m_chunk_size);

            if (m_virtual_memory == nullptr)
            {
                fprintf(stderr, "Failed to reserve memory: %d.\n",
                    _womem_get_last_error());
                abort();
            }
        }

        ~Chunk()
        {
            // De commit all pages.
            auto* released_pages = m_released_page.pick_all();
            while (released_pages)
            {
                auto* cur_page = released_pages;
                released_pages = released_pages->last;

                if (cur_page->m_alloc_count != 0)
                {
                    fprintf(stderr, "Released page: %p(%dbyte/%dtotal) still alive %d unit.\n",
                        cur_page, (int)cur_page->m_page_unit_size, (int)cur_page->m_max_avliable_unit_count, (int)cur_page->m_alloc_count);

                    for (uint8_t i = 0; i < cur_page->m_max_avliable_unit_count; ++i)
                    {
                        PageUnitHead* head = (PageUnitHead*)((char*)cur_page->m_chunkdata
                            + (size_t)i * ((size_t)cur_page->m_page_unit_size + sizeof(PageUnitHead)));

                        if (head->m_in_used_flag)
                            fprintf(stderr, "  %d: %p in used, attrib: %x.\n", (int)i, head + 1, (int)head->m_attrib);
                    }
                }

                if (!_womem_decommit_mem(cur_page, PAGE_SIZE))
                {
                    fprintf(stderr, "Failed to decommit released page: %p(%d).\n",
                        cur_page, _womem_get_last_error());
                    abort();
                }
            }
            for (size_t i = (size_t)AllocGroup::L8; i < (size_t)AllocGroup::COUNT; ++i)
            {
                auto& pages = m_free_pages[i];
                for (auto* page : pages)
                {
                    if (page->m_alloc_count != 0)
                    {
                        fprintf(stderr, "Free page(Group: %d): %p(%dbyte/%dtotal) still alive %d unit.\n",
                            (int)i, page, (int)page->m_page_unit_size, (int)page->m_max_avliable_unit_count, (int)page->m_alloc_count);

                        for (uint8_t i = 0; i < page->m_max_avliable_unit_count; ++i)
                        {
                            PageUnitHead* head = (PageUnitHead*)((char*)page->m_chunkdata
                                + (size_t)i * ((size_t)page->m_page_unit_size + sizeof(PageUnitHead)));

                            if (head->m_in_used_flag)
                                fprintf(stderr, "  %d: %p in used, attrib: %x.\n", (int)i, head + 1, (int)head->m_attrib);
                        }
                    }

                    if (!_womem_decommit_mem(page, PAGE_SIZE))
                    {
                        fprintf(stderr, "Failed to decommit page: %p(%d).\n",
                            page, _womem_get_last_error());
                        abort();
                    }
                }
            }
            m_free_pages->clear();

            if (!_womem_release_mem(m_virtual_memory, m_chunk_size))
            {
                fprintf(stderr, "Failed to free chunk: %p(%d).\n",
                    m_virtual_memory, _womem_get_last_error());
                abort();
            }
        }

        Page* _alloc_page(uint8_t elem_sz)
        {
            auto group = eval_alloc_group(elem_sz);

            std::lock_guard g1(m_free_pages_mx);
            do
            {
                if (!m_free_pages[group].empty())
                {
                    auto* p = m_free_pages[group].front();
                    m_free_pages[group].pop_front();
                    return p;
                }
                if (!m_free_pages[AllocGroup::FREEPAGE].empty())
                {
                    auto* p = m_free_pages[AllocGroup::FREEPAGE].front();
                    m_free_pages[AllocGroup::FREEPAGE].pop_front();
                    p->m_page_unit_size = 0;
                    p->m_alloc_count = 0;
                    return p;
                }
            } while (0);

            // No useable page, commit to OS
            if (m_commited_page_count >= m_max_page)
                return nullptr;

            Page* new_p = (Page*)m_virtual_memory + m_commited_page_count++;
            if (!_womem_commit_mem(new_p, PAGE_SIZE))
            {
                fprintf(stderr, "Failed to commit page: %p(%d).\n",
                    new_p, _womem_get_last_error());
                abort();
            }
            new_p->m_page_unit_size = 0;
            new_p->m_alloc_count = 0;
            new_p->m_free_page = 1;
            return new_p;
        }
        Page* alloc_page(uint8_t elem_sz)
        {
            auto* p = _alloc_page(elem_sz);
            if (p != nullptr)
                p->init(m_chunk_id, eval_alloc_size(elem_sz));
            return p;
        }

        void release_page(Page* page)
        {
            m_released_page.add_one(page);
        }
        void tidy_pages(bool full)
        {
            std::lock_guard g1(m_free_pages_mx);

            auto* pages = m_released_page.pick_all();
            while (pages)
            {
                auto* cur_page = pages;
                pages = pages->last;

                if (cur_page->m_alloc_count < cur_page->m_max_avliable_unit_count)
                {
                    if (cur_page->m_alloc_count == 0)
                    {
                        cur_page->m_free_page = 1;
                        m_free_pages[AllocGroup::FREEPAGE].push_back(cur_page);
                    }
                    else
                        m_free_pages[eval_alloc_group(cur_page->m_page_unit_size)].push_back(cur_page);
                }
                else
                    m_released_page.add_one(cur_page);
            }

            if (full)
            {
                for (size_t i = (size_t)AllocGroup::L8; i < (size_t)AllocGroup::COUNT; ++i)
                {
                    auto e = m_free_pages[i].end();
                    for (auto iter = m_free_pages[i].begin(); iter != e;)
                    {
                        auto c = iter++;
                        if ((*c)->m_alloc_count == 0)
                        {
                            m_free_pages[AllocGroup::FREEPAGE].push_back(*c);
                            m_free_pages[i].erase(c);
                        }
                    }
                }
            }
        }

        Page* find_page(void* unit)
        {
            auto offset = (char*)unit - m_virtual_memory;
            if (offset >= 0 && (size_t)offset < m_chunk_size)
            {
                auto pageid = (size_t)offset / PAGE_SIZE;
                if (pageid < m_commited_page_count)
                {
                    return (Page*)m_virtual_memory + pageid;
                }
            }
            return nullptr;
        }

        void* enum_page(size_t* out_page_count, size_t* out_page_size)
        {
            std::lock_guard g1(m_free_pages_mx);

            *out_page_count = m_commited_page_count;
            *out_page_size = sizeof(Page);
            return m_virtual_memory;
        }
    };

    static womem::Chunk* _global_chunk = nullptr;

    // ThreadCache is page cache in thread local
    class ThreadCache
    {
        uint8_t m_prepare_alloc_page_reserve_count[Chunk::AllocGroup::COUNT];
        std::list<Page*> m_prepare_alloc_page[Chunk::AllocGroup::COUNT];
    public:
        ThreadCache()
        {
            for (size_t i = 0; i < Chunk::AllocGroup::COUNT; ++i)
                m_prepare_alloc_page_reserve_count[i] = 1;
        }
        ~ThreadCache()
        {
            clear();
        }
        void clear()
        {
            for (auto& pages : m_prepare_alloc_page)
            {
                for (auto* page : pages)
                {
                    if (page != nullptr)
                        _global_chunk->release_page(page);
                }
                pages.clear();
            }
        }
        void* alloc(size_t sz, womem_attrib_t attrib)
        {
            auto group = Chunk::eval_alloc_group((uint8_t)sz);
            auto& pages = m_prepare_alloc_page[group];
            if (pages.empty())
            {
                auto& preserve = m_prepare_alloc_page_reserve_count[group];
                preserve = std::min(128, preserve * 2);

                for (uint8_t i = 0; i < preserve; ++i)
                {
                    auto* pg = _global_chunk->alloc_page((uint8_t)sz);
                    pages.push_back(pg);

                    if (pg == nullptr)
                        break;
                }
            }
            auto* page = pages.front();
            if (page == nullptr)
            {
                // Failed
                pages.pop_front();
                return nullptr;
            }
            auto* ptr = page->alloc(attrib);
            if (page->m_free_offset_idx >= page->m_max_avliable_unit_count)
            {
                // Page ran out
                _global_chunk->release_page(page);
                pages.pop_front();
            }

            return ptr;
        }
    };
    static thread_local ThreadCache _tls_thread_cache_page_pool;
}

void womem_init(size_t virtual_pre_alloc_size)
{
    womem::_global_chunk = new womem::Chunk(virtual_pre_alloc_size, 0);
}
void womem_shutdown()
{
    womem::_tls_thread_cache_page_pool.clear();
    delete womem::_global_chunk;
}

void* womem_alloc(size_t size, womem_attrib_t attrib)
{
    return womem::_tls_thread_cache_page_pool.alloc(size, attrib);
}
void womem_free(void* memptr)
{
    auto* head = (womem::PageUnitHead*)memptr - 1;
    auto* page = womem::_global_chunk->find_page(head);

    assert(page);

    --page->m_alloc_count;
    head->m_in_used_flag = 0;
}
void womem_tidy_pages(int fulltiny)
{
    womem::_global_chunk->tidy_pages(fulltiny != 0);
}
void* womem_verify(void* memptr, womem_attrib_t** attrib)
{
    auto* page = womem::_global_chunk->find_page(memptr);
    if (page)
    {
        auto diff = (char*)memptr - (char*)page->m_chunkdata;
        if (diff > 0)
        {
            auto idx = diff / (sizeof(womem::PageUnitHead) + page->m_page_unit_size);
            auto* head = (womem::PageUnitHead*)((char*)page->m_chunkdata +
                idx * (sizeof(womem::PageUnitHead) + page->m_page_unit_size));

            if (head->m_in_used_flag && memptr == head + 1)
            {
                *attrib = &head->m_attrib;
                return head + 1;
            }
        }
    }
    return nullptr;
}

void* womem_enum_pages(size_t* page_count, size_t* page_size)
{
    return womem::_global_chunk->enum_page(page_count, page_size);
}

void* womem_get_unit_buffer(void* page, size_t* unit_count, size_t* unit_size)
{
    auto* p = std::launder(reinterpret_cast<womem::Page*>(page));
    if (p->m_free_page == 0)
    {
        *unit_count = (size_t)p->m_max_avliable_unit_count;
        *unit_size = sizeof(womem::PageUnitHead*) + (size_t)p->m_page_unit_size;

        // Recheck
        if (p->m_free_page == 0)
            return p->m_chunkdata;
    }
    return nullptr;
}

void* womem_get_unit_ptr_attribute(void* unit, womem_attrib_t** attrib)
{
    auto* p = std::launder(reinterpret_cast<womem::PageUnitHead*>(unit));
    if (p->m_in_used_flag)
    {
        *attrib = &p->m_attrib;
        return p + 1;
    }
    return nullptr;
}