#include "../../RestorableScene/rs.h"

#ifdef _WIN32
#   ifdef _WIN64
#       ifdef NDEBUG
#           pragma comment(lib, "../../x64/Release/librscene.lib")
#       else
#           pragma comment(lib,"../../x64/Debug/librscene_debug.lib")
#       endif
#   else
#       ifdef NDEBUG
#           pragma comment(lib,"../../Release/librscene32.lib")
#       else
#           pragma comment(lib,"../../Debug/librscene32_debug.lib")
#       endif
#   endif
#endif

#include "wepoll.h"

static size_t _entry_count = 0;

struct epoll_holder
{
    bool _stop_thread_flag = false;
    static void _epoll_listen_and_awake_thread_work(epoll_holder* _this)
    {
        while (!_this->_stop_thread_flag)
        {

        }
    }
};



RS_API void entry()
{
    if (_entry_count++ == 0)
    {
        // First entry, create a thread to do epoll

    }
}

RS_API void exit()
{

}