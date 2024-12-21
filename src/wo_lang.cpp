#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    lang_Symbol::~lang_Symbol()
    {
        if (m_is_template)
        {
            if (m_symbol_kind == VARIABLE)
                delete m_template_value_instances;
            else
                delete m_template_type_instances;
        }
        else
        {
            if (m_symbol_kind == VARIABLE)
                delete m_value_instance;
            else
                delete m_type_instance;
        }
    }
#endif
}
