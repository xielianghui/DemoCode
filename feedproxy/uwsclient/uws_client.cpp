#include "uws_client.h"


UwsClient::UwsClient():
    m_ws(nullptr),
    m_cb(nullptr){}

UwsClient::~UwsClient(){}

void UwsClient::Register(event_base* base, OnRecv cb)
{
    m_cb = cb;
    m_hub.onConnection([this](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest /*req*/) {
        this->m_ws = ws;
        std::cout<<"Uws connect"<<std::endl;
    });
    m_hub.onMessage([this](uWS::WebSocket<uWS::CLIENT> *ws, char *data, size_t len, uWS::OpCode type) {
        this->m_mtx.lock();
        this->m_resVec.emplace_back(std::string(data, len));
        this->m_mtx.unlock();
        this->m_notify->Notify();
    });

    m_notify = std::make_shared<EventNotify<UwsClient>>(base, this, &UwsClient::OnNotify);
    m_notify->Init();
}

int UwsClient::Connect(std::string addr, int port)
{
    std::string hostName = ("ws://" + addr + ":" + std::to_string(port));
    m_hub.connect(hostName);
    m_thread = std::thread(&UwsClient::Run, this);
    m_thread.detach();
    return 0;
}

int UwsClient::SendReq(const std::string& msg)
{
    if(m_ws){
        m_ws->send(msg.c_str());
        return 0;
    }
    else{
        return -1;
    }
}

void UwsClient::Run()
{
    m_hub.run();
}

void UwsClient::OnNotify()
{
    std::vector<std::string> resVec;
    m_mtx.lock();
    resVec.swap(m_resVec);
    m_mtx.unlock();
    (*m_cb)(std::move(resVec));
}