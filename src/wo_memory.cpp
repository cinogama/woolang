#define WOMEM_IMPL

#include "wo_memory.hpp"
#include "wo_gc.hpp"

#include <cassert>
#include <cstdio>
#include <atomic>
#include <list>
#include <vector>
#include <mutex>
#include <algorithm>

#if WO_BUILD_WITH_MINGW
#include <mingw.mutex.h>
#endif

#ifdef WIN32
#   include <Windows.h>
#   undef min
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

size_t WO_SYS_MEM_PAGE_SIZE;

size_t _womem_page_size()
{
#ifdef WIN32
    SYSTEM_INFO s;
    GetSystemInfo(&s);
    return s.dwPageSize;
#else
    return getpagesize();
#endif
}

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
    struct PageUnitHead
    {
        std::atomic_uint8_t m_in_used_flag;

        uint8_t m_belong_chunk;
        uint8_t __m_reserved;
        womem_attrib_t m_attrib;

        uint16_t m_last_free_idx;

        uint16_t __m_reserved2;
    };
    static_assert(sizeof(PageUnitHead) == 8);

    struct Page
    {
        std::atomic_uint8_t m_free_page;

        // Alloc size of unit in this page
        uint8_t m_page_unit_size;

        // Allocable unit offset
        uint16_t m_free_offset_idx;

        // Max unit
        uint16_t m_max_avliable_unit_count;

        // Alloc count, used for reuse page.
        std::atomic_uint16_t m_alloc_count;

        union
        {
            Page* last;

            // Useless, make sure m_chunkdata begin at 8 byte allign place
            int64_t _;
        };

        // Init current page
        void init(uint8_t chunkid, uint8_t sz)
        {
            assert(sz >= 8);
            assert(m_page_unit_size == 0 || m_page_unit_size == sz);

            m_max_avliable_unit_count =
                (uint16_t)((WO_SYS_MEM_PAGE_SIZE - 8 - sizeof(Page*)) / ((size_t)sz + sizeof(PageUnitHead)));

            m_free_offset_idx = m_max_avliable_unit_count;

            char* buf = m_chunkdata;
            uint16_t* p_last_free_idx = &m_free_offset_idx;
            for (uint16_t i = 0; i < m_max_avliable_unit_count; ++i)
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

        char m_chunkdata[1];
    };
    static_assert(offsetof(Page, m_chunkdata) % 8 == 0);

    class Chunk
    {
    public:
        enum AllocGroup :uint8_t
        {
            L16,
            L24,
            L32,
            L40,
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

        char* const m_virtual_memory;
        size_t m_commited_page_count;

        const size_t m_chunk_size;
        const size_t m_max_page;
        const uint8_t m_chunk_id;

        wo::atomic_list<Page> m_released_page;

        std::mutex m_free_pages_mx;
        Page* m_free_pages[AllocGroup::COUNT + 1] = {};
    public:

        static constexpr AllocGroup eval_alloc_group(uint8_t sz)
        {
#define WOMEM_CASE(N) if (sz <= N) return AllocGroup::L##N

            WOMEM_CASE(16);
            WOMEM_CASE(24);
            WOMEM_CASE(32);
            WOMEM_CASE(40);
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

            WOMEM_CASE(16);
            WOMEM_CASE(24);
            WOMEM_CASE(32);
            WOMEM_CASE(40);
            WOMEM_CASE(48);
            WOMEM_CASE(64);
            WOMEM_CASE(96);
            WOMEM_CASE(128);

#undef WOMEM_CASE
            return 0;
        }

        Chunk(size_t chunk_size, uint8_t cid)
            : m_virtual_memory(nullptr)
            , m_commited_page_count(0)
            , m_chunk_size(0)
            , m_max_page(0)
            , m_chunk_id(cid)
        {
            size_t reserving_chunk_size = chunk_size;
            char* reserved_chunk_buffer = nullptr;
            for (;;)
            {
                reserving_chunk_size = reserving_chunk_size / WO_SYS_MEM_PAGE_SIZE * WO_SYS_MEM_PAGE_SIZE;

                if (reserving_chunk_size == 0)
                {
                    fprintf(stderr, "Failed to reserve memory, reason: %d.\n",
                        _womem_get_last_error());

                    wo_error("Failed to reserve memory.");
                }

                reserved_chunk_buffer = (char*)_womem_reserve_mem(reserving_chunk_size);

                if (reserved_chunk_buffer != nullptr)
                    break;

                wo_warning("Failed to reserve gc-managed-memory, retry.");
                reserving_chunk_size = reserving_chunk_size / 2;
            }

            const_cast<size_t&>(m_max_page) = reserving_chunk_size / WO_SYS_MEM_PAGE_SIZE;
            const_cast<size_t&>(m_chunk_size) = reserving_chunk_size;
            const_cast<char*&>(m_virtual_memory) = reserved_chunk_buffer;

            assert(m_chunk_size % WO_SYS_MEM_PAGE_SIZE == 0);
            assert(m_max_page <= UINT32_MAX);
            assert(m_virtual_memory != nullptr);
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

                if (!_womem_decommit_mem(cur_page, WO_SYS_MEM_PAGE_SIZE))
                {
                    fprintf(stderr, "Failed to decommit released page: %p(%d).\n",
                        cur_page, _womem_get_last_error());
                    abort();
                }
            }
            for (size_t i = (size_t)AllocGroup::L16; i < (size_t)AllocGroup::COUNT; ++i)
            {
                auto*& pages = m_free_pages[i];
                while (pages)
                {
                    auto* cur_page = pages;
                    pages = pages->last;
                    if (cur_page->m_alloc_count != 0)
                    {
                        fprintf(stderr, "Free page(Group: %d): %p(%dbyte/%dtotal) still alive %d unit.\n",
                            (int)i, cur_page,
                            (int)cur_page->m_page_unit_size,
                            (int)cur_page->m_max_avliable_unit_count,
                            (int)cur_page->m_alloc_count);

                        for (uint8_t i = 0; i < cur_page->m_max_avliable_unit_count; ++i)
                        {
                            PageUnitHead* head = (PageUnitHead*)((char*)cur_page->m_chunkdata
                                + (size_t)i * ((size_t)cur_page->m_page_unit_size + sizeof(PageUnitHead)));

                            if (head->m_in_used_flag)
                                fprintf(stderr, "  %d: %p in used, attrib: %x.\n", (int)i, head + 1, (int)head->m_attrib);
                        }
                    }

                    if (!_womem_decommit_mem(cur_page, WO_SYS_MEM_PAGE_SIZE))
                    {
                        fprintf(stderr, "Failed to decommit page: %p(%d).\n",
                            cur_page, _womem_get_last_error());
                        abort();
                    }
                }
            }

            if (!_womem_release_mem(m_virtual_memory, m_chunk_size))
            {
                fprintf(stderr, "Failed to free chunk: %p(%d).\n",
                    m_virtual_memory, _womem_get_last_error());
                abort();
            }
        }

        Page* _alloc_page(uint8_t elem_sz, AllocGroup group)
        {
            if (nullptr != m_free_pages[group])
            {
                auto* p = m_free_pages[group];
                m_free_pages[group] = p->last;
                return p;
            }
            if (nullptr != m_free_pages[AllocGroup::FREEPAGE])
            {
                auto* p = m_free_pages[AllocGroup::FREEPAGE];
                m_free_pages[AllocGroup::FREEPAGE] = p->last;
                p->m_page_unit_size = 0;
                p->m_alloc_count = 0;
                p->init(m_chunk_id, eval_alloc_size(elem_sz));
                return p;
            }

            // No useable page, commit to OS
            if (m_commited_page_count >= m_max_page)
                return nullptr;

            Page* new_p = (Page*)(m_virtual_memory + m_commited_page_count++ * WO_SYS_MEM_PAGE_SIZE);
            if (!_womem_commit_mem(new_p, WO_SYS_MEM_PAGE_SIZE))
            {
                fprintf(stderr, "Failed to commit page: %p(%d).\n",
                    new_p, _womem_get_last_error());
                abort();
            }
            new_p->m_page_unit_size = 0;
            new_p->m_alloc_count = 0;
            new_p->m_free_page = 1;
            new_p->init(m_chunk_id, eval_alloc_size(elem_sz));
            return new_p;
        }
        Page* alloc_pages(uint8_t elem_sz, AllocGroup group, size_t alloc_page_count)
        {
            std::lock_guard g1(m_free_pages_mx);

            Page* last = nullptr;
            for (size_t i = 0; i < alloc_page_count; ++i)
            {
                if (auto* page = _alloc_page(elem_sz, group))
                {
                    page->last = last;
                    last = page;
                }
                else
                    break;
            }
            return last;
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
                        cur_page->last = m_free_pages[AllocGroup::FREEPAGE];
                        m_free_pages[AllocGroup::FREEPAGE] = cur_page;
                    }
                    else
                    {
                        cur_page->init(m_chunk_id, cur_page->m_page_unit_size);
                        auto alloc_group = eval_alloc_group(cur_page->m_page_unit_size);
                        cur_page->last = m_free_pages[alloc_group];
                        m_free_pages[alloc_group] = cur_page;
                    }
                }
                else
                    m_released_page.add_one(cur_page);
            }

            if (full)
            {
                for (size_t i = (size_t)AllocGroup::L16; i < (size_t)AllocGroup::COUNT; ++i)
                {
                    auto* pages = m_free_pages[i];
                    m_free_pages[i] = nullptr;

                    while (pages)
                    {
                        auto* cur_page = pages;
                        pages = pages->last;

                        if (cur_page->m_alloc_count == 0)
                        {
                            // Currne page should move to free pages.
                            cur_page->last = m_free_pages[AllocGroup::FREEPAGE];
                            m_free_pages[AllocGroup::FREEPAGE] = cur_page;
                        }
                        else
                        {
                            // Add it back
                            cur_page->last = m_free_pages[i];
                            m_free_pages[i] = cur_page;
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
                auto pageid = (size_t)offset / WO_SYS_MEM_PAGE_SIZE;
                if (pageid < m_commited_page_count)
                {
                    return (Page*)(m_virtual_memory + pageid * WO_SYS_MEM_PAGE_SIZE);
                }
            }
            return nullptr;
        }

        void* enum_page(size_t* out_page_count, size_t* out_page_size)
        {
            std::lock_guard g1(m_free_pages_mx);

            *out_page_count = m_commited_page_count;
            *out_page_size = WO_SYS_MEM_PAGE_SIZE;
            return m_virtual_memory;
        }
    };

    static womem::Chunk* _global_chunk = nullptr;

    // ThreadCache is page cache in thread local
    class ThreadCache
    {
        size_t m_prepare_alloc_page_reserve_count[Chunk::AllocGroup::COUNT];
        Page* m_prepare_alloc_pages[Chunk::AllocGroup::COUNT] = {};
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
            for (auto*& pages : m_prepare_alloc_pages)
            {
                while (pages)
                {
                    auto* cur_page = pages;
                    pages = pages->last;

                    _global_chunk->release_page(cur_page);
                }
            }
        }
        void* alloc(size_t sz, womem_attrib_t attrib)
        {
            auto group = Chunk::eval_alloc_group((uint8_t)sz);
            auto** pages = &m_prepare_alloc_pages[group];
            if (nullptr == *pages)
            {
                auto& preserve = m_prepare_alloc_page_reserve_count[group];
                preserve = std::min((size_t)512, preserve * 2);

                *pages = _global_chunk->alloc_pages((uint8_t)sz, group, preserve);
            }
            if (*pages == nullptr)
            {
                // Failed
                return nullptr;
            }
            auto* ptr = (*pages)->alloc(attrib);
            if ((*pages)->m_free_offset_idx >= (*pages)->m_max_avliable_unit_count)
            {
                // Page ran out
                auto* abondon_page = *pages;
                *pages = abondon_page->last;
                _global_chunk->release_page(abondon_page);
            }

            return ptr;
        }
    };
    static thread_local ThreadCache _tls_thread_cache_page_pool;
}

void womem_init(size_t virtual_pre_alloc_size)
{
    WO_SYS_MEM_PAGE_SIZE = _womem_page_size();
    wo_assert(WO_SYS_MEM_PAGE_SIZE / (8 + 16) <= UINT16_MAX);
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
void womem_tidy_pages(bool full)
{
    womem::_global_chunk->tidy_pages(full);
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
        *unit_size = sizeof(womem::PageUnitHead) + (size_t)p->m_page_unit_size;

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