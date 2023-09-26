#pragma once
#include "wo_memory.hpp"
#include "wo_assert.hpp"

#include <shared_mutex>
#include <atomic>

namespace wo
{
    struct vmbase;
    namespace gc
    {
        void gc_start();
        bool gc_is_marking();
        bool gc_is_recycling();
        void mark_vm(vmbase* marking_vm, size_t worker_id);
    }

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

    struct value;

    struct gcbase
    {
        // TODO : _shared_spin need to remake.
        struct _shared_spin
        {
            std::atomic_flag _sspin_write_flag = {};
            std::atomic_uint _sspin_read_flag = {};

            inline bool try_lock() noexcept
            {
                if (_sspin_write_flag.test_and_set())
                    return false;
                if (_sspin_read_flag)
                {
                    _sspin_write_flag.clear();
                    return false;
                }
                return true;
            }

            inline void lock() noexcept
            {
                while (_sspin_write_flag.test_and_set());
                while (_sspin_read_flag);
            }
            inline void unlock() noexcept
            {
                _sspin_write_flag.clear();
            }
            inline void lock_shared() noexcept
            {
                while (_sspin_write_flag.test_and_set());
                _sspin_read_flag.fetch_add(1);
                _sspin_write_flag.clear();
            }
            inline void unlock_shared() noexcept
            {
                _sspin_read_flag.fetch_sub(1);
            }
        };

        struct gc_read_guard
        {
            gcbase* _mx;
            inline gc_read_guard(gcbase* sp)
                :_mx(sp)
            {
                _mx->read();
            }
            inline ~gc_read_guard()
            {
                _mx->read_end();
            }
        };

        struct gc_write_guard
        {
            gcbase* _mx;
            inline gc_write_guard(gcbase* sp)
                :_mx(sp)
            {
                _mx->write();
            }
            inline ~gc_write_guard()
            {
                _mx->write_end();
            }
        };

        enum class gctype : uint8_t
        {
            no_gc= 0,
            young,
            old,
        };
        enum class gcmarkcolor : uint8_t
        {
            no_mark = 0,
            self_mark,
            full_mark,
        };

        using rw_lock = _shared_spin;
        
        union unit_attrib
        {
            struct
            {
                uint8_t m_gc_age : 4;
                uint8_t m_marked : 2;
                uint8_t m_alloc_mask : 1;
                uint8_t m_nogc : 1;
            };
            womem_attrib_t m_attr;
        };        
        static_assert(sizeof(unit_attrib) == 1);

        struct memo_unit
        {
            gcbase* gcunit;
            unit_attrib* gcunit_attr;
            memo_unit* last;
        };

        rw_lock gc_read_write_mx;

#ifndef NDEBUG
        bool gc_destructed = false;
        const char* gc_typename = nullptr;
#endif

        inline void write()
        {
            gc_read_write_mx.lock();
        }
        inline void write_end()
        {
            gc_read_write_mx.unlock();
        }
        inline void read()
        {
            gc_read_write_mx.lock_shared();
        }
        inline void read_end()
        {
            gc_read_write_mx.unlock_shared();
        }

        static void write_barrier(const value* val);

        virtual ~gcbase() 
        {
#ifndef NDEBUG
            gc_destructed = true;
#endif
        };

        inline static std::atomic_uint32_t gc_new_count = 0;
    };

    template<typename T>
    struct gcunit : public gcbase, public T
    {
        template<gcbase::gctype AllocType, typename ... ArgTs >
        static gcunit<T>* gc_new(ArgTs && ... args)
        {
            ++gc_new_count;

            gcbase::unit_attrib a;
            a.m_gc_age = AllocType == gcbase::gctype::young ? (uint8_t)0x0F : (uint8_t)0;
            a.m_marked = 0;
            a.m_nogc = AllocType == gcbase::gctype::no_gc ? (uint8_t)0x01 : (uint8_t)0;
            a.m_alloc_mask = 0;

            auto* created_gcnuit = new (alloc64(sizeof(gcunit<T>), a.m_attr))gcunit<T>(args...);

#ifndef NDEBUG
            created_gcnuit->gc_typename = typeid(T).name();
#endif
            return created_gcnuit;
        }

        template<typename ... ArgTs>
        gcunit(ArgTs && ... args) : T(args...)
        {

        }

        T* elem()
        {
            return static_cast<T*>(this);
        }
    };
}