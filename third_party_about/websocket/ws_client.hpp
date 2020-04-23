#pragma once

#include "ws_proto.hpp"
#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"
#include "../network/net/client.h"

namespace eddid{
namespace wsutils{

const char* httpFormat = "GET /%s HTTP/1.1\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                         "Host: %s\r\n"
                         "Sec-WebSocket-Version: 13\r\n";

static uint32_t CheckHandShake(char* data, uint32_t size)
{
    std::string dataStr(data, size);
    auto it = dataStr.find("\r\n\r\n");
    if(it == std::string::npos) return 0;
    std::string httpHead = dataStr.substr(0, it + 4); // 4 is length of "\r\n\r\n"
    // HTTP/1.1 101 Switching Protocols
    // Upgrade: websocket
    // Connection: Upgrade
    // Sec-WebSocket-Accept: HSmrc0sMlYUkAGmm5OPpG2HaGWk=
    // Sec-WebSocket-Version: 13
    // WebSocket-Server: uWebSockets
    //
    //
    uint32_t start = 0;
    std::vector<std::string> headVec;
    for (uint32_t pos = 0; pos < httpHead.size(); ++pos)
    {
        if (httpHead[pos] == '\n'){
            headVec.emplace_back(httpHead.substr(start, pos - start - 1));
            start = pos + 1;
        }
    }
    if(headVec.empty()) return 0;
    if(headVec[0].find("HTTP") == std::string::npos || headVec[0].find("101") == std::string::npos) return 0;
    std::unordered_map<std::string, std::string> headMap;
    for(int i = 1; i < headVec.size(); ++i)
    {
        headMap[headVec[i].substr(0, headVec[i].find(":"))] = 
            headVec[i].substr(headVec[i].find(":") + 1);
    }
    // Connection field check
    auto mapIt = headMap.find("Connection");
    if(mapIt == headMap.end()) return 0;
    if(mapIt->second.find("Upgrade") == std::string::npos) return 0;
    // Upgrade field check
    mapIt = headMap.find("Upgrade");
    if(mapIt == headMap.end()) return 0;
    if(mapIt->second.find("websocket") == std::string::npos) return 0;
    
    return httpHead.size();
}

class CWsClient 
{
    using OnConnection = eddid_function_wrap<void()>;
    using OnDisconnection = eddid_function_wrap<void()>;
    using OnMessage = eddid_function_wrap<void(char *, size_t , EN_OP_CODE)>;
    using OnErr = eddid_function_wrap<void(int, const char *)>;

    EDDID_DISALLOW_COPY(CWsClient)
public:
    CWsClient(eddid::event_wrap::CEvLoop* evLoop):
        m_tcpClientPtr(nullptr),
        m_evLoop(evLoop),
        m_isWsConnected(false){}
    
    ~CWsClient()
    {
        m_tcpClientPtr->Release();
    }

public:

    /**
     * \brief 初始化连接信息
     * \param url 服务器的url路径,例如:
     * ws://127.0.0.1:2222/ws/API
     * wss://127.0.0.1:2222/ws/API (https)
     * \param extraHeaders http握手时增加的头
     * \param reconnectTimeS 自动重连时间
     * \return 0表示成功,-1表示失败
     */
    int Init(const std::string& uri, const std::map<std::string, std::string>& extraHeaders = {}, uint32_t reconnectTimeS = 3)
    {
        m_uri = uri;
        // url check
        if (!ParseURI(m_uri, m_isHttps, m_hostname, m_port, m_path)){
            std::cerr << "Parse uri failed, please check" <<std::endl;
            return -1;
        }
        // tcp client init
        m_tcpClientPtr = std::make_shared<eddid::event_wrap::Client<CWsClient>>(m_evLoop, *this);
        m_tcpClientPtr->Initialize(m_hostname, m_port, reconnectTimeS);
        // ws proto init
        m_wsProto.RegisterCallback(EDDID_BIND_MEM_FUNC(*this, OnData));
        // init http head
        m_httpHead.clear();
        char cHttp[2048]{0};
        std::string host = (m_hostname + ":" + std::to_string(m_port));
        snprintf(cHttp, 2048, httpFormat, m_path.c_str(), host.c_str());
        m_httpHead += std::string(cHttp);
        for(auto& it : extraHeaders)
        {
            m_httpHead += (it.first + ": " + it.second + "\r\n");
        }
        m_httpHead += "\r\n";
        return 0;
    }

    /**
     * \brief 发送消息(线程安全)
     * \param data 数据指针
     * \param size 数据长度
     * \param opCode 发送数据的类型,具体含义可以看websocket头协议
     * \return 0表示成功,-1表示失败
     */
    int SendMsg(std::shared_ptr<char>&& data, size_t len, EN_OP_CODE opCode = EN_OP_CODE::BINARY)
    {
        if(m_tcpClientPtr == nullptr || !m_isWsConnected) return -1;
        auto msgVec = m_wsProto.PackMsg<false>(opCode, std::move(data), len);
        for(auto& it : msgVec)
        {
            m_tcpClientPtr->Send(it.first.get(), it.second);
        }
        return 0;
    }

    void Run()
    {
        m_tcpClientPtr->Start();
    }

    void Stop()
    {
        m_tcpClientPtr->Stop();
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

    void setErrorCb(const OnErr& cb)
    {
        m_errCb = cb;
    }

public:
    // tcp callback
    void OnConnected()
    {
        // tcp connected, send http head
        m_tcpClientPtr->Send(m_httpHead.data(), m_httpHead.size());
    }
    void OnDisConnect()
    {
        m_isWsConnected = false;
        if(m_disconCb){
            m_disconCb();
        }
    }
    void OnError(int errorNo, const char * errorMsg)
    {
        m_isWsConnected = false;
        if(m_errCb){
            m_errCb(errorNo, errorMsg);
        }
    }
    void OnReadDone(evbuffer *&& data)
    {
        uint32_t size = evbuffer_get_length(data);
        std::shared_ptr<char> rawMsg(new char[size]{0}, std::default_delete<char[]>());
        evbuffer_remove(data, rawMsg.get(), size);
        if(!m_isWsConnected){
            // check is http hand shake
            uint32_t httpHeadSize{0};
            if((httpHeadSize = CheckHandShake(rawMsg.get(), size))){
                m_isWsConnected = true;
                // callback
                if(m_conCb){
                    m_conCb();
                }
            }
            else{
                std::cerr << "Error http hand shake" <<std::endl;
                return;
            }
            uint32_t realSize = (size - httpHeadSize);
            if(realSize == 0) return;
            std::shared_ptr<char> realData(new char[size - httpHeadSize]{0}, std::default_delete<char[]>());
            memcpy(realData.get(), rawMsg.get() + httpHeadSize, realSize);
            rawMsg.swap(realData);
        }
        m_wsProto.ParseMsg(std::move(rawMsg), size);
    }
    void OnData(std::shared_ptr<char>&& data, uint32_t len, EN_OP_CODE opCode)
    {
        if(m_msgCb){
            m_msgCb(data.get(), len, opCode);
        }
    }

public:
    OnConnection m_conCb;
    OnDisconnection m_disconCb;
    OnMessage m_msgCb;
    OnErr m_errCb;

    std::string m_uri;
    int m_port;
    bool m_isHttps;
    std::string m_hostname;
    std::string m_path;

    std::string m_httpHead;

    std::shared_ptr<eddid::event_wrap::Client<CWsClient>> m_tcpClientPtr;
    eddid::event_wrap::CEvLoop* m_evLoop;

    CWebSocketProto m_wsProto;
    std::atomic<bool> m_isWsConnected;
};

}
}
