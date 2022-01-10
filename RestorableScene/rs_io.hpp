#pragma once

#include "rs_global_setting.hpp"
#include "rs_meta.hpp"


#include <iostream>

namespace rs
{
    class rs_endline {};
    inline std::ostream& operator<<(std::ostream& os, const rs_endline&)
    {
        return os << std::endl;
    }
    inline std::wostream& operator<<(std::wostream& os, const rs_endline&)
    {
        return os << std::endl;
    }

    inline static rs_endline rs_endl;

    template<typename OST = std::ostream>
    class rs_ostream
    {
        OST& os;
    public:
        rs_ostream(OST& ostrm)
            :os(ostrm)
        {

        }

        template<typename T>
        rs_ostream& operator <<(const T& oitem)
        {
            if (config::ENABLE_OUTPUT_ANSI_COLOR_CTRL)
            {
                os << oitem;
            }
            else
            {
                if constexpr (rs::meta::is_string<T>::value || rs::meta::is_wstring<T>::value)
                {
                    bool skipping_ansi_code = false;
                    if constexpr (std::is_same<T, std::string>::value || std::is_same<T, std::wstring>::value)
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

    inline static rs_ostream rs_stdout(std::cout);
    inline static rs_ostream rs_stderr(std::cerr);
    inline static rs_ostream<std::wostream> rs_wstdout(std::wcout);
    inline static rs_ostream<std::wostream> rs_wstderr(std::wcerr);

}