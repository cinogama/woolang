#pragma once

#include <shared_mutex>
#include <atomic>

namespace rs
{
    namespace gc
    {
        uint16_t gc_work_round_count();

        void gc_start();

        void gc_begin(bool full_gc);
    }

    template<typename NodeT>
    struct atomic_list
    {
        std::atomic<NodeT*> last_node = nullptr;

        void add_one(NodeT* node)
        {
            NodeT* last_last_node = last_node;// .exchange(node);
            do
            {
                node->last = last_last_node;
            } while (!last_node.compare_exchange_weak(last_last_node, node));
        }

        NodeT* pick_all()
        {
            NodeT* result = nullptr;
            result = last_node.exchange(nullptr);

            return result;
        }
    };

    struct gcbase
    {
        inline static atomic_list<gcbase> eden_age_gcunit_list;
        inline static atomic_list<gcbase> young_age_gcunit_list;
        inline static atomic_list<gcbase> old_age_gcunit_list;

        struct _shared_spin
        {
            std::atomic_flag _sspin_write_flag = {};
            std::atomic_uint _sspin_read_flag = {};

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
                _sspin_read_flag++;
                _sspin_write_flag.clear();
            }
            inline void unlock_shared() noexcept
            {
                _sspin_read_flag--;
            }
        };

        struct gc_read_guard
        {
            gcbase* _mx;
            gc_read_guard(gcbase* sp)
                :_mx(sp)
            {
                _mx->read();
            }
            ~gc_read_guard()
            {
                _mx->read_end();
            }
        };

        struct gc_write_guard
        {
            gcbase* _mx;
            gc_write_guard(gcbase* sp)
                :_mx(sp)
            {
                _mx->write();
            }
            ~gc_write_guard()
            {
                _mx->write_end();
            }
        };

        enum class gctype : uint8_t
        {
            no_gc,

            eden,
            young,
            old,
        };
        enum class gcmarkcolor : uint8_t
        {
            no_mark,
            self_mark,
            full_mark,
        };

        gctype gc_type = gctype::no_gc;
        gcmarkcolor gc_mark_color = gcmarkcolor::no_mark;
        uint16_t gc_mark_version = 0;
        uint16_t gc_mark_alive_count = 0;

        inline void gc_mark(uint16_t version, gcmarkcolor color)
        {
            if (gc_mark_version != version)
            {
                // first mark
                gc_mark_version = version;
                gc_mark_color = color;

                gc_mark_alive_count++;
            }
            else
            {
                gcmarkcolor aim_color = color;
                while (aim_color > gc_mark_color)
                {
                    static_assert(sizeof(std::atomic<gcmarkcolor>) == sizeof(gcmarkcolor));

                    gcmarkcolor old_color = ((std::atomic<gcmarkcolor>&)gc_mark_color).exchange(aim_color);
                    if (aim_color < old_color)
                    {
                        aim_color = old_color;
                    }
                }
            }
        }

        inline gcmarkcolor gc_marked(uint16_t version)
        {
            if (version == gc_mark_version)
            {
                return gc_mark_color;
            }
            return gc_mark_color = gcmarkcolor::no_mark;
        }

        using rw_lock = _shared_spin;
        std::shared_ptr<rw_lock> gc_read_write_mx = std::make_shared<rw_lock>();
        inline void write()
        {
            gc_read_write_mx->lock();
        }
        inline void write_end()
        {
            gc_read_write_mx->unlock();
        }
        inline void read()
        {
            gc_read_write_mx->lock_shared();
        }
        inline void read_end()
        {
            gc_read_write_mx->unlock_shared();
        }

        // used in linklist;
        gcbase* last = nullptr;

        virtual ~gcbase() = default;


        inline static std::atomic_uint32_t gc_new_count = 0;
    };

    template<typename T>
    struct gcunit : public gcbase, public T
    {


        template<gcbase::gctype AllocType, typename ... ArgTs >
        static gcunit<T>* gc_new(gcbase*& write_aim, ArgTs && ... args)
        {
            ++gc_new_count;

            auto* created_gcnuit = new gcunit<T>(args...);
            created_gcnuit->gc_type = AllocType;

            *reinterpret_cast<std::atomic<gcbase*>*>(&write_aim) = created_gcnuit;

            switch (AllocType)
            {
            case rs::gcbase::gctype::no_gc:
                /* DO NOTHING */
                break;
            case rs::gcbase::gctype::eden:
                eden_age_gcunit_list.add_one(created_gcnuit);
                break;
            case rs::gcbase::gctype::young:
                young_age_gcunit_list.add_one(created_gcnuit);
                break;
            case rs::gcbase::gctype::old:
                old_age_gcunit_list.add_one(created_gcnuit);
                break;
            default:
                // rs_error("Unknown gc type.");
                break;
            }

            return created_gcnuit;
        }

        template<typename ... ArgTs>
        gcunit(ArgTs && ... args) : T(args...)
        {

        }

        template<typename TT>
        inline gcunit& operator = (TT&& _val)
        {
            (*(T*)this) = _val;
            return *this;
        }
    };


}