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

#include <type_traits>
#include <string>
#include <atomic>

// Windows
#include <stdint.h>

#ifdef _WIN32
#include <Ws2tcpip.h>
#include <WinSock2.h>
#pragma comment(lib, "WS2_32")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#endif


#include "wepoll.h"

namespace network
{
    template <typename T>
    struct sfinae_type
    {
        using type = T;
    };

    struct sfinae_fail
    {
    };


#define __concept_static_member_struct_name(OT,StaticM,Line) sfinae_static_member##_##OT##_##StaticM##_##Line
#define _concept_static_member_struct_name(OT,Line) __concept_static_member_struct_name(OT,StaticM,Line)

#define __concept_member_struct_name(OT,M,Line) sfinae_member##_##OT##_##M##_##Line
#define _concept_member_struct_name(OT,Line) __concept_static_member_struct_name(OT,M,Line)

#define _concept_static_member_type(OT,StaticM,Line)\
    template<typename T>\
    struct _concept_static_member_struct_name(OT,Line)\
    {\
        template<typename U>\
        static auto sfinae_func(int)->sfinae_type<decltype(U::StaticM)>;\
        template<typename U>\
        static sfinae_type<sfinae_fail> sfinae_func(...);\
        \
        using type=typename decltype(sfinae_func<T>(0))::type;\
        \
    };


#define _concept_member_type(OT,M,Line)\
    template<typename T>\
    struct _concept_member_struct_name(OT,Line)\
    {\
        template<typename U>\
        static auto sfinae_func(int)->sfinae_type<decltype(std::declval<U>().M)>;\
        template<typename U>\
        static sfinae_type<sfinae_fail> sfinae_func(...);\
        \
        using type=typename decltype(sfinae_func<T>(0))::type;\
        \
    };

#define concept_static_member(OT,StaticM) \
_concept_static_member_type(OT,StaticM,__LINE__)\
static_assert(!std::is_same<sfinae_fail,_concept_static_member_struct_name(OT,__LINE__)<OT>::type>::value, "'" #OT "' don't have static member: '" #StaticM "'.");

#define concept_member(OT,M) \
_concept_member_type(OT,M,__LINE__)\
static_assert(!std::is_same<sfinae_fail,_concept_member_struct_name(OT,__LINE__)<OT>::type>::value, "'" #OT "' don't have member: '" #M "'.");

#define concept_bool static_assert

#ifdef _WIN32
    using sock_t = SOCKET;
#else
    using sock_t = int;
#endif

    using family_t = decltype(std::declval<sockaddr>().sa_family);
    using state_t = int;


    class ip_any : public sockaddr_storage
    {
        concept_bool(sizeof(sockaddr_storage) > sizeof(sockaddr_in));
        concept_bool(sizeof(sockaddr_storage) > sizeof(sockaddr_in6));

    public:

        enum ip_proto :family_t
        {
            ipv4 = AF_INET,
            ipv6 = AF_INET6
        };

        int real_length() const
        {
            if (family() == ip_proto::ipv4)
                return sizeof(sockaddr_in);
            return sizeof(sockaddr_in6);
        }

        ip_any(ip_proto type = ip_proto::ipv6)
        {
            memset(this, 0, sizeof(ip_any));
            ((sockaddr*)this)->sa_family = type;
        }

        ip_proto family() const
        {
            return (ip_proto)((sockaddr*)this)->sa_family;
        }
        ip_proto family(ip_proto proto)
        {
            memset(this, 0, sizeof(ip_any));
            return (ip_proto)(((sockaddr*)this)->sa_family = proto);
        }

        int parse(const std::string& ip, short port)
        {
            const char* p = ip.c_str();
            int cnt = 0;
            for (; *p != '\0'; p++)
                if (*p == ':')
                    cnt++;
            if (cnt >= 2)
            {
                ((sockaddr_in6*)this)->sin6_family = AF_INET6;
                ((sockaddr_in6*)this)->sin6_port = htons(port);
                if (inet_pton(AF_INET6, ip.c_str(), &((sockaddr_in6*)this)->sin6_addr) <= 0)
                    return -1;
            }
            else
            {
                ((sockaddr_in*)this)->sin_family = AF_INET;
                ((sockaddr_in*)this)->sin_port = htons(port);
                if (inet_pton(AF_INET, ip.c_str(), &((sockaddr_in*)this)->sin_addr) <= 0)
                    return -1;
            }
            return 0;
        }

        int parse(short port)
        {
            ip_proto fm = family();

            if (fm == ip_proto::ipv4)
            {
#ifdef _WIN32
#else
                const static in_addr in4addr_any = { INADDR_ANY };
#endif
                ((sockaddr_in*)this)->sin_port = htons(port);
                ((sockaddr_in*)this)->sin_addr = in4addr_any;
            }
            else if (fm == ip_proto::ipv6)
            {
                ((sockaddr_in6*)this)->sin6_port = htons(port);
                ((sockaddr_in6*)this)->sin6_addr = in6addr_any;
            }
            else
                return -1;
            return 0;
        }

        std::string to_string() const
        {
            char buffer[INET6_ADDRSTRLEN] = {};
            if (family() == ip_proto::ipv6)
            {
                inet_ntop(AF_INET6, &((sockaddr_in6*)this)->sin6_addr, buffer, INET6_ADDRSTRLEN);
                return buffer;
            }
            else
            {
                inet_ntop(AF_INET, &((sockaddr_in*)this)->sin_addr, buffer, INET6_ADDRSTRLEN);
                return buffer;
            }
        }
    };

    class udp
    {
        concept_member(ip_any, family());
        concept_member(ip_any, parse("", 0));
        concept_member(ip_any, parse(0));

    public:
        static sock_t sock_create(ip_any::ip_proto proto_family = ip_any::ip_proto::ipv6)
        {
            return socket(proto_family, SOCK_DGRAM, IPPROTO_UDP);
        }
        static sock_t sock_create_noblock(ip_any::ip_proto proto_family = ip_any::ip_proto::ipv6)
        {
            auto sock = socket(proto_family, SOCK_DGRAM, IPPROTO_UDP);

            unsigned long setting = 1;
#ifdef _WIN32
            ioctlsocket(sock, FIONBIO, &setting);
#else
            ioctl(sock, FIONBIO, &setting);
#endif

            return sock;
        }

        static state_t sock_bind(sock_t sock, const ip_any* pt)
        {
            return bind(sock, (const sockaddr*)pt, sizeof(ip_any));
        }
        static int sock_recvfrom(sock_t sock, void* buffer, size_t max_recv_sz, ip_any* fromwho)
        {
            socklen_t sz = sizeof(ip_any);
            return recvfrom(sock, (char*)buffer, (int)max_recv_sz, 0, (sockaddr*)fromwho, &sz);
        }
        static int sock_sendto(sock_t sock, const void* buffer, size_t max_recv_sz, ip_any* towhom)
        {
            return sendto(sock, (char*)buffer, (int)max_recv_sz, 0, (sockaddr*)towhom, sizeof(ip_any));
        }

        static int sock_close(sock_t sock)
        {
#ifdef _WIN32
            return closesocket(sock);
#else
            return close(sock);
#endif
        }

    };
    class tcp
    {
    public:

        static sock_t sock_create(ip_any::ip_proto proto_family = ip_any::ip_proto::ipv6)
        {
            return socket(proto_family, SOCK_STREAM, IPPROTO_TCP);
        }
        static sock_t sock_create_noblock(ip_any::ip_proto proto_family = ip_any::ip_proto::ipv6)
        {
            auto sock = socket(proto_family, SOCK_STREAM, IPPROTO_TCP);

            unsigned long setting = 1;
#ifdef _WIN32
            ioctlsocket(sock, FIONBIO, &setting);
#else
            ioctl(sock, FIONBIO, &setting);
#endif

            return sock;
        }

        static int sock_listen(sock_t sock, int backlog)
        {
            return listen(sock, backlog);
        }
        static sock_t sock_accept(sock_t sock, ip_any& client_addr)
        {
            socklen_t sz = sizeof(ip_any);
            return accept(sock, (sockaddr*)&client_addr, &sz);
        }
        static int sock_connect(sock_t sock, const ip_any& server_addr)
        {
            return connect(sock, (const sockaddr*)&server_addr, sizeof(ip_any));
        }

        static int sock_recv(sock_t sock, void* buffer, size_t max_recv_sz)
        {
            return recv(sock, (char*)buffer, (int)max_recv_sz, 0);
        }

        static int sock_send(sock_t sock, const void* buffer, size_t max_recv_sz)
        {
            return send(sock, (char*)buffer, (int)max_recv_sz, 0);
        }
    };

    namespace default_policy
    {
        struct default_execute_policy
        {
            template<typename ContainerT, typename Ft>
            static void execute(size_t from, size_t to, ContainerT& container, Ft func)
            {
                for (size_t index = from; index < to; index++)
                {
                    func(container[index]);
                }
            }
        };
    }
}


static std::atomic_size_t _entry_count = 0;

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

static epoll_holder _holder;

RS_API void rslib_entry()
{
    if (_entry_count++ == 0)
    {
        // First entry, create a thread to do epoll
    }
}

RS_API void rslib_exit()
{

}