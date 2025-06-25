#pragma once

#include <atomic>

namespace wo
{
    template<typename T, typename CountT = std::atomic_size_t>
    class shared_pointer
    {
        static_assert(sizeof(CountT) == sizeof(size_t));

        using release_func_t = void(*)(T*);
        static void __default_release_func(T* ptr)
        {
            delete ptr;
        }

        T* ptr = nullptr;
        CountT* ref_count = nullptr;
        release_func_t release_func = nullptr;

    public:
        void clear()
        {
            if (ptr)
            {
                if (!-- * ref_count)
                {
                    // Recycle
                    release_func(ptr);

                    ref_count->~CountT();
                    delete ref_count;
                }
            }
            else
                wo_assert(ref_count == nullptr);
        }
        ~shared_pointer()
        {
            clear();
        }

        shared_pointer() noexcept = default;
        shared_pointer(T* v, release_func_t f = nullptr) noexcept :
            ptr(v),
            ref_count(nullptr),
            release_func(f ? f : __default_release_func)
        {
            if (ptr != nullptr)
                ref_count = new CountT(1);
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
            clear();

            ptr = v.ptr;
            release_func = v.release_func;
            if ((ref_count = v.ref_count))
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
