#pragma once
/*******
*** @file  service.h
*** @brief 对libevent进行服务器封装
*** @author xiay
*** @version v0.1.1
*** @date   2019-11-22
*************/

#include <ev_loop.h>
#include <event2/listener.h>
#include <list>
#include <unordered_set>
#include <eddid_cricular_buffer.h>
#include <string.h>
#include <any>
#include <cli_context.h>

// 服务器回调函数签名
//class Handle
//{
//public:
//    void OnAccept(CContext* conn, const char* peer, uint32_t len);       // 接受连接回调
//    void OnReadDone(CContext* conn, evbuffer*&& recv_data);              // 读完成回调
//    void OnDisConnect(CContext* conn);                                   // 断线回调
//    void OnError(CContext* conn, int err, const char* err_msg);          // 错误消息回调
//};


#define DEFAULT_LISTEN_ADDR "127.0.0.1"
#define DEFAULT_LISTEN_PORT 10012
#define DEFAULT_MAX_CONN    3000
#define DEFAULT_MAX_ABOVE_CONN 10
#define DEFAULT_MONITOR_CLI_CONTEXT_SECONDS 20

namespace eddid {
namespace event_wrap {
    template <class THandle>
    class Service
    {
    public:
        using CContext = CliContext<THandle, Service>;
        using CContextPtr = std::shared_ptr<CContext>;
        using CContextWeakPtr = std::weak_ptr<CContext>;
        using _My = Service<THandle>;
    protected:
        Service(const Service&) = delete;
        Service& operator= (const Service&) = delete;
        Service(const Service&&) = delete;
        Service& operator= (Service&&) = delete;
    public:
        Service(EvLoop* loop, THandle& handle)
            : loop_(loop)
            , cb_(handle)
            , listen_addr_(DEFAULT_LISTEN_ADDR)
            , listen_port_(DEFAULT_LISTEN_PORT)
            , is_initialize_(false)
            , is_start_(false)
            , is_exit_(false)
            , max_conn_(DEFAULT_MAX_CONN)
            , current_conn_(0)
            , is_auto_monitor_client_(false)
            , monitor_timer_repeat_(DEFAULT_MONITOR_CLI_CONTEXT_SECONDS)
            , listener_(nullptr)
            , monitor_cli_event_(nullptr)
        {
        }
        ~Service()
        {
        }
    public:
        int Initialize(const std::string& listen_addr, uint16_t listen_port, uint32_t max_conn = DEFAULT_MAX_CONN)
        {
            if (is_initialize_) {
                return 0;
            }

            listen_addr_ = listen_addr;
            listen_port_ = listen_port;
            max_conn_ = max_conn;

            int ret(0);
            // 判断当前IP是否是IPV4
            addrinfo* pLocalAddrInfo;
            addrinfo localAddrInfo;
            memset(&localAddrInfo, 0, sizeof(addrinfo));
            localAddrInfo.ai_flags = AI_PASSIVE;
            localAddrInfo.ai_family = AF_UNSPEC;
            localAddrInfo.ai_socktype = static_cast<int>(SOCK_STREAM);
            localAddrInfo.ai_protocol = IPPROTO_IP;
            ret = getaddrinfo(listen_addr_.c_str(), NULL, &localAddrInfo, &pLocalAddrInfo);
            if (ret != 0) {
                return ret;
            }
            memset(&listen_addrinfo_, 0, sizeof(addrinfo));
            memcpy(&listen_addrinfo_, pLocalAddrInfo, sizeof(addrinfo));
            freeaddrinfo(pLocalAddrInfo);

            //  创建连接池
            for (uint32_t i = 0; i < max_conn + DEFAULT_MAX_ABOVE_CONN; ++i) {
                CContextPtr ctx_ptr(new CContext(cb_, *this));
                ctx_ptr->Initialize();
                lst_free_conn_.emplace_back(std::forward<CContextPtr>(ctx_ptr));
            }

            is_initialize_ = true;
            return 0;
        }

        void Release()
        {
            Stop();
        }

        int Start()
        {
            if (!is_initialize_) {
                return -1;
            }
            if (is_start_) {
                return 0;
            }

            event_base* loop = loop_->GetWorkLoop();
            loop_->Increment(loop);
            if (listen_addrinfo_.ai_family == AF_INET) { // IPv4
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htobe16(listen_port_);
                evutil_inet_pton(AF_INET, listen_addr_.c_str(), &addr.sin_addr);

                listener_ = evconnlistener_new_bind(loop, OnAccept, this,
                    LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_REUSEABLE | LEV_OPT_REUSEABLE_PORT,
                    SOMAXCONN, (sockaddr*)&addr, sizeof(sockaddr_in));
                if (listener_ == nullptr) {
                    return -1;
                }
            } else if (listen_addrinfo_.ai_family == AF_INET6) { // IPv6
                struct sockaddr_in6 addr;
                addr.sin6_family = AF_INET6;
                addr.sin6_port = htobe16(listen_port_);
                evutil_inet_pton(AF_INET6, listen_addr_.c_str(), &addr.sin6_addr);
                listener_ = evconnlistener_new_bind(loop, OnAccept, this,
                    LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_REUSEABLE | LEV_OPT_REUSEABLE_PORT | LEV_OPT_BIND_IPV6ONLY,
                    SOMAXCONN, (sockaddr*)&addr, sizeof(sockaddr_in));
                if (listener_ == nullptr) {
                    return -1;
                }
            }
            evconnlistener_set_error_cb(listener_, OnAcceptError);

            if (is_auto_monitor_client_) {
                timeval tv;
                evutil_timerclear(&tv);
                tv.tv_sec = monitor_timer_repeat_;

                // 创建定时器
                monitor_cli_event_ = event_new(loop, -1, EV_PERSIST, OnTimer, this);
                if (monitor_cli_event_ == nullptr) {
                    return -1;
                }
                event_add(monitor_cli_event_, &tv);
            }

            is_start_ = true;
            return 0;
        }

        void Stop()
        {
            if (!is_initialize_) {
                return;
            }
            if (!is_start_) {
                return;
            }
            is_exit_ = true;

            if (listener_ != nullptr) {
                evconnlistener_disable(listener_);
                evconnlistener_free(listener_);
                listener_ = nullptr;
            }

            // 先关闭所有客户端
            for (auto it = mp_client_conn_.begin(); it != mp_client_conn_.end(); ++it) {
                if (it->first != nullptr) {
                    it->first->Close();
                }
            }

            lst_free_conn_.clear();

            if (is_auto_monitor_client_) {
                event_del(monitor_cli_event_);
                event_free(monitor_cli_event_);
                monitor_cli_event_ = nullptr;
            }
            is_start_ = false;
        }

        void SetAutoMonitorClient(bool is_auto_monitor_client, uint32_t seconds)
        {
            is_auto_monitor_client_ = is_auto_monitor_client;
            monitor_timer_repeat_ = seconds;
        }

        // 将客户端上下文放入空闲连接监听队列
        void AddConnToMonitor(CContext* conn)
        {
            if (!conn->GetContext().has_value()) {
                return;
            }

            IdlConnEntryWeakPtr idl_weak_ptr = std::any_cast<IdlConnEntryWeakPtr>(conn->GetContext());
            IdlConnEntryPtr idl_ptr(idl_weak_ptr.lock());
            std::lock_guard<std::mutex> lock(idle_conn_mutex_);
            if (idle_cricular_buffer_.size() == 0) {
                idle_cricular_buffer_.push(std::forward<IdlBucket>(IdlBucket()));
            }
            idle_cricular_buffer_.back().insert(idl_ptr);
        }

        void CloseClient(CContext* conn)
        {
            if (conn != nullptr) {
                conn->Close();
            }
        }
    protected:
        void AcceptHelper(evconnlistener* listener, evutil_socket_t fd, sockaddr *addr, int socklen)
        {
            if (current_conn_ >= max_conn_) {
                evutil_closesocket(fd);
                return;
            }

            CContextPtr ptr;
            {
                std::lock_guard<std::mutex> l(free_client_conn_mutex_);
                ptr = lst_free_conn_.front();
                lst_free_conn_.pop_front();
            }
            int ret = ptr->AcceptClient(loop_, fd, addr, socklen);
            if (ret != 0) {
                ptr->Close();
                std::lock_guard<std::mutex> l(free_client_conn_mutex_);
                lst_free_conn_.emplace_back(std::forward<CContextPtr>(ptr));
                return;
            }
            current_conn_++;
            {
                std::lock_guard<std::mutex> l(client_conn_mutex_);
                mp_client_conn_.emplace(std::make_pair(ptr.get(), ptr));
            }

            if (!is_auto_monitor_client_) {
                return;
            }

            // 添加到静默连接监听队列
            IdlConnEntryPtr idl_ptr = std::make_shared<IdleConnectEntry>(ptr);
            IdlConnEntryWeakPtr idl_weak_ptr(idl_ptr);
            ptr->SetContext(idl_weak_ptr);

            std::lock_guard<std::mutex> lock(idle_conn_mutex_);
            if (idle_cricular_buffer_.size() == 0) {
                idle_cricular_buffer_.push(std::forward<IdlBucket>(IdlBucket()));
            }
            idle_cricular_buffer_.back().insert(idl_ptr);
        }

        void Error(evconnlistener* listener)
        {

        }

        void RecycleClientConn(CContext* conn)
        {
            CContextPtr conn_ptr;
            {
                std::lock_guard<std::mutex> l(client_conn_mutex_);
                auto it = mp_client_conn_.find(conn);
                if (it == mp_client_conn_.end()) {
                    return;
                }
                conn_ptr = it->second;
                mp_client_conn_.erase(it);
            }
            current_conn_--;

            {
                std::lock_guard<std::mutex> l(free_client_conn_mutex_);
                lst_free_conn_.emplace_back(std::forward<CContextPtr>(conn_ptr));
            }
        }
    public:
        static void OnAccept(evconnlistener* listener, evutil_socket_t fd, sockaddr *addr, int socklen, void* param)
        {
            _My* my = static_cast<_My*>(param);
            if (my == nullptr) {
                return;
            }
            my->AcceptHelper(listener, fd, addr, socklen);
        }

        static void OnAcceptError(evconnlistener* listener, void* param)
        {
            _My* my = static_cast<_My*>(param);
            if (my == nullptr) {
                return;
            }
            my->Error(listener);
        }
    protected:
        friend class CliContext<THandle, _My>;
    private:
        EvLoop*               loop_;
        THandle&              cb_;                                  // 消息回调句柄
        std::string           listen_addr_;                         // 监听的地址 
        uint16_t              listen_port_;                         // 监听端口
        bool                  is_initialize_;                       // 初始化标志
        bool                  is_start_;                            // 服务起动标志
        bool                  is_exit_;                             // 服务退出标志
        uint32_t              max_conn_;                            // 最大连接数
        std::atomic_uint32_t  current_conn_;                        // 当前连接数
        bool                  is_auto_monitor_client_;              // 是否自动监控静默连接
        uint32_t              monitor_timer_repeat_;                // 静默连接监控定时器的超时时间 单位：S
        evconnlistener*       listener_;                            // 监听事件
        event*                monitor_cli_event_;                   // 监视空闲连接的定时器
        addrinfo              listen_addrinfo_;

        std::mutex client_conn_mutex_;
        std::unordered_map<CContext*, CContextPtr> mp_client_conn_;

        std::mutex free_client_conn_mutex_;
        std::list<CContextPtr> lst_free_conn_;

        // 静默连接及心跳监控处理
    public:
        class IdleConnectEntry
        {
        public:
            IdleConnectEntry(const CContextWeakPtr& ctx_weak_ptr)
                : ctx_weak_ptr_(ctx_weak_ptr)
            {}

            ~IdleConnectEntry()
            {
                CContextPtr context_ptr = ctx_weak_ptr_.lock();
                if (context_ptr) {
                    context_ptr->Close();
                }
            }
        private:
            CContextWeakPtr ctx_weak_ptr_;
        };
        using IdlConnEntryPtr = std::shared_ptr<IdleConnectEntry>;
        using IdlConnEntryWeakPtr = std::weak_ptr<IdleConnectEntry>;
        using IdlBucket = std::unordered_set<IdlConnEntryPtr>;

        friend class IdleConnectEntry;
        friend class CliContext<THandle, Service>;
        // 静默连接处理
    protected:
        std::mutex idle_conn_mutex_;
        eutil::CricularBuffer<IdlBucket> idle_cricular_buffer_;

    protected:
        void ProcessIdlConn()
        {
            std::lock_guard<std::mutex> l(idle_conn_mutex_);
            idle_cricular_buffer_.push(std::forward<IdlBucket>(IdlBucket()));
        }

        // 定时器回调
    public:
        static void OnTimer(evutil_socket_t fd, short event, void* param)
        {
            _My* my = static_cast<_My*>(param);
            if (my == nullptr) {
                return;
            }
            my->ProcessIdlConn();
        }
    };
}
}
