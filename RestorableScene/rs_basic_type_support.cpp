#include "rs_basic_type.hpp"

namespace rs
{
    bool value_compare::operator()(const value& lhs, const value& rhs) const
    {
        if (lhs.type == rhs.type)
        {
            switch (lhs.type)
            {
            case value::valuetype::integer_type:
                return lhs.integer < rhs.integer;
            case value::valuetype::real_type:
                return lhs.real < rhs.real;
            case value::valuetype::handle_type:
                return lhs.handle < rhs.handle;
            case value::valuetype::string_type:
                return (*lhs.string) < (*rhs.string);
            default:
                rs_fail(RS_ERR_TYPE_FAIL, "Values of this type cannot be compared.");
                return false;
            }
        }
        return lhs.type < rhs.type;
    }
}