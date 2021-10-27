#pragma once

namespace rs
{
    template<typename NodeT>
    struct atomic_list
    {
        NodeT* head_point;

    };

    struct gcbase
    {
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
        std::

        // used in linklist;
        gcbase* next = nullptr;

        virtual ~gcbase() = default;
    };

    template<typename T>
    struct gcunit : public gcbase, public T
    {
        template<typename ... ArgTs>
        gcunit(ArgTs && ... args) : T(args...)
        {

        }

        template<gcbase::gctype AllocType, typename ... ArgTs>
        static gcunit<T>* gc_new(gcunit<T>*& write_aim, ArgTs && ... args)
        {
            write_aim = new gcunit<T>(args...);
            write_aim->gc_type = AllocType;
            return write_aim;
        }

        template<typename TT>
        inline gcunit& operator = (TT&& _val)
        {
            (*(T*)this) = _val;
            return *this;
        }
    };

    namespace gc
    {
        uint16_t gc_work_round_count();

        void gc_start();
    }
}