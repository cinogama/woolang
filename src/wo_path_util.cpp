#include "wo_afx.hpp"

#include "wo_path_util.hpp"

namespace wo
{
    using namespace std;
    string get_file_loc(string path)
    {
        normalize_path(&path);

        size_t fnd = path.rfind('/');
        if (fnd < path.size())
            return path.substr(0, fnd);

        return "";
    }

    void normalize_path(string* inout_path)
    {
#ifdef _WIN32
        for (char& ch : *inout_path)
        {
            if (ch == '\\')
                ch = '/';
        }
        if (inout_path->length() >= 2 && inout_path->at(1) == L':')
        {
            char& p = inout_path->at(0);
            if (p >= 'a' && p <= 'z')
                p = toupper(p);
        }
#endif
    }
}
