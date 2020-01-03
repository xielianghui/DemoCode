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
    int InitLevService(std::string& addr, const std::vector<int>& portVec);
    // libwebsockets thread
    void OnLwsRecvMsg(std::string&& msg);
    void OnLwsConnect();
    // libevent thread
    void OnLevAccept(CContext* conn, const char* peer, uint32_t len);
    void OnLevReadDone(CContext* conn, evbuffer*&& recv_data);
    void OnLevDisConnect(CContext* conn);
    void OnLevError(CContext* conn, int err, const char* err_msg);
    void SendReq(std::string& msg);
    void Resub();
    void CheckClientAlive();
private:
    static void OnSendHeartbeatTimer(evutil_socket_t fd, short event, void* args);
    static void OnQueryInsInfoTimer(evutil_socket_t fd, short event, void* args);
    static void OnResubTimer(evutil_socket_t fd, short event, void* args);
    std::string PackMsg(const std::string& RawMsg);
    int ProcessReq(CContext* conn, const std::string& reqStr);
private:
    std::string m_lastMsg;
    // libwebsockets
    std::shared_ptr<LwsCltCbHdl> m_lwsCbPtr;
    std::shared_ptr<LwsClient> m_lwsClientPtr;
    // tcp service
    eddid::event_wrap::EvLoop m_loopHdl;
    std::shared_ptr<LevSrvCbHdl> m_cbPtr;
    std::vector<std::shared_ptr<eddid::event_wrap::Service<LevSrvCbHdl>>> m_levServicePtrVec;
    // event for send heartbeat
    event* m_heartbeatEv;
    event* m_queryAllInsEv;
    event* m_resubEv;
    // req/res model
    int m_reqId;
    std::unordered_map<int64_t, CContext*> m_id2ctxMap;//(req_id -> ctx)
    std::unordered_map<int, uint64_t> m_id2originalId;//(internal_req_id -> request_req_id)
    int m_vecPos;
    std::vector<int> m_marketVec;
    // sub/push model
    std::unordered_map<std::string, std::unordered_map<intptr_t, CContext*>> m_type2ctxsMap; //type|market|code -> (ptr_t -> ctx)
    std::unordered_map<intptr_t, std::vector<std::string>> m_ctx2typeMap; //ctx -> type|market|code
    
    std::unordered_map<CContext*, bool> m_ctx2isLiveMap;//(req_id -> is_receive_heartbeat)

};