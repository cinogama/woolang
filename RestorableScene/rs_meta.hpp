#pragma once
#include <type_traits>
#include <string>


namespace rs
{
    namespace meta
    {
        template<typename T>
        using origin_type = typename std::remove_cv<typename std::decay<T>::type>::type;

        using true_type = std::true_type;
        using false_type = std::false_type;

        template<typename T>
        struct does_have_method_gc_travel
        {
            template<typename U>
            static auto checkout(int) -> decltype((&U::gc_travel), std::declval<true_type>());

            template<typename U>
            static false_type checkout(...);

            static constexpr bool value = origin_type<decltype(checkout<T>(0))>::value;
        };

        template<typename T>
        struct is_string
        {
            static constexpr bool value =
                std::is_convertible<origin_type<T>, std::string>::value;

        };

        template<typename T>
        struct is_wstring
        {
            static constexpr bool value =
                std::is_convertible<origin_type<T>, std::wstring>::value;

        };

        template<typename T>
        size_t type_hash = typeid(T).hash_code();
    } // namespace meta;
}