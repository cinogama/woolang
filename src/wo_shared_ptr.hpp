#pragma once

#include <atomic>

namespace wo
{
    template<typename T>
    class shared_pointer
    {
        using release_func_t = void(*)(T*);
        static void __default_release_func(T* ptr)
        {
            delete ptr;
        }

        T* ptr = nullptr;
        std::atomic_size_t* ref_count = nullptr;
        release_func_t release_func = nullptr;

        void _release()
        {
            if (ptr)
            {
                if (!-- *ref_count)
                {
                    // Recycle
                    release_func(ptr);
                    delete ref_count;
                }
            }
            else
                wo_assert(ref_count == nullptr);
        }

    public:
        void reset()
        {
            _release();
            ptr = nullptr;
            ref_count = nullptr;
        }
        ~shared_pointer()
        {
            _release();
        }

        shared_pointer() noexcept = default;
        explicit shared_pointer(T* v, release_func_t f = &__default_release_func) noexcept :
            ptr(v),
            ref_count(nullptr),
            release_func(f)
        {
            if (ptr != nullptr)
                ref_count = new std::atomic_size_t(1);
        }

        shared_pointer(const shared_pointer& v) noexcept
        {
            ptr = v.ptr;
            release_func = v.release_func;
            if ((ref_count = v.ref_count))
                ++* ref_count;
        }

        shared_pointer(shared_pointer&& v) noexcept
        {
            ptr = v.ptr;
            release_func = v.release_func;
            ref_count = v.ref_count;
            v.ptr = nullptr;
            v.ref_count = nullptr;
        }

        shared_pointer& operator =(const shared_pointer& v) noexcept
        {
            _release();

            ptr = v.ptr;
            release_func = v.release_func;
            if ((ref_count = v.ref_count))
                ++* ref_count;

            return *this;
        }

        shared_pointer& operator =(shared_pointer&& v)noexcept
        {
            _release();

            ptr = v.ptr;
            release_func = v.release_func;
            ref_count = v.ref_count;
            v.ptr = nullptr;
            v.ref_count = nullptr;
            return *this;
        }

        T* get() const noexcept
        {
            return ptr;
        }
        operator T& ()const noexcept
        {
            return *ptr;
        }
        T& operator * ()const noexcept
        {
            return *ptr;
        }
        operator T* ()const noexcept
        {
            return ptr;
        }
        operator bool()const noexcept
        {
            return ptr;
        }
        T* operator -> ()const noexcept
        {
            return ptr;
        }

        bool operator ==(const T* pointer)const noexcept
        {
            return ptr == pointer;
        }
        bool operator !=(const T* pointer)const noexcept
        {
            return ptr != pointer;
        }
        bool operator ==(const shared_pointer& pointer)const noexcept
        {
            return ptr == pointer.ptr;
        }
        bool operator !=(const shared_pointer& pointer)const noexcept
        {
            return ptr != pointer.ptr;
        }

        size_t used_count() const noexcept
        {
            return *ref_count;
        }

    };
}
