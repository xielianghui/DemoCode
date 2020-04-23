#pragma once
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>

#include "../thirdparty/uWS/uWS.h"
#include "../public/event_notify.h"

template<typename T>
class UwsClient final
{
public:
    typedef void(T::*OnRecv)(std::vector<std::string>&&);

    UwsClient(T* mainService):
        m_ws(nullptr),
        m_mainService(mainService),
        m_cb(nullptr),
        m_swapTimer(nullptr){}

    ~UwsClient()
    {
        if(m_swapTimer){
            event_free(m_swapTimer);
        }
    }
public:
    int SendReq(const std::string& msg)
    {
        if(m_ws){
            if(!m_ws->isShuttingDown() && !m_ws->isClosed()){
                m_ws->send(msg.c_str());
                return 0;
            }
        }
        else{
            return -1;
        }
    }
    
    void Register(event_base* base, OnRecv cb)
    {
        m_pBase = base;
        m_cb = cb;
        m_hub.onConnection([this](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest /*req*/) {
            std::cout<<"Connected"<<std::endl;
            this->m_ws = ws;
        });
        m_hub.onMessage([this](uWS::WebSocket<uWS::CLIENT> *ws, char *data, size_t len, uWS::OpCode type) {
            this->m_mtx.lock();
            this->m_resVec.emplace_back(std::string(data, len));
            this->m_mtx.unlock();
        });
        m_hub.onDisconnection([this](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *data, size_t len) {
            std::cout<<"Websocket disconnect"<<std::endl;
            timeval tv{2, 0};
        });
        m_hub.onError([this](uWS::Group<uWS::CLIENT>::errorType) {
            std::cout<<"Connect error"<<std::endl;
        });

        timeval tv;
        evutil_timerclear(&tv);
        tv.tv_usec = 10000;
        m_swapTimer = event_new(base, -1, EV_PERSIST , OnSwapTimer, this);
        if (m_swapTimer == nullptr) {
            std::cout<<"event_new() swap timer failed"<<std::endl;
            return ;
        }
        event_add(m_swapTimer, &tv);
    }

    int Connect(std::string addr, int port)
    {
        m_hostName = ("ws://" + addr + ":" + std::to_string(port));
        m_hub.connect(m_hostName);
        m_thread = std::thread(&UwsClient::Run, this);
        m_thread.detach();
        return 0;
    }

    void OnNotify()
    {
        std::vector<std::string> resVec;
        m_mtx.lock();
        resVec.swap(m_resVec);
        m_mtx.unlock();
        (m_mainService->*m_cb)(std::move(resVec));
    }

private:
    void Run()
    {
        m_hub.run();
    }

    static void OnSwapTimer(evutil_socket_t fd, short event, void* args)
    {
        UwsClient<T>* th = (UwsClient<T>*)args;
        th->OnNotify();
    }

public:
    std::mutex m_mtx;
    std::vector<std::string> m_resVec;
    uWS::WebSocket<uWS::CLIENT>* m_ws;
    T* m_mainService;
    event_base* m_pBase;
    OnRecv m_cb;
    int m_port;
    std::string m_addr;
    uWS::Hub m_hub;
    std::thread m_thread;
    event* m_swapTimer;
    std::string m_hostName;
};