#include "zmq_wrapper.h"

using namespace eddid::zmqutils;

void* CZmqWrapper::m_zmqCtx = nullptr;

CZmqWrapper::CZmqWrapper(event_base* pEvBase):
    m_base(pEvBase),
    m_readEvent(nullptr),
    m_zmqSocket(nullptr),
    m_outerBase(true),
    m_isExit(false),
    m_eventFd(-1),
    m_notifyEvent(nullptr){}

CZmqWrapper::~CZmqWrapper()
{
    // stop recv
    if(m_readEvent){
        event_free(m_readEvent);
    }
    if(m_notifyEvent){
        event_free(m_notifyEvent);
    }
    if(!m_outerBase){
        m_recvThread.join();
    }
    if(m_eventFd != -1){
        close(m_eventFd);
    }
    // free zmq
    if(m_zmqSocket){
        zmq_close(m_zmqSocket);
    }

    // if(m_zmqCtx){
    //     zmq_ctx_destroy(m_zmqCtx);
    // }
}

/**
 * \brief init service
 * \param socketType socket types. e.g.
 * ZMQ_PAIR | ZMQ_PUB/ZMQ_SUB | ZMQ_REQ/ZMQ_REP | ZMQ_DEALER/ZMQ_ROUTER
 * ZMQ_PULL/ZMQ_PUSH | ZMQ_XPUB/ZMQ_XSUB | ZMQ_STREAM
 * \param routingID the routing id of the specified socket when connecting to a ROUTER socket.
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::Init(int socketType, const std::string& routingID)
{
    m_socketType = socketType;
    // create context
    if (m_zmqCtx == nullptr){
        static std::once_flag flag;
        std::call_once(flag, []() {
            m_zmqCtx = zmq_ctx_new();
            if (m_zmqCtx == nullptr){
                std::cerr << "New zmq context failed" << std::endl;
                return;
            }
            // context options set
            zmq_ctx_set(m_zmqCtx, ZMQ_IO_THREADS, 3);// specifies the size of the ØMQ thread pool to handle I/O operations. 
        });
    }
    if (m_zmqCtx == nullptr) return -1;
    // create socket
    m_zmqSocket = zmq_socket(m_zmqCtx, socketType);
    if(m_zmqSocket == nullptr){
        std::cerr << "Create zmq socket failed" <<std::endl;
        zmq_ctx_destroy(m_zmqCtx);
        return -1;
    }
    // socket options set
    if(!routingID.empty()){
        // applicable socket types: ZMQ_REQ, ZMQ_REP, ZMQ_ROUTER, ZMQ_DEALER.
        // a routing id must be at least one byte and at most 255 bytes long.
        zmq_setsockopt(m_zmqSocket, ZMQ_ROUTING_ID, routingID.c_str(), routingID.length());
    }
    int optValue = 0; // zmq_send()/zmq_recv() will return immediately, with a EAGAIN error if the message cannot be sent/receive.
    zmq_setsockopt(m_zmqSocket, ZMQ_SNDTIMEO, &optValue, sizeof(optValue));
    zmq_setsockopt(m_zmqSocket, ZMQ_RCVTIMEO, &optValue, sizeof(optValue));
    optValue = 0; // no limit 
    zmq_setsockopt(m_zmqSocket, ZMQ_SNDHWM, &optValue, sizeof(optValue));
    zmq_setsockopt(m_zmqSocket, ZMQ_RCVHWM, &optValue, sizeof(optValue));
    optValue = 10; // reconnect interval(ms)
    zmq_setsockopt(m_zmqSocket, ZMQ_RECONNECT_IVL, &optValue, sizeof(optValue));
    optValue = 2000; // max reconnect interval(ms)
    zmq_setsockopt(m_zmqSocket, ZMQ_RECONNECT_IVL_MAX, &optValue, sizeof(optValue));
    optValue = 1; // tcp keepalive
    zmq_setsockopt(m_zmqSocket, ZMQ_TCP_KEEPALIVE, &optValue, sizeof(optValue));
    
    optValue = 3; // keepalive pkg send interval(s)
    zmq_setsockopt(m_zmqSocket, ZMQ_TCP_KEEPALIVE_IDLE, &optValue, sizeof(optValue));
    optValue = 1; // keepalive pkg send retry interval(s)
    zmq_setsockopt(m_zmqSocket, ZMQ_TCP_KEEPALIVE_INTVL, &optValue, sizeof(optValue));
    optValue = 3; // keepalive pkg send retry times
    zmq_setsockopt(m_zmqSocket, ZMQ_TCP_KEEPALIVE_CNT, &optValue, sizeof(optValue));
    optValue = 0; // discarded immediately Pending messages when call zmq_disconnect() or zmq_close().
    zmq_setsockopt(m_zmqSocket, ZMQ_LINGER, &optValue, sizeof(optValue));

    // if base mot empty, create internal base and thread
    if(m_base == nullptr){
        m_outerBase = false;
        m_base = event_base_new();
    }
    int fd = -1;
    size_t fdSize = sizeof(fd);
    int rc = zmq_getsockopt(m_zmqSocket, ZMQ_FD, &fd, &fdSize);
    if(rc != 0){
        std::cerr << "Get zmq fd failed" << ", error no: " << errno << ", error msg: " <<  zmq_strerror(errno) << std::endl;
        return -1;
    }
    m_readEvent = event_new(m_base, fd, EV_READ | EV_PERSIST, OnRead, this);
    if(m_readEvent == nullptr){
        std::cerr << "event_new() failed" << std::endl;
        return -1;
    }

    // create event fd 
    m_eventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if(m_eventFd < 0){
        std::cerr<<"eventfd() failed, error msg: "<<strerror(errno)<<std::endl;
        return -1;
    }
    m_notifyEvent = event_new(m_base, m_eventFd, EV_READ | EV_PERSIST, OnNotify, this);

    //https://github.com/zeromq/libzmq/issues/1434
    timeval tv{0, 0};
    tv.tv_usec = 1000 * 100;
    event_add(m_readEvent, &tv);
    event_add(m_notifyEvent, nullptr);
    if(!m_outerBase){
        m_recvThread = std::thread(&CZmqWrapper::DoLoop, this);
    }

    return 0;
}

/**
 * \brief binds the socket to a local endpoint and then accepts incoming connections on that endpoint.
 * \param addr consisting of a transport:// followed by an address, e.g.*/
// tcp: tcp://*:5555 | tcp://127.0.0.1:5555 | tcp://eth0:5555
/* ipc: ipc:///tmp/feeds/0
    * inproc: inproc://my-endpoint
    * \return returns zero if successful. Otherwise it returns -1
    */
int CZmqWrapper::Bind(const std::string& addr)
{
    int rc = zmq_bind(m_zmqSocket, addr.c_str());
    if(rc != 0){
        std::cerr << "Zmq bind failed" << ", error no: " << errno << ", error msg: " <<  zmq_strerror(errno) << std::endl;
    }
    return rc;
}

/**
 * \brief unbind a socket specified by the socket argument from the endpoint specified by the endpoint argument.
 * \param addr when bind use wild-card *, the caller should use real endpoint 
 * obtained from the ZMQ_LAST_ENDPOINT socket option to unbind this endpoint from a socket.
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::UnBind(const std::string& addr)
{
    int rc = zmq_bind(m_zmqSocket, addr.c_str());
    if(rc != 0){
        std::cerr << "Zmq unbind failed" << ", error no: " << errno << ", error msg: " <<  zmq_strerror(errno) << std::endl;
    }
    return rc;
}

/**
 * \brief connects the socket to an endpoint and then accepts incoming connections on that endpoint.
 * \param addr consisting of a transport:// followed by an address, not support wild-card *. e.g.
 * tcp: tcp://192.168.1.1:5555
 * ipc: ipc:///tmp/feeds/0
 * inproc: inproc://my-endpoint
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::Connect(const std::string& addr)
{
    int rc = zmq_connect(m_zmqSocket, addr.c_str());
    if(rc != 0){
        std::cerr << "Zmq connect failed" << ", error no: " << errno << ", error msg: " <<  zmq_strerror(errno) << std::endl;
    }
    return rc;
}

/**
 * \brief disconnect a socket specified by the socket argument from the endpoint specified by the endpoint argument.
 * \param addr same with connect addr
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::Disconnect(const std::string& addr)
{
    int rc = zmq_disconnect(m_zmqSocket, addr.c_str());
    if(rc != 0){
        std::cerr << "Zmq connect failed" << ", error no: " << errno << ", error msg: " <<  zmq_strerror(errno) << std::endl;
    }
    return rc;
}

/**
 * \brief send a message part on a socket.A successful invocation of zmq_send() does not indicate that 
 * the message has been transmitted to the network, only that it has been queued on the socket 
 * and ØMQ has assumed responsibility for the message.
 * \param msg_or_msgVec message you want to send or pub
 * \param routingID peer routing ID, ZMQ_ROUTER need specified ID
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::Send(const std::string& msg, const std::string& routingID)
{
    std::vector<std::string> msgVec;
    msgVec.emplace_back(msg);
    return Send(msgVec, routingID);
}

int CZmqWrapper::Send(const std::vector<std::string>& msgVec, const std::string& routingID)
{
    if(msgVec.empty()) return -1;
    auto msgPtr = std::make_shared<ST_ZmqMsg>(routingID, msgVec);
    m_sendQueue.enqueue(std::move(msgPtr));
    uint64_t u = 1;
    if(write(m_eventFd, &u, sizeof(uint64_t)) != sizeof(uint64_t)){
        return -1; 
    }
    return 0;
}

/**
 * \brief establish a new message filter on a ZMQ_SUB socket.
 * \param topic sub topic.
 * An empty option_value of length zero shall subscribe to all incoming messages.
 * A non-empty option_value shall subscribe to all messages beginning with the specified prefix.
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::Sub(const std::string& topic)
{
    if(zmq_setsockopt(m_zmqSocket, ZMQ_SUBSCRIBE, topic.c_str(), topic.size()) == -1){
        std::cerr << "Sub failed" <<", topic: " << topic << 
        ", error no: " << errno << ", error msg: " << zmq_strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

/**
 * \brief remove an existing message filter on a ZMQ_SUB socket.
 * \param topic unsub topic.
 * If the socket has several instances of the same filter attached 
 * the ZMQ_UNSUBSCRIBE option shall remove only one instance, 
 * leaving the rest in place and functional.
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::UnSub(const std::string& topic)
{
    if(zmq_setsockopt(m_zmqSocket, ZMQ_UNSUBSCRIBE, topic.c_str(), topic.size()) == -1){
        std::cerr << "UnSub failed" <<", topic: " << topic << 
            ", error no: " << errno << ", error msg: " << zmq_strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

/**
 * \brief set the option specified by the optionName argument 
 * to the value pointed to by the optionValue argument for 
 * the ØMQ socket pointed to by the socket argument. 
 * \param optionName option you want to set. http://api.zeromq.org/master:zmq-setsockopt
 * \param optionValue set option to that value
 * \param optionsLen option value length
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::SetSocketOption(int optionName, const void* optionValue, size_t optionLen)
{
    if(zmq_setsockopt(m_zmqSocket, optionName, optionValue, optionLen) == -1){
        std::cerr << "Set socket option failed" << ", option name: " << optionName << 
            ", error no: " << errno << ", error msg: " << zmq_strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

/**
 * \brief retrieve the value for the option specified by the optionName argument, 
 * and store it in the buffer pointed to by the optionValue argument. 
 * \param optionName option you want to get value. http://api.zeromq.org/master:zmq-getsockopt
 * \param optionValue set option to that value
 * \param optionsLen option value length
 * \return returns zero if successful. Otherwise it returns -1
 */
int CZmqWrapper::GetSocketOption(int optionName, void* optionValue, size_t* optionValueLen)
{
    if(zmq_getsockopt(m_zmqSocket, optionName, optionValue, optionValueLen) == -1){
        std::cerr << "Get socket option failed" << ", option name: " << optionName << 
            ", error no: " << errno << ", error msg: " << zmq_strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

/**
 * \brief get zmq socket pointer
 * \return returns socket pointer, maybe nullptr
 */
void* CZmqWrapper::getSocket()
{
    return m_zmqSocket;
}

void CZmqWrapper::RegisterCallback(const OnRecvMsg& cb)
{
    m_cb = cb;
}

void CZmqWrapper::OnRead(evutil_socket_t fd, short event, void *args)
{
    CZmqWrapper* pThis = static_cast<CZmqWrapper*>(args);
    if (pThis){
        pThis->DoRecvMsg();
    }
}

void CZmqWrapper::OnNotify(evutil_socket_t fd, short event, void *args)
{
    CZmqWrapper* pThis = static_cast<CZmqWrapper*>(args);
    if (pThis){
        pThis->DoDequeueMsg();
    }
}

void CZmqWrapper::DoLoop()
{
    event_base_dispatch(m_base);
}

void CZmqWrapper::DoRecvMsg()
{
    if(m_zmqSocket)
    {
        // get zmq fd event
        int zmqEvent = 0;
        size_t zmqEventSize = sizeof(zmqEvent);
        zmq_getsockopt(m_zmqSocket, ZMQ_EVENTS, &zmqEvent, &zmqEventSize);

        std::string routingID;
        // Indicates that at least one message may be received from the specified socket without blocking.
        while(zmqEvent & ZMQ_POLLIN)
        {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            bool haveGetRoutingID = false;
            bool haveGetTopic = false;
            while(zmq_msg_recv(&msg, m_zmqSocket, ZMQ_DONTWAIT) >= 0)
            {
                std::string strMsg((char *)zmq_msg_data(&msg), zmq_msg_size(&msg));
                if((m_socketType == ZMQ_ROUTER || m_socketType == ZMQ_STREAM) && !haveGetRoutingID){
                    /**
                     * ZMQ_ROUTER/ZMQ_STREAM shall remove the first part of the message 
                     * and use it to determine the routing id of the peer the message shall be routed to.
                     */
                    routingID.swap(strMsg);
                    haveGetRoutingID = true;
                }
                else if(m_socketType == ZMQ_SUB && !haveGetTopic){
                    // topic no need use
                    haveGetTopic = true;
                }
                else{
                    // callback
                    if (m_cb){
                        m_cb(routingID, std::move(strMsg));
                    }
                }
                int more = zmq_msg_more(&msg);
                zmq_msg_close(&msg);
                if(more){
                    zmq_msg_init(&msg);
                }
                else{
                    break;
                }
            }
            // update the state of ZMQ_EVENTS after each invocation of zmq_send or zmq_recv
            zmqEvent = 0;
            zmq_getsockopt(m_zmqSocket, ZMQ_EVENTS, &zmqEvent, &zmqEventSize);
        }
    }
}

void CZmqWrapper::DoDequeueMsg()
{
    uint64_t u;
    read(m_eventFd, &u, sizeof(uint64_t));
    std::shared_ptr<ST_ZmqMsg> oneMsg;
    while(m_sendQueue.try_dequeue(oneMsg)) {
        if(!oneMsg) continue;
        std::string& routingID = oneMsg->routingID;
        std::vector<std::string>& msgVec = oneMsg->msgVec;
        // real zmq send
        if(msgVec.empty()) continue;
        if(!routingID.empty() && (m_socketType == ZMQ_ROUTER || m_socketType == ZMQ_STREAM)){
            if(zmq_send(m_zmqSocket, routingID.c_str(), routingID.size(), ZMQ_SNDMORE | ZMQ_DONTWAIT) == -1){
                std::cerr << "Zmq send failed" << ", error no: " << errno << ", error msg: " <<  zmq_strerror(errno) << std::endl;
                continue;
            }
        }
        for(unsigned int i = 0; i < (msgVec.size() - 1); ++i)
        {
            if(zmq_send(m_zmqSocket, msgVec[i].c_str(), msgVec[i].size(), ZMQ_SNDMORE | ZMQ_DONTWAIT) == -1){
                std::cerr << "Zmq send failed" << ", error no: " << errno << ", error msg: " <<  zmq_strerror(errno) << std::endl;
                continue;
            }
        }
        /**
         * sends multi-part messages must use the ZMQ_SNDMORE flag
         * when sending each message part except the final one.
         */
        if (zmq_send(m_zmqSocket, msgVec.back().c_str(), msgVec.back().size(), ZMQ_DONTWAIT) == -1){
            std::cerr << "Zmq send failed" << ", error no: " << errno << ", error msg: " << zmq_strerror(errno) << std::endl;
            continue;
        }
    }
}
