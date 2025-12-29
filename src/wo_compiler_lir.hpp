#pragma once

#include <optional>

namespace wo::lir
{
    struct Label
    {
        std::optional<size_t> m_binded_offset;
        // std::optional<
    };
    struct Opnum
    {
        enum class Type
        {
            CONSTANT,
            GLOBAL,
            VIRTUALREG,
        };

        Type m_type;
        uint32_t m_addressing;
    };
    static_assert(sizeof(Opnum) == 8);
    static_assert(std::is_trivial_v<Opnum>);

    struct LIRCompiler
    {
        // std::vector<>;
    };
}