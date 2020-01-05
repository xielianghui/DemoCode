#pragma once
#include <iostream>
#include <memory>
#include <thread>

#include "../public/event_notify.h"
#include "../thirdparty/uWS/uWS.h"


class UwsClient final
{
public:
    typedef void(*OnRecv)(std::string&&);

    UwsClient();
    UwsClient(event_base* pEvBase);
    ~UwsClient();
public:
    int SendReq(const std::string& msg);
    void Register(event_base* base, OnRecv cb);
    int Connect(std::string addr, int port);
private:
    void Run();
    void OnNotify(std::string&& msg);
    static void OnConnect(uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest /*req*/);
    static void OnMessage(uWS::WebSocket<uWS::CLIENT> *ws, char *data, size_t len, uWS::OpCode type);
public:
    uWS::WebSocket<uWS::CLIENT>* m_ws;
    std::shared_ptr<EventNotify<UwsClient>> m_notify;
private:
    OnRecv m_cb;
    int m_port;
    std::string m_addr;
    uWS::Hub m_hub;
    std::thread m_thread;
};