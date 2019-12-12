#pragma once
/***
*** @file eddid_common.h
*** @brief 包括项目所需要的C库及C++头文件，并定义相关宏处理平台差异的函数
*** @author xiay
*** @version v 0.1.1
*** @date 2019-11-22
****/
#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <cstddef>
#include <cstdarg>
#include <cassert>
#include <cmath>
#include <ctime>
#include <cstring>

#if defined(__linux__)
// 标准系统头文件
#include <unistd.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <syscall.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <dirent.h>
#include <glob.h>
#include <fcntl.h>
#include <sched.h>
#include <endian.h>
#include <poll.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <signal.h>
#include <time.h>
#include <malloc.h>
#include <limits.h>
#include <pthread.h>
#include <inttypes.h>
#include <semaphore.h>
#include <dlfcn.h>

typedef int socket_fd;
typedef int handle_fd;
typedef pthread_t thread_handle;
typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr_in6 SOCKADDR_IN6;
typedef sem_t semaphore_t;

#define INFINITE 0xFFFFFFFF
#define strerror_p strerror_r
#define snprintf_s(s, n, fmt,...) snprintf(s, n, fmt, ##__VA_ARGS__)
#define strncpy_s(s, d, n)  strncpy(s, d, n)
#define DIRECTORY_ROOT "/"
#define MAX std::max
#elif defined(_WIN32) || defined(_WIN64)
#include <ws2tcpip.h>
#include <mswsock.h>
#include <nb30.h>
#include <iphlpapi.h>
#include <mmsystem.h>
#include <malloc.h>
#include <conio.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Winmm.lib")
typedef HANDLE semaphore_t;
typedef SOCKET socket_fd;
typedef HANDLE handle_fd;
typedef HANDLE thread_handle;
typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr_in6 SOCKADDR_IN6;
typedef HANDLE pid_t;
#define snprintf_s(s, n, fmt, ...) _snprintf_s(s, n, n - 1, fmt, ##__VA_ARGS__)
#define strerror_p strerror_s
#define DIRECTORY_ROOT "\\"
#define MAX max
#endif

// 标准C++头文件
#include <bitset>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <memory>
#include <string>
#include <set>
#include <map>
#include <cstdint>
#include <list>
#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <iterator>
#include <regex>
#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <type_traits>
#include <tuple>
#include <any>             // stdc++17
#include <string_view>     // stdc++17
#include <variant>         // stdc++17
#include <filesystem>      // stdc++17


typedef std::shared_ptr<char> data_ptr;
typedef std::shared_ptr<char[]> data_arr_ptr;

#define DEFAULT_RECV_BUFFER_SIZE 1024

#ifndef EDDID_DISALLOW_COPY
#define EDDID_DISALLOW_COPY(class_name)                         \
public:                                                         \
    class_name(const class_name&) = delete;                     \
    class_name& operator=(const class_name&) = delete;
#endif
    
#ifndef EDDID_RVALUE_DISALLOW_MOVE
#define EDDID_RVALUE_DISALLOW_MOVE(class_name)                  \
public:                                                         \
    class_name(const class_name&&) = delete;                    \
    class_name& operator=(const class_name&&) = delete;
#endif

#ifndef EDDID_DISALLOW_MOVE_COPY
#define EDDID_DISALLOW_MOVE_COPY(thisClass)    \
    EDDID_DISALLOW_COPY(thisClass)             \
    EDDID_RVALUE_DISALLOW_MOVE(thisClass)
#endif


#if defined(__linux__)
    using arr_events = std::vector<epoll_event>;
#elif defined(_WIN32) || defined(_WIN64)
    using arr_events = std::vector<OVERLAPPED_ENTRY>;
#endif

#ifndef rein_cast
#define rein_cast(t, d) reinterpret_cast<t>(d)
#endif // !rein_cast

struct DeleteCStyle
{
    void operator()(char* ptr) const
    {
        ::free(ptr);
    }
};
    
    
