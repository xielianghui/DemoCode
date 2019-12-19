#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "lev_callback.h"
#include "lws_callback.h"


class ConvertService
{
public:
    using CContext = typename eddid::event_wrap::Service<LevSrvCbHdl>::CContext;
public:
    ConvertService();
    ~ConvertService();
public:
    int Init();
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
    void SendReq(std::string& msg);

private:
    static void OnSendHeartbeatTimer(evutil_socket_t fd, short event, void* args);
    static void OnQueryInsInfoTimer(evutil_socket_t fd, short event, void* args);
    std::string PackMsg(const std::string& RawMsg);
    std::string UnPackMsg(const std::string& RawMsg);
private:
    // libwebsockets
    std::shared_ptr<LwsCltCbHdl> m_lwsCbPtr;
    std::shared_ptr<LwsClient> m_lwsClientPtr;
    // tcp service
    eddid::event_wrap::EvLoop m_loopHdl;
    std::shared_ptr<LevSrvCbHdl> m_cbPtr;
    std::shared_ptr<eddid::event_wrap::Service<LevSrvCbHdl>> m_levServicePtr;
    // event for send heartbeat
    event* m_heartbeatEv;
    event* m_queryAllInsEv;
    // req/res model
    int m_reqId;
    std::unordered_map<int64_t, CContext*> m_id2ctxMap;//(req_id -> ctx)
    std::unordered_map<int, uint64_t> m_id2originalId;//(internal_req_id -> request_req_id)
    int m_vecPos;
    std::vector<int> m_marketVec;
    // sub/push model
    std::unordered_map<std::string, std::unordered_map<intptr_t, CContext*>> m_type2ctxsMap; //type|market|code -> (ptr_t -> ctx)
};