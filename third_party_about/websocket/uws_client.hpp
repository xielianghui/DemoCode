#pragma once
#include <chrono>

#include "uWS/uWS.h"

#include "ws_common.h"
#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"

namespace eddid{
namespace uwsutils{

class UwsClient 
{
    using OnConnection = eddid_function_wrap<void()>;
    using OnDisconnection = eddid_function_wrap<void()>;
    using OnMessage = eddid_function_wrap<void(char *, size_t , EN_OP_CODE)>;
    using OnError = eddid_function_wrap<void()>;

    EDDID_DISALLOW_COPY(UwsClient)
public:
    UwsClient():
        m_ws(nullptr),
        m_isExit(true){}
    
    ~UwsClient()
    {
        m_isExit = true;
        Stop();
        m_thread.join();
    }
public:

    /**
     * \brief 初始化连接信息
     * \param url 服务器的url路径,例如:
     * ws://127.0.0.1:2222/ws/API
     * wss://127.0.0.1:2222/ws/API (https)
     * \param extraHeaders http握手时增加的头
     * \return 0表示成功,-1表示失败
     */
    int Init(const std::string& url, const std::map<std::string, std::string>& extraHeaders = {})
    {
        m_hostUrl = url;
        m_extraHeaders = extraHeaders;
        // register uws callback functions
        m_hub.onConnection([this](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
            this->m_ws = ws;
            if(this->m_conCb){
                this->m_conCb();
            }
        });

        m_hub.onDisconnection([this](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *data, size_t len) {
            if(this->m_disconCb){
                this->m_disconCb();
            }
            if(!m_isExit){// need reconnect
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 3));
                m_hub.connect(m_hostUrl, nullptr, m_extraHeaders);
            }
        });

        m_hub.onMessage([this](uWS::WebSocket<uWS::CLIENT> *ws, char *data, size_t len, uWS::OpCode type) {
            if (this->m_msgCb){
                this->m_msgCb(data, len, (EN_OP_CODE)type);
            }
        });

        m_hub.onError([this](void* ) {
            std::cerr << "uWebsockets error" << std::endl;
            if(this->m_errCb){
                this->m_errCb();
            }
            if(!m_isExit){// need reconnect
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 3));
                m_hub.connect(m_hostUrl, nullptr, m_extraHeaders);
            }
        });

        m_hub.connect(m_hostUrl, nullptr, m_extraHeaders);
        return 0;
    }

    /**
     * \brief 发送消息(线程安全)
     * \param data 数据指针
     * \param size 数据长度
     * \param opCode 发送数据的类型,具体含义可以看websocket头协议
     * \return 0表示成功,-1表示失败
     */
    int SendMsg(const char* data, size_t len, EN_OP_CODE opCode = EN_OP_CODE::BINARY)
    {
        if(m_ws){
            if(!m_ws->isShuttingDown() && !m_ws->isClosed()){
                m_ws->send(data, len, (uWS::OpCode)opCode);
                return 0;
            }
        }
        return -1;
    }

    /**
     * \brief 启动后台线程,所有回调由该线程调用，外部注意线程同步问题
     */
    void Run()
    {
        if(m_isExit){
            m_isExit = false;
            m_thread = std::thread(&UwsClient::DoRun, this);
        }
    }

    void Stop()
    {
        m_isExit = true;
        m_hub.uWS::Group<uWS::CLIENT>::close(0, nullptr, 0);
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

    std::string m_hostUrl;
    std::map<std::string, std::string> m_extraHeaders;

    uWS::Hub m_hub;
    uWS::WebSocket<uWS::CLIENT>* m_ws;
    std::atomic<bool> m_isExit;
};

}
}