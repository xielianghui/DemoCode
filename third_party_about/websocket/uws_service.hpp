#pragma once
#include "uWS/uWS.h"

#include "ws_common.h"
#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"

namespace eddid{
namespace uwsutils{

struct ST_WS
{
    uWS::WebSocket<uWS::SERVER>* ws;
    ST_WS(uWS::WebSocket<uWS::SERVER>* w):ws(w){}
    ~ST_WS(){
        if(ws){
            ws->shutdown();
        }
    }
};

class CRoughTimingWheel
{
public:
    CRoughTimingWheel(int size, int tickMs):m_wheelSize(size),m_tickMs(tickMs),m_currentTime(0)
    {
        m_bucketVec.resize(m_wheelSize);
    }

    ~CRoughTimingWheel(){}

public:
    void Add(std::shared_ptr<ST_WS> ptr, int timeMs)
    {
        int advanceTimes = timeMs / m_tickMs;
        int nextTime = (m_currentTime + advanceTimes) % m_wheelSize;
        m_bucketVec[nextTime].push_back(ptr);
    }

    void AdvanceClock()
    {
        m_currentTime = (m_currentTime + 1) % m_wheelSize;
        m_bucketVec[m_currentTime].clear();

    }

private:
    int m_wheelSize;
    int m_tickMs;
    int m_currentTime;
    std::vector<std::vector<std::shared_ptr<ST_WS>>> m_bucketVec;
};

class UwsService
{
    using OnConnection = eddid_function_wrap<void(uWS::WebSocket<uWS::SERVER>* )>;
    using OnDisconnection = eddid_function_wrap<void(uWS::WebSocket<uWS::SERVER>*)>;
    using OnMessage = eddid_function_wrap<void(uWS::WebSocket<uWS::SERVER>*, char *, size_t , EN_OP_CODE)>;
    using OnError = eddid_function_wrap<void(int)>;

    EDDID_DISALLOW_COPY(UwsService)
public:
    UwsService():
        m_isExit(true),
        m_maxHeartbeatTime(-1),
        m_hbCheckTimerPtr(nullptr),
        m_timeWheelPtr(nullptr){}
    
    ~UwsService(){}

public:

    int Init(int port, const char* addr = nullptr, int maxHeartbeatTimeMs = -1)
    {
        m_maxHeartbeatTime = maxHeartbeatTimeMs;
        m_timeWheelPtr = std::make_shared<CRoughTimingWheel>(60, 1000);

        // register uws callback functions
        m_hub.onConnection([this](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
            auto wsPtr = std::make_shared<ST_WS>(ws);
            this->m_timeWheelPtr->Add(wsPtr, m_maxHeartbeatTime);
            this->m_wsMap[(intptr_t)ws] = std::weak_ptr<ST_WS>(wsPtr);
            if(this->m_conCb){
                this->m_conCb(ws);
            }
        });

        m_hub.onDisconnection([this](uWS::WebSocket<uWS::SERVER> *ws, int code, char *data, size_t len) {
            this->m_wsMap.erase((intptr_t)ws);
            if(this->m_disconCb){
                this->m_disconCb(ws);
            }
        });

        m_hub.onError([this](int port) {
            if(this->m_errCb){
                this->m_errCb(port);
            }
        });

        m_hub.onMessage([this](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
            auto it = m_wsMap.find((intptr_t)ws);
            if(it != m_wsMap.end() && !it->second.expired()){
                auto wsPtr = it->second.lock();
                this->m_timeWheelPtr->Add(wsPtr, m_maxHeartbeatTime);
            }
            if (this->m_msgCb){
                this->m_msgCb(ws, message, length, (EN_OP_CODE)opCode);
            }
        });

        return m_hub.listen(addr, port, nullptr, uS::ListenOptions::REUSE_PORT) ? 0 : -1;
    }

    void Broadcast(const char* data, size_t len, EN_OP_CODE opCode = EN_OP_CODE::BINARY)
    {
        m_hub.getDefaultGroup<uWS::SERVER>().broadcast(data, len, (uWS::OpCode)opCode);
    }

    /**
     * \brief 启动后台线程,所有回调由该线程调用，外部注意线程同步问题
     */
    void Run()
    {
        if(m_isExit){
            // timer start
            if(m_maxHeartbeatTime > 1000){
                m_hbCheckTimerPtr = std::make_shared<uS::Timer>(m_hub.getLoop());
                m_hbCheckTimerPtr->setData(this);
                m_hbCheckTimerPtr->start(&UwsService::OnHbCheckTimer, 500, 1000);
            }
            // thread start 
            m_isExit = false;
            m_thread = std::thread(&UwsService::DoRun, this);
            m_thread.detach();
        }
    }

    void Stop()
    {
        m_isExit = true;
        if(m_hbCheckTimerPtr != nullptr){
            m_hbCheckTimerPtr->stop();
            m_hbCheckTimerPtr->close();
        }
        m_hub.getDefaultGroup<uWS::SERVER>().close();
    }

    void setConnectionCb(const OnConnection& cb)
    {
        m_conCb = cb;
    }

    void setDisconnectionCb(const OnDisconnection& cb)
    {
        m_disconCb = cb;
    }

    void setMessageCb(const OnMessage& cb)
    {
        m_msgCb = cb;
    }

    void setErrorCb(const OnError& cb)
    {
        m_errCb = cb;
    }

public:
    void CheckHeartBeat()
    {
        m_timeWheelPtr->AdvanceClock();
    }

public:
    static void OnHbCheckTimer(uS::Timer* timer)
    {
        UwsService *th = (UwsService*) timer->getData();
        if(th){
            th->CheckHeartBeat();
        }
    }
private:
    void DoRun()
    {
        m_hub.run();
    }

public:
    OnConnection m_conCb;
    OnDisconnection m_disconCb;
    OnMessage m_msgCb;
    OnError m_errCb;

    std::thread m_thread;
    uWS::Hub m_hub;
    std::atomic<bool> m_isExit;
    std::unordered_map<intptr_t, std::weak_ptr<ST_WS>> m_wsMap;

    int m_maxHeartbeatTime;
    std::shared_ptr<uS::Timer> m_hbCheckTimerPtr;
    std::shared_ptr<CRoughTimingWheel> m_timeWheelPtr;
};

}
}