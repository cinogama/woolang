#pragma once
#include "wo_memory.hpp"
#include "wo_assert.hpp"

#include <shared_mutex>
#include <atomic>

namespace wo
{
    class vmbase;
    struct value;

    namespace gc
    {
        void gc_start();
        void gc_stop();
        bool gc_is_marking();
        bool gc_is_collecting_memo();
        bool gc_is_recycling();
        void mark_vm(vmbase* marking_vm, size_t worker_id);
    }

    namespace pin
    {
        wo_pin_value create_pin_value();
        void set_pin_value(wo_pin_value pin_value, value* val);
        void set_dup_pin_value(wo_pin_value pin_value, value* val);
        void close_pin_value(wo_pin_value pin_value);
        void read_pin_value(value* out_value, wo_pin_value pin_value);
    }
    namespace weakref
    {
        wo_weak_ref create_weak_ref(value* val);
        void close_weak_ref(wo_weak_ref weak_ref);
        bool lock_weak_ref(value* out_value, wo_weak_ref weak_ref);
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
        struct _shared_spin
        {
            std::atomic<bool> write_lock = ATOMIC_VAR_INIT(false);
            std::atomic<unsigned> readers = ATOMIC_VAR_INIT(0);

            static void spin_loop_hint()
            {
                // If in msvc
#if defined(_MSC_VER) && _MSC_VER >= 1900
                _mm_pause();
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__aarch64__) || defined(_M_ARM64)
                __asm__ __volatile__("yield");
#elif defined(__x86_64__) || defined(_M_X64)
                __asm__ __volatile__("pause");
#else
                std::this_thread::yield();
#endif
#else
                // No specific pause instruction available, use a generic hint
                std::this_thread::yield();
#endif
            }

            bool try_lock() noexcept
            {
                if (write_lock.exchange(true, std::memory_order_acquire))
                    return false;

                if (readers.load(std::memory_order_acquire) != 0) {
                    write_lock.store(false, std::memory_order_release);
                    return false;
                }
                return true;
            }
            void lock() noexcept
            {
                bool expected = false;
                while (!write_lock.compare_exchange_weak(expected, true,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                    expected = false;
                }

                while (readers.load(std::memory_order_acquire) != 0)
                    spin_loop_hint();
            }
            void unlock() noexcept
            {
                write_lock.store(false, std::memory_order_release);
            }
            void lock_shared() noexcept
            {
                while (true) {
                    while (write_lock.load(std::memory_order_acquire))
                        spin_loop_hint();

                    readers.fetch_add(1, std::memory_order_acq_rel);
                    if (!write_lock.load(std::memory_order_acquire))
                        break;

                    readers.fetch_sub(1, std::memory_order_release);
                }
            }
            void unlock_shared() noexcept
            {
                readers.fetch_sub(1, std::memory_order_release);
            }
        };
        struct gc_mark_read_guard
        {
            gcbase* _mx;

            gc_mark_read_guard(const gc_mark_read_guard&) = delete;
            gc_mark_read_guard(gc_mark_read_guard&&) = delete;
            gc_mark_read_guard& operator=(const gc_mark_read_guard&) = delete;
            gc_mark_read_guard& operator=(gc_mark_read_guard&&) = delete;

            inline gc_mark_read_guard(gcbase* sp)
                :_mx(sp)
            {
                _mx->read();
            }
            inline ~gc_mark_read_guard()
            {
                _mx->read_end();
            }
        };
        struct gc_modify_write_guard
        {
            gcbase* _mx;

            gc_modify_write_guard(const gc_modify_write_guard&) = delete;
            gc_modify_write_guard(gc_modify_write_guard&&) = delete;
            gc_modify_write_guard& operator=(const gc_modify_write_guard&) = delete;
            gc_modify_write_guard& operator=(gc_modify_write_guard&&) = delete;

            inline gc_modify_write_guard(gcbase* sp)
                :_mx(sp)
            {
                _mx->write();
            }
            inline ~gc_modify_write_guard()
            {
                _mx->write_end();
            }
        };

#if WO_FORCE_GC_OBJ_THREAD_SAFETY
        using gc_read_guard = gc_mark_read_guard;
        using gc_write_guard = gc_modify_write_guard;
#else
        struct gc_non_lock_guard
        {
            inline gc_non_lock_guard(gcbase* _)
            {
            }
            inline ~gc_non_lock_guard()
            {
            }
        };
        using gc_read_guard = gc_non_lock_guard;
        using gc_write_guard = gc_non_lock_guard;
#endif

        enum class gctype : uint8_t
        {
            no_gc = 0,
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

#if WO_ENABLE_RUNTIME_CHECK
        const char* gc_typename = nullptr;
        bool gc_destructed = false;
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

        virtual ~gcbase();

        inline static uint32_t gc_new_releax_count = 0;
    };

    template<typename T>
    struct gcunit : public gcbase, public T
    {
        template<gcbase::gctype AllocType, typename ... ArgTs >
        static gcunit<T>* gc_new(ArgTs && ... args)
        {
            ++gc_new_releax_count;

            // TODO: Optimize this.
            gcbase::unit_attrib a;
            a.m_gc_age = AllocType == gcbase::gctype::young ? (uint8_t)0x0F : (uint8_t)0;
            a.m_marked = 0;
            a.m_nogc = AllocType == gcbase::gctype::no_gc ? (uint8_t)0x01 : (uint8_t)0;
            a.m_alloc_mask = 0;

            auto* created_gcnuit = new (alloc64(sizeof(gcunit<T>), a.m_attr))gcunit<T>(args...);

#if WO_ENABLE_RUNTIME_CHECK
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