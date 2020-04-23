#pragma once
#include <cstring>

#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"

#include <hiredis-vip/hiredis.h>
#include <hiredis-vip/async.h>
#include <hiredis-vip/adapters/libevent.h>

/**
 *  hiredis-vip中修改处:
 *  1. hircluster.c:581 使用CLUSTER NODES获取节点地址信息时,信息类似:192.168.50.97:7000@17000,这里获取端口出错
 *  2. hircluster.c:4154 集群模式连接和断连回调时,c->data为它内部的node结构,这里把我们传入的this放到node的data中,以便在回调中取出
 *  3. hircluster.c:4128 集群模式只有在执行cmd时才去尝试连接,所以本.h:241行采用执行任意命令来尝试重连
 *  4. hircluster.c:4444/4450 增加对回调函数指针的判断,是可能放入空指针的
 * */
namespace eddid{
namespace redisutils{
enum class EN_REDIS_MODE
{
    SINGLE = 1,
    CLUSTER = 2
};

enum EN_REDIS_STATUS
{
    ERRORS = -1,
    OK = 0
};

enum EN_REDIS_REPLY_TYPE
{
    STRING = 1,
    ARRAY = 2,
    INTEGER = 3,
    NIL = 4,
    STATUS = 5,
    ERRORT = 6
};

enum EN_REDIS_ERR_TYPE
{
    ERR_IO = 1,       /* Error in read or write */
    ERR_OTHER = 2,    /* Everything else... */
    ERR_EOF = 3,      /* End of file */
    ERR_PROTOCOL = 4, /* Protocol error */
    ERR_OOM = 5       /* Out of memory */
};

template<EN_REDIS_MODE mode = EN_REDIS_MODE::SINGLE>
class CRedisWrapper
{
    using OnRedisConnectCb = eddid_function_wrap<void(EN_REDIS_STATUS status, std::string&& errMsg)>;
    using OnRedisDisConnectCb = eddid_function_wrap<void(EN_REDIS_STATUS status, std::string&& errMsg)>;
    using OnRedisReplyCb = eddid_function_wrap<void(void* privade, EN_REDIS_REPLY_TYPE& resType, std::vector<std::string>&& resVec)>;

public:
    CRedisWrapper() = delete;
    CRedisWrapper(event_base* base):
        m_base(base), 
        m_port(0), 
        m_isConn(false), 
        m_reconnTimer(nullptr), 
        m_redisCtx(nullptr), 
        m_redisClusterCtx(nullptr){}

    ~CRedisWrapper()
    {
        if(m_reconnTimer){
            event_free(m_reconnTimer);
        }
        if(m_redisCtx){
            //important: prevent call disconnect callback again
            m_redisCtx->data = nullptr;
            // if already connected, do disconnect; else just call free
            if (m_isConn) {
                redisAsyncDisconnect(m_redisCtx); // Disconnect inner will call redisAsyncFree()
            } else {
                redisAsyncFree(m_redisCtx);
            }
            m_redisCtx = nullptr;
        }

        if(m_redisClusterCtx){
            // TODO: may be need some other operation
            redisClusterAsyncFree(m_redisClusterCtx);
            m_redisClusterCtx = nullptr;
        }
    }
public:
    /**
     * Usage:
     *  // single model
     *  Connect("127.0.0.1:6379")
     *  // cluster model
     *  Connect("127.0.0.1:6379,127.0.0.1:6380,127.0.0.1:6381")
     */
    int Connect(const char* addr)
    {
        m_addr = std::string(addr);
        if(mode == EN_REDIS_MODE::SINGLE){
            m_ip = m_addr.substr(0, m_addr.find(":"));
            m_port = std::atoi(&m_addr[m_addr.find(":") + 1]);
        }
        int ret = Connect2Redis();
        // create reconnect timer
        timeval tv;
        evutil_timerclear(&tv);
        tv.tv_sec = 6;
        m_reconnTimer = event_new(m_base, -1, EV_PERSIST, &CRedisWrapper::OnReconTimer, this);
        event_add(m_reconnTimer, &tv);
        return ret;
    }

    /**
     * Usage:
     *  // Set a key
     *  Command((void*)privdata, "SET %s %s", "foo", "hello world");
     *  // Set a key using binary safe API
     *  Command((void*)privdata, "SET %b %b", "bar", (size_t) 3, "hello", (size_t) 5);
     */
    int Command(void* privdata, const char* format, ...)
    {
        if(mode == EN_REDIS_MODE::SINGLE){
            if (m_redisCtx) {
                va_list ap;
                va_start(ap, format);
                int ret = redisvAsyncCommand(m_redisCtx, &(CRedisWrapper::OnCommandReplay), privdata, format, ap);
                va_end(ap);
                return ret;
            }
        }
        else{
            if (m_redisClusterCtx) {
                va_list ap;
                va_start(ap, format);
                int ret = redisClustervAsyncCommand(m_redisClusterCtx, &(CRedisWrapper::OnClusterCommandReplay), privdata, format, ap);
                va_end(ap);
                return ret;
            }
        }
        return EN_REDIS_STATUS::ERRORS;
    }

    void RegisterConnCallback(const OnRedisConnectCb& cb)
    {
        m_connCb = cb;
    }

    void RegisterDisconnCallback(const OnRedisDisConnectCb& cb)
    {
        m_disconnCb = cb;
    }

    void RegisterReplyCallback(const OnRedisReplyCb& cb)
    {
        m_repCb = cb;
    }

protected:
    static void OnConnected(const redisAsyncContext* c, int status)
    {
        if (c->data) {
            CRedisWrapper* pThis = nullptr;
            if(mode == EN_REDIS_MODE::SINGLE){
                pThis = (CRedisWrapper*) (c->data);
            }
            else{
                cluster_node* node = (cluster_node*)(c->data);
                pThis = (CRedisWrapper*) (node->data);
            }

            if (pThis->m_connCb && !pThis->m_isConn) {// first connect
                (pThis->m_connCb)((EN_REDIS_STATUS)status, std::string(c->errstr));
            }
            if((EN_REDIS_STATUS)status == EN_REDIS_STATUS::OK){
                pThis->m_isConn = true;
            }
        }
    }

    static void OnDisConnect(const redisAsyncContext* c, int status)
    {
        if (c->data) {
            CRedisWrapper* pThis = nullptr;
            if(mode == EN_REDIS_MODE::SINGLE){
                pThis = (CRedisWrapper*) (c->data);
            }
            else{
                cluster_node* node = (cluster_node*)(c->data);
                pThis = (CRedisWrapper*) (node->data);
            }

            pThis->m_isConn = false;
            if (pThis->m_disconnCb) {
                (pThis->m_disconnCb)((EN_REDIS_STATUS)status, std::string(c->errstr));
            }
        }
    }

    static void OnCommandReplay(redisAsyncContext* c, void *r, void *privdata)
    {
        if (c->data && r) {
            CRedisWrapper* pThis = (CRedisWrapper*) (c->data);
            if (pThis->m_repCb) {
                redisReply* reply = (redisReply*)r;
                EN_REDIS_REPLY_TYPE replyType = (EN_REDIS_REPLY_TYPE)reply->type;
                std::vector<std::string> resVec;
                pThis->ProcessRedisRes(reply, resVec);
                (pThis->m_repCb)(privdata, replyType, std::move(resVec));
            }
        }
    }

    static void OnClusterCommandReplay(redisClusterAsyncContext* c, void *r, void *privdata)
    {
        if (c->data && r) {
            CRedisWrapper* pThis = (CRedisWrapper*) (c->data);
            if (pThis->m_repCb) {
                redisReply* reply = (redisReply*)r;
                EN_REDIS_REPLY_TYPE replyType = (EN_REDIS_REPLY_TYPE)reply->type;
                std::vector<std::string> resVec;
                pThis->ProcessRedisRes(reply, resVec);
                (pThis->m_repCb)(privdata, replyType, std::move(resVec));
            }
        }
    }

    static void OnReconTimer(evutil_socket_t fd, short event, void* args)
    {
        if(args){
            CRedisWrapper* pThis = (CRedisWrapper*) (args);
            if(!pThis->m_isConn){
                std::cout<<"try reconnecting..."<<std::endl;
                // redis ctx will auto free when disconnect
                pThis->Connect2Redis();
            }
        }
    }

private:
    int Connect2Redis()
    {
        int ret = 0;
        if(mode == EN_REDIS_MODE::SINGLE)
        {
            m_redisCtx = redisAsyncConnect(m_ip.c_str(), m_port);
            if (m_redisCtx) {
                if ((ret = m_redisCtx->err) == 0) {
                    m_redisCtx->data = this;
                    redisLibeventAttach(m_redisCtx, m_base);
                    redisAsyncSetConnectCallback(m_redisCtx, &CRedisWrapper::OnConnected);
                    redisAsyncSetDisconnectCallback(m_redisCtx, &CRedisWrapper::OnDisConnect);
                } else {
                    std::cerr<<"Fail to connect redis, addr: " << m_addr <<", error no: " << m_redisCtx->err << ", error msg: "<< m_redisCtx->errstr <<std::endl;
                }
            }
        }
        else{
            if(m_redisClusterCtx){
                redisClusterAsyncCommand(m_redisClusterCtx, nullptr, nullptr, "get xlh");
            }
            else{
                m_redisClusterCtx = redisClusterAsyncConnect(m_addr.c_str(), HIRCLUSTER_FLAG_NULL);
                if (m_redisClusterCtx) {
                    if ((ret = m_redisClusterCtx->err) == 0) {
                        m_redisClusterCtx->data = this;
                        redisClusterLibeventAttach(m_redisClusterCtx, m_base);
                        redisClusterAsyncSetConnectCallback(m_redisClusterCtx, &CRedisWrapper::OnConnected);
                        redisClusterAsyncSetDisconnectCallback(m_redisClusterCtx, &CRedisWrapper::OnDisConnect);
                        redisClusterAsyncCommand(m_redisClusterCtx, nullptr, nullptr, "get xlh");
                    } else {
                        std::cerr<<"Fail to connect redis, addr: " << m_addr <<", error no: " << m_redisClusterCtx->err << ", error msg: "<< m_redisClusterCtx->errstr <<std::endl;
                    }
                }
            }
            
        }
        return ret;
    }

    void ProcessRedisRes(redisReply* reply, std::vector<std::string>& resVec)
    {
        if(reply){
            EN_REDIS_REPLY_TYPE replyType = (EN_REDIS_REPLY_TYPE)reply->type;
            auto getStrRes = [&](redisReply *oneReply) {
                if ((EN_REDIS_REPLY_TYPE)oneReply->type == EN_REDIS_REPLY_TYPE::INTEGER){
                    resVec.emplace_back(std::to_string(oneReply->integer));
                }
                else{
                    resVec.emplace_back(std::string(oneReply->str, oneReply->len));
                }
            };
            if (replyType != EN_REDIS_REPLY_TYPE::ARRAY){
                getStrRes(reply);
            }
            else{
                size_t arraySize = reply->elements;
                resVec.reserve(arraySize);
                for (size_t i = 0; i < arraySize; ++i)
                {
                    getStrRes(reply->element[i]);
                }
            }
        }
    }

public:
    event_base* m_base;
    std::string m_ip;
    int m_port;
    std::string m_addr;
    bool m_isConn;
    event* m_reconnTimer;
    redisAsyncContext* m_redisCtx;
    redisClusterAsyncContext* m_redisClusterCtx;
    OnRedisConnectCb m_connCb;
    OnRedisDisConnectCb m_disconnCb;
    OnRedisReplyCb m_repCb;

};

} // namespace redisutils
} // namespace eddid