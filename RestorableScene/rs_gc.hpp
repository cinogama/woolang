#pragma once

namespace rs
{
    struct gcbase
    {
        enum gctype
        {
            no_gc,
            eden,
            young,
            old,
        };
        gctype gc_type = gctype::no_gc;

        virtual ~gcbase() = default;
    };

    template<typename T>
    struct gcunit : public gcbase, public T
    {
        template<typename ... ArgTs>
        gcunit(ArgTs && ... args) : T(args...)
        {

        }

        template<typename ... ArgTs>
        static gcunit<T>* gc_new(ArgTs && ... args)
        {
            return new gcunit<T>(args...);
        }

        template<typename TT>
        inline gcunit& operator = (TT&& _val)
        {
            (*(T*)this) = _val;
            return *this;
        }
    };

}