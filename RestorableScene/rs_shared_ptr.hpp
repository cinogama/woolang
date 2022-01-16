#pragma once
#include <atomic>
namespace rs
{
    template<typename T, typename COUNTT = std::atomic_size_t>
    class shared_pointer
    {
        T* ptr = nullptr;
        COUNTT* ref_count = nullptr;
        void(*release_func)(T*) = nullptr;

        static_assert(sizeof(COUNTT) == sizeof(size_t));

        static void DEFAULT_DESTROY_FUNCTION(T* ptr) { delete ptr; }

    public:
        void clear()
        {
            if (ptr)
            {
                if (!-- * ref_count)
                {
                    // Recycle
                    release_func(ptr);

                    ref_count->~COUNTT();
                    delete ref_count;
                }
            }
        }
        ~shared_pointer()
        {
            clear();
        }

        shared_pointer() = default;
        shared_pointer(T* v, void(*f)(T*) = nullptr) :
            ptr(v),
            release_func(f ? f : DEFAULT_DESTROY_FUNCTION),
            ref_count(new COUNTT(1))
        {

        }

        shared_pointer(const shared_pointer& v)
        {
            ptr = v.ptr;
            release_func = v.release_func;
            if (ref_count = v.ref_count)
                ++* ref_count;
        }

        shared_pointer(shared_pointer&& v)
        {
            ptr = v.ptr;
            release_func = v.release_func;
            ref_count = v.ref_count;
            v.ptr = nullptr;
        }

        shared_pointer& operator =(const shared_pointer& v)
        {
            clear();

            ptr = v.ptr;
            release_func = v.release_func;
            if (ref_count = v.ref_count)
                ++* ref_count;

            return *this;
        }

        shared_pointer& operator =(shared_pointer&& v)noexcept
        {
            clear();

            ptr = v.ptr;
            release_func = v.release_func;
            ref_count = v.ref_count;
            v.ptr = nullptr;

            return *this;
        }

        T* get() const
        {
            return ptr;
        }
        operator T& ()const
        {
            return *ptr;
        }
        T& operator * ()const
        {
            return *ptr;
        }
        operator T* ()const
        {
            return ptr;
        }
        operator bool()const
        {
            return ptr;
        }
        T* operator -> ()const
        {
            return ptr;
        }

        bool operator ==(const T* pointer)const
        {
            return ptr == pointer;
        }
        bool operator !=(const T* pointer)const
        {
            return ptr != pointer;
        }
        bool operator ==(const shared_pointer& pointer)const
        {
            return ptr == pointer.ptr;
        }
        bool operator !=(const shared_pointer& pointer)const
        {
            return ptr != pointer.ptr;
        }

        size_t used_count() const
        {
            return *ref_count;
        }

    };
}
