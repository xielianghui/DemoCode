#pragma once
#include "ev_loop.h"
#include <any> 
#include <memory>
#include <string>
#include <algorithm>
#include "eddid_endian.h"
#include "concurrentqueue.h"

#define DEFAULT_RECV_BUFFER_SIZE     1024
#define DEFAULT_ASYNC_SEND_EVENT     50
#define DEFAULT_WRITE_REQ_SIZE       100
#define DEFAULT_RECV_BUFFER_LST_SIZE 1024
#define DEFAULT_RECV_QUE_SIZE        10

namespace eddid {
namespace event_wrap {
    template <class THandle>
    class Service;

    template <class THandle, class MGR>
    class CliContext
    {
    public:
        using _My = CliContext<THandle, MGR>;
    public:
        CliContext(const CliContext&) = delete;
        CliContext& operator=(const CliContext&) = delete;
    public:
        CliContext(THandle& handle, MGR& mgr)
            : loop_(nullptr)
            , cb_(handle)
            , mgr_(mgr)
            , is_initialize_(false)
            , is_exit_(false)
            , is_connected_(false)
            , cli_bev_(nullptr)
            , recv_buffer_(nullptr)
            , is_pending_(false)
        {
        }

        ~CliContext()
        {
            Release();
        }

    public:
        void Send(char* data, uint32_t size)
        {
            if (!is_connected_) {
                return;
            }
            bufferevent_enable(cli_bev_, EV_WRITE);
            bufferevent_write(cli_bev_, data, size);
        }
    protected:
        int Initialize()
        {
            if (is_initialize_) {
                return 0;
            }

            recv_buffer_ = evbuffer_new();
            if (recv_buffer_ == nullptr) {
                return -1;
            }

            is_initialize_ = true;
            return 0;
        }

        void Release()
        {
            if (!is_initialize_) {
                return;
            }
            Close();

            if (cli_bev_ != nullptr) {
                bufferevent_free(cli_bev_);
                cli_bev_ = nullptr;
            }

            if (recv_buffer_ != nullptr) {
                evbuffer_free(recv_buffer_);
                recv_buffer_ = nullptr;
            }
        }

        int AcceptClient(EvLoop* loop, evutil_socket_t fd, const sockaddr* addr, uint32_t addr_len)
        {
            is_exit_ = false;
            loop_ = loop;
            event_base* base = loop_->GetWorkLoop();
            if (base == nullptr) {
                evutil_closesocket(fd);
                return -1;
            }

            evutil_make_socket_nonblocking(fd);
            if (cli_bev_ == nullptr) {
                cli_bev_ = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
                if (cli_bev_ == nullptr) {
                    evutil_closesocket(fd);
                    return -1;
                }
                bufferevent_setcb(cli_bev_, OnRecv, OnSend, OnError, this);
            } else {
                int ret(0);
                ret = bufferevent_base_set(base, cli_bev_);
                if (ret != 0) {
                    evutil_closesocket(fd);
                    return -1;
                }
                ret = bufferevent_setfd(cli_bev_, fd);
                if (ret != 0) {
                    evutil_closesocket(fd);
                    return -1;
                }
            }
            is_connected_ = true;
            bufferevent_enable(cli_bev_, EV_READ | EV_PERSIST);

            //读超时10秒 写超时无限
            //struct timeval delay = { 10, 0 };
            //bufferevent_set_timeouts(cli_bev_, &delay, NULL);

            char peer[256] = { 0 };
            uint32_t size = 256;
            uint16_t port(0);
            if (addr->sa_family == AF_INET) {
                const sockaddr_in* addr4 = static_cast<const sockaddr_in*>((void*)addr);
                evutil_inet_ntop(AF_INET, &addr4->sin_addr, peer, static_cast<socklen_t>(size));
                port = be16toh(addr4->sin_port);
            } else if (addr->sa_family == AF_INET6) {
                const sockaddr_in6* addr6 = static_cast<const sockaddr_in6*>((void*)addr);
                evutil_inet_ntop(AF_INET6, &addr6->sin6_addr, peer, static_cast<socklen_t>(size));
                port = be16toh(addr6->sin6_port);
            }

            // 回调接受连接
            cb_.OnAccept(this, peer, port);

            return 0;
        }

        void Close()
        {
            if (is_exit_) {
                return;
            }

            if (cli_bev_ == nullptr) {
                is_exit_ = true;
                return;
            }
            bufferevent_disable(cli_bev_, EV_READ | EV_WRITE);
            evutil_socket_t fd = bufferevent_getfd(cli_bev_);
            if (fd != -1) {
                evutil_closesocket(fd);
            }
            bufferevent_setfd(cli_bev_, -1);
            event_base* base = bufferevent_get_base(cli_bev_);
            loop_->Decrement(base);

            is_connected_ = false;

            is_exit_ = true;
        }

        void SetContext(const std::any& any)
        {
            ctx_any_ = any;
        }

        inline const std::any& GetContext()
        {
            return ctx_any_;
        }

        inline std::any* GetMutableContext()
        {
            return &ctx_any_;
        }

    protected:
        void ReadDone(bufferevent *bev)
        {
            evbuffer *input = bufferevent_get_input(bev);
            evbuffer_add_buffer(recv_buffer_, input);
            cb_.OnReadDone(this, std::forward<evbuffer*>(recv_buffer_));
        }

        void SendDone(bufferevent *bev)
        {
            evbuffer *output = bufferevent_get_output(bev);
            if (output == nullptr) {
                return;
            }
        }

        void Error(bufferevent *bev, short what)
        {
            if (what & BEV_EVENT_EOF) {
                // 客户端已经关闭
                Close();
                mgr_.RecycleClientConn(this);
                cb_.OnDisConnect(this);
                int err = EVUTIL_SOCKET_ERROR();
                cb_.OnError(this, err, evutil_socket_error_to_string(err));
            } else if (what & BEV_EVENT_ERROR) {
                // 客户端已经关闭
                Close();
                mgr_.RecycleClientConn(this);
                cb_.OnDisConnect(this);
                int err = EVUTIL_SOCKET_ERROR();
                cb_.OnError(this, err, evutil_socket_error_to_string(err));
            } else if (what & BEV_EVENT_READING) {

            } else if (what & BEV_EVENT_WRITING) {

            }
        }
    public:
        static void OnRecv(bufferevent *bev, void *ctx)
        {
            _My* my = static_cast<_My*>(ctx);
            if (my == nullptr) {
                return;
            }
            my->ReadDone(bev);
        }

        static void OnSend(bufferevent *bev, void *ctx)
        {
            _My* my = static_cast<_My*>(ctx);
            if (my == nullptr) {
                return;
            }
            my->SendDone(bev);
        }

        static void OnError(struct bufferevent *bev, short what, void *ctx)
        {
            _My* my = static_cast<_My*>(ctx);
            if (my == nullptr) {
                return;
            }
            my->Error(bev, what);
        }
    protected:
        friend class Service<THandle>;
    private:
        EvLoop*          loop_;
        THandle&         cb_;
        MGR&             mgr_;
        bool             is_initialize_;
        bool             is_exit_;
        std::atomic_bool is_connected_;

        bufferevent*     cli_bev_;

        // 接收缓冲区
        evbuffer* recv_buffer_;

        std::atomic_bool is_pending_;
        std::any         ctx_any_;
    };
}
}


