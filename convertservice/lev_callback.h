#pragma once

#include "levservice/service.h"

class ConvertService;
class LevSrvCbHdl final
{
public:
    using CContext = typename eddid::event_wrap::Service<LevSrvCbHdl>::CContext;
public:
    LevSrvCbHdl() = delete;
    ~LevSrvCbHdl() = default;
    LevSrvCbHdl(ConvertService* mainService);
public:
    void OnAccept(CContext* conn, const char* peer, uint32_t len);       // 接受连接回调
    void OnReadDone(CContext* conn, evbuffer*&& recv_data);              // 读完成回调
    void OnDisConnect(CContext* conn);                                   // 断线回调
    void OnError(CContext* conn, int err, const char* err_msg);          // 错误消息回调
private:
    ConvertService* m_mainService;
};
