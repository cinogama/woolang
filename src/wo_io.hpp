#pragma once

#include "wo_global_setting.hpp"
#include "wo_meta.hpp"

#include <iostream>

namespace wo
{
    class wo_endline {};
    inline std::ostream& operator<<(std::ostream& os, const wo_endline&)
    {
        return os << std::endl;
    }
    inline std::wostream& operator<<(std::wostream& os, const wo_endline&)
    {
        return os << std::endl;
    }

    inline static wo_endline wo_endl;

    template<typename OST = std::ostream>
    class wo_ostream
    {
        OST& os;
    public:
        wo_ostream(OST& ostrm)
            :os(ostrm)
        {

        }

        template<typename T>
        wo_ostream& operator <<(const T& oitem)
        {
            if (config::ENABLE_OUTPUT_ANSI_COLOR_CTRL)
                os << oitem;
            else
            {
                if constexpr (wo::meta::is_string<T>::value)
                {
                    bool skipping_ansi_code = false;
                    if constexpr (std::is_same<T, std::string>::value)
                    {
                        for (auto chbeg = oitem.begin(); chbeg != oitem.end(); chbeg++)
                        {
                            if (!skipping_ansi_code)
                            {
                                if (*chbeg == '\033')
                                    skipping_ansi_code = true;
                                else
                                    os << *chbeg;
                            }
                            else if (*chbeg == 'm')
                                skipping_ansi_code = false;
                        }
                    }
                    else
                    {
                        for (auto chbeg = oitem; *chbeg; chbeg++)
                        {
                            if (!skipping_ansi_code)
                            {
                                if (*chbeg == '\033')
                                    skipping_ansi_code = true;
                                else
                                    os << *chbeg;
                            }
                            else if (*chbeg == 'm')
                                skipping_ansi_code = false;
                        }
                    }

                }
                else
                    os << oitem;
            }
            return *this;
        }
    };

    inline static wo_ostream wo_stdout(std::cout);
    inline static wo_ostream wo_stderr(std::cerr);
}