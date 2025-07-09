#pragma once

#include "wo_assert.hpp"

#include <memory>
#include <string>
#include <unordered_set>

using wo_pstring_t = const std::wstring*;

#define WO_GLOBAL_PSTR(str) inline wo_pstring_t _global_##str;

#define WO_GLOBAL_PSTR_LIST \
WO_GLOBAL_PSTR(woolang)\
WO_GLOBAL_PSTR(int)\
WO_GLOBAL_PSTR(handle)\
WO_GLOBAL_PSTR(real)\
WO_GLOBAL_PSTR(string)\
WO_GLOBAL_PSTR(dict)\
WO_GLOBAL_PSTR(array)\
WO_GLOBAL_PSTR(map)\
WO_GLOBAL_PSTR(vec)\
WO_GLOBAL_PSTR(gchandle)\
WO_GLOBAL_PSTR(nil)\
WO_GLOBAL_PSTR(union)\
WO_GLOBAL_PSTR(struct)\
WO_GLOBAL_PSTR(tuple)\
WO_GLOBAL_PSTR(void)\
WO_GLOBAL_PSTR(nothing)\
WO_GLOBAL_PSTR(pending)\
WO_GLOBAL_PSTR(dynamic)\
WO_GLOBAL_PSTR(function)\
WO_GLOBAL_PSTR(bool)\
WO_GLOBAL_PSTR(char)\
WO_GLOBAL_PSTR(auto)\
WO_GLOBAL_PSTR(std)\
WO_GLOBAL_PSTR(iterator)\
WO_GLOBAL_PSTR(next)\
WO_GLOBAL_PSTR(KT)\
WO_GLOBAL_PSTR(VT)\
WO_GLOBAL_PSTR(bind)\
WO_GLOBAL_PSTR(value)\
WO_GLOBAL_PSTR(none)\
WO_GLOBAL_PSTR(unsafe)\
WO_GLOBAL_PSTR(_)\
// end

#define WO_PSTR(str) wo::fixstr::_global_##str

namespace wo::fixstr
{
    WO_GLOBAL_PSTR_LIST;
    inline wo_pstring_t _global_EMPTY;
    inline wo_pstring_t _global__iter;
    inline wo_pstring_t _global__val;

    inline wo_pstring_t _global_operator_ADD;
    inline wo_pstring_t _global_operator_SUB;
    inline wo_pstring_t _global_operator_MUL;
    inline wo_pstring_t _global_operator_DIV;
    inline wo_pstring_t _global_operator_MOD;
    inline wo_pstring_t _global_operator_LAND;
    inline wo_pstring_t _global_operator_LOR;
    inline wo_pstring_t _global_operator_LESS;
    inline wo_pstring_t _global_operator_LESSEQ;
    inline wo_pstring_t _global_operator_GREAT;
    inline wo_pstring_t _global_operator_GREATEQ;
    inline wo_pstring_t _global_operator_EQ;
    inline wo_pstring_t _global_operator_NEQ;
    inline wo_pstring_t _global_label_woolang_program_end;
}

#undef WO_GLOBAL_PSTR

namespace wo
{
    struct wstring_pool
    {
        inline static std::unique_ptr<wstring_pool> _m_global_string_pool;

        struct pstr_hasher
        {
            size_t operator ()(const wo_pstring_t& str) const noexcept
            {
                static std::hash<std::wstring> hasher;
                return hasher(*str);
            }
        };
        struct pstr_equal
        {
            size_t operator ()(const wo_pstring_t& left, const wo_pstring_t& right) const noexcept
            {
                return *left == *right;
            }
        };

        std::unordered_set<wo_pstring_t, pstr_hasher, pstr_equal> _m_string_pool;

        static void init_global_str_pool()
        {
            wo_assert(_m_global_string_pool == nullptr);

            _m_global_string_pool = std::make_unique<wstring_pool>();

#define WO_GLOBAL_PSTR(str)\
    WO_PSTR(str) = new std::wstring(L ## #str);\
    _m_global_string_pool->_m_string_pool.insert(WO_PSTR(str));
#define WO_GLOBAL_PSTR_WITH_CONST(str, wlstr)\
    WO_PSTR(str) = new std::wstring(wlstr);\
    _m_global_string_pool->_m_string_pool.insert(WO_PSTR(str));

            WO_GLOBAL_PSTR_LIST;
            WO_GLOBAL_PSTR_WITH_CONST(EMPTY, L"");
            WO_GLOBAL_PSTR_WITH_CONST(_iter, L"$_iter");
            WO_GLOBAL_PSTR_WITH_CONST(_val, L"$_val");
            WO_GLOBAL_PSTR_WITH_CONST(operator_ADD, L"operator +");
            WO_GLOBAL_PSTR_WITH_CONST(operator_SUB, L"operator -");
            WO_GLOBAL_PSTR_WITH_CONST(operator_MUL, L"operator *");
            WO_GLOBAL_PSTR_WITH_CONST(operator_DIV, L"operator /");
            WO_GLOBAL_PSTR_WITH_CONST(operator_MOD, L"operator %");
            WO_GLOBAL_PSTR_WITH_CONST(operator_LAND, L"operator &&");
            WO_GLOBAL_PSTR_WITH_CONST(operator_LOR, L"operator ||");
            WO_GLOBAL_PSTR_WITH_CONST(operator_LESS, L"operator <");
            WO_GLOBAL_PSTR_WITH_CONST(operator_LESSEQ, L"operator <=");
            WO_GLOBAL_PSTR_WITH_CONST(operator_GREAT, L"operator >");
            WO_GLOBAL_PSTR_WITH_CONST(operator_GREATEQ, L"operator >=");
            WO_GLOBAL_PSTR_WITH_CONST(operator_EQ, L"operator ==");
            WO_GLOBAL_PSTR_WITH_CONST(operator_NEQ, L"operator !=");
            WO_GLOBAL_PSTR_WITH_CONST(label_woolang_program_end, L"#woolang_program_end");
#undef WO_GLOBAL_PSTR

        }
        static void shutdown_global_str_pool()
        {
            wo_assert(_m_global_string_pool != nullptr);
            _m_global_string_pool.reset();

            // NOTE: No need to free/delete, it has been done in `pool.reset`.
#define WO_GLOBAL_PSTR(str) WO_PSTR(str) = nullptr;
            WO_GLOBAL_PSTR_LIST;
            WO_GLOBAL_PSTR(EMPTY);
            WO_GLOBAL_PSTR(_iter);
            WO_GLOBAL_PSTR(_val);
            WO_GLOBAL_PSTR(operator_ADD);
            WO_GLOBAL_PSTR(operator_SUB);
            WO_GLOBAL_PSTR(operator_MUL);
            WO_GLOBAL_PSTR(operator_DIV);
            WO_GLOBAL_PSTR(operator_MOD);
            WO_GLOBAL_PSTR(operator_LAND);
            WO_GLOBAL_PSTR(operator_LOR);
            WO_GLOBAL_PSTR(operator_LESS);
            WO_GLOBAL_PSTR(operator_LESSEQ);
            WO_GLOBAL_PSTR(operator_GREAT);
            WO_GLOBAL_PSTR(operator_GREATEQ);
            WO_GLOBAL_PSTR(operator_EQ);
            WO_GLOBAL_PSTR(operator_NEQ);
#undef WO_GLOBAL_PSTR
        }

#undef WO_GLOBAL_PSTR_LIST

        wstring_pool* get_global_string_pool()
        {
            wo_assert(_m_global_string_pool);
            return _m_global_string_pool.get();
        }

        // All function is lock free, because global_pool is readonly and rwpool only
        // used in 'one' thread.
        wo_pstring_t find(const std::wstring& str) const
        {
            auto fnd = _m_string_pool.find(&str);
            if (fnd == _m_string_pool.end())
                return nullptr;
            return *fnd;
        }

        wo_pstring_t find_or_add(const std::wstring& str)
        {
            wo_pstring_t fnd;
            if ((fnd = get_global_string_pool()->find(str)))
                return fnd;
            if ((fnd = find(str)))
                return fnd;

            wo_pstring_t new_str = new std::wstring(str);
            _m_string_pool.insert(new_str);

            return new_str;
        }
        wstring_pool() = default;
        ~wstring_pool()
        {
            for (wo_pstring_t pstr : _m_string_pool)
                delete pstr;
        }

        wstring_pool(wstring_pool&) = delete;
        wstring_pool(wstring_pool&&) = delete;
        wstring_pool& operator = (wstring_pool&) = delete;
        wstring_pool& operator = (wstring_pool&&) = delete;

        inline static thread_local wstring_pool* _m_this_thread_pool = nullptr;
        inline static thread_local size_t _m_this_thread_pool_count = 0;

        static wstring_pool* begin_new_pool()
        {
            if (nullptr == _m_this_thread_pool)
            {
                wo_assert(_m_this_thread_pool_count == 0);
                _m_this_thread_pool = new wstring_pool();
            }
            else
                wo_assert(_m_this_thread_pool_count);

            ++_m_this_thread_pool_count;
            return _m_this_thread_pool;
        }

        static void end_pool()
        {
            wo_assert(_m_this_thread_pool_count != 0);
            if (0 == --_m_this_thread_pool_count)
            {
                delete _m_this_thread_pool;
                _m_this_thread_pool = nullptr;
            }
        }

        static wo_pstring_t get_pstr(const std::wstring& str)
        {
            wo_assert(_m_this_thread_pool);
            return _m_this_thread_pool->find_or_add(str);
        }
    };

    struct start_string_pool_guard
    {
        start_string_pool_guard(start_string_pool_guard&) = delete;
        start_string_pool_guard(start_string_pool_guard&&) = delete;
        start_string_pool_guard& operator = (start_string_pool_guard&) = delete;
        start_string_pool_guard& operator = (start_string_pool_guard&&) = delete;

        start_string_pool_guard()
        {
            wstring_pool::begin_new_pool();
        }
        ~start_string_pool_guard()
        {
            wstring_pool::end_pool();
        }
    };
}