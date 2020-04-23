#pragma once
#include <sys/eventfd.h>

#include <zmq.h>
#include <event.h>

#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"
#include "../utils/concurrentqueue.h"

namespace eddid{
namespace zmqutils{

struct ST_ZmqMsg
{
    std::string routingID;
    std::vector<std::string> msgVec;
    ST_ZmqMsg(const std::string& r, const std::vector<std::string>& m):routingID(r),msgVec(m){}
};

struct ZMQTraits: public moodycamel::ConcurrentQueueDefaultTraits
{
    static const size_t BLOCK_SIZE = 256;
};

class CZmqWrapper
{
    using OnRecvMsg = eddid_function_wrap<void(const std::string&, std::string&&)>;

public:
    CZmqWrapper(event_base* pEvBase = nullptr);
    ~CZmqWrapper();

public:
    int Init(int socketType, const std::string& routingID = "");
    int Bind(const std::string& addr);
    int UnBind(const std::string& addr);
    int Connect(const std::string& addr);
    int Disconnect(const std::string& addr);
    int Send(const std::string& msg, const std::string& routingID = "");
    int Send(const std::vector<std::string>& msgVec, const std::string& routingID = "");
    int Sub(const std::string& topic);
    int UnSub(const std::string& topic);
    int SetSocketOption(int optionName, const void* optionValue, size_t optionLen);
    int GetSocketOption(int optionName, void* optionValue, size_t* optionValueLen);
    void* getSocket();
    void RegisterCallback(const OnRecvMsg& cb);

private:
    static void OnRead(evutil_socket_t fd, short event, void *args);
    static void OnNotify(evutil_socket_t fd, short event, void *args);

private:
    void DoLoop();
    void DoRecvMsg();
    void DoDequeueMsg();

private:
    OnRecvMsg m_cb;
    event_base* m_base;
    event* m_readEvent;
    static void* m_zmqCtx;
    void* m_zmqSocket;
    int m_socketType;
    bool m_outerBase;
    std::thread m_recvThread;

    moodycamel::ConcurrentQueue<std::shared_ptr<ST_ZmqMsg>, ZMQTraits> m_sendQueue;
    std::atomic<bool> m_isExit;

    int m_eventFd;
    event* m_notifyEvent;
};

}
}