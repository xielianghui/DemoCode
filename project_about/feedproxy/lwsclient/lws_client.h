#pragma once
#include <iostream>
#include <queue>
#include <memory>

#include "libwebsockets.h"

#include "../levservice/ev_loop.h"
#include "lws_cb_interface.h"

#define MAX_PAYLOAD_SIZE  10 * 1024

struct session_data
{
    unsigned char buf[LWS_PRE + MAX_PAYLOAD_SIZE];
};

class LwsClient final
{
public:
    LwsClient() = delete;
    LwsClient(event_base* pEvBase);
    ~LwsClient();
public:
    int SendReq(const std::string& msg);
    int Connect(std::string addr, int port);
    void Register(LwsCbInterface* pCb);
    LwsCbInterface* getCbPtr();
    std::string getOneReq();
    void Run();
private:
    static void OnRunTimer(evutil_socket_t fd, short event, void* args);
private:
    int m_port;
    std::string m_addr;
    LwsCbInterface* m_pCb;
    std::queue<std::string> m_reqQue;
    session_data m_userData;
    event_base* m_pEvBase;
    event* m_lpTimerEv;
};