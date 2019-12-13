#pragma once
#include <memory>
#include <vector>
#include <unordered_map>

#include "lev_callback.h"
#include "lws_callback.h"
#include "convert_utils.h"

class ConvertService
{
public:
    using CContext = typename eddid::event_wrap::Service<LevSrvCbHdl>::CContext;
public:
    ConvertService();
    ~ConvertService();
public:
    int InitLwsClient(std::string& addr, int port);
    int InitLevService(std::string& addr, int port);
    // libwebsockets thread
    void OnLwsRecvMsg(std::string&& msg);
    void OnLwsConnect();
    // libevent thread
    void OnLevAccept(CContext* conn, const char* peer, uint32_t len);
    void OnLevReadDone(CContext* conn, evbuffer*&& recv_data);
    void OnLevDisConnect(CContext* conn);
    void OnLevError(CContext* conn, int err, const char* err_msg);
    void SendHeartbeat(std::string& msg);

private:
    static void OnSendHeartbeatTimer(evutil_socket_t fd, short event, void* args);

private:
    // libwebsockets
    std::shared_ptr<LwsCltCbHdl> m_lwsCbPtr;
    std::shared_ptr<LwsClient> m_lwsClientPtr;
    // tcp service
    eddid::event_wrap::EvLoop m_loopHdl;
    std::shared_ptr<LevSrvCbHdl> m_CbPtr;
    std::shared_ptr<eddid::event_wrap::Service<LevSrvCbHdl>> m_levServicePtr;
    // event for send heartbeat
    event* m_heartbeatEv;
    // req/res model
    int64_t m_reqId;
    std::unordered_map<int64_t, CContext*> m_id2ctxMap;
    // sub/push model
    std::unordered_map<std::string, std::unordered_map<intptr_t, CContext*>> m_type2ctxsMap; //type|market|code -> (ptr_t -> ctx)
};