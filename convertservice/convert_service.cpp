#include "convert_service.h"

ConvertService::ConvertService():
    m_heartbeatEv(nullptr),
    m_reqId(1)
{
    if (m_loopHdl.Initialize() != 0) {
        puts("loop init failed\n");
    }
}

ConvertService::~ConvertService()
{
    if(m_heartbeatEv){
        event_free(m_heartbeatEv);
    }
    m_levServicePtr->Release();
    m_loopHdl.Release();
}

int ConvertService::InitLwsClient(std::string& addr, int port)
{
    event_base* pEvBase = m_loopHdl.GetWorkLoop();
    m_loopHdl.Increment(pEvBase);
    m_lwsClientPtr = std::make_shared<LwsClient>(pEvBase);
    m_lwsCbPtr = std::make_shared<LwsCltCbHdl>(this);
    m_lwsClientPtr->Register(m_lwsCbPtr.get());
    m_lwsClientPtr->Connect(addr, port);
    return 0;
}

int ConvertService::InitLevService(std::string& addr, int port)
{
    m_CbPtr = std::make_shared<LevSrvCbHdl>(this);
    // attach heart timer to loop
    timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = 30;
    event_base* pEvBase = m_loopHdl.GetWorkLoop();
    m_loopHdl.Increment(pEvBase);
    m_heartbeatEv = event_new(pEvBase, -1, EV_PERSIST , OnSendHeartbeatTimer, this);
    if (m_heartbeatEv == nullptr) {
        puts("event_new() heartbeat timer failed\n");
        return -1;
    }
    event_add(m_heartbeatEv, &tv);
    if (m_loopHdl.Start() != 0) {
        m_loopHdl.Release();
        puts("loop start failed\n");
        return -1;
    }
    m_levServicePtr = std::make_shared<eddid::event_wrap::Service<LevSrvCbHdl>>(&m_loopHdl, *m_CbPtr);
    m_levServicePtr->Initialize(addr, port);
    m_levServicePtr->Start();
    return 0;
}

void ConvertService::OnLwsRecvMsg(std::string&& msg)
{
    printf("Lws Recv:%s\n", msg.c_str());
    int64_t reqId;
    std::string protoRes;
    ConvertUtils::JsonRes2ProtoRes(reqId, msg, protoRes);
    if(reqId == 1){
        // heartbeat res
        return;
    }
    auto it = m_id2ctxMap.find(reqId);
    if(it == m_id2ctxMap.end()){
        printf("Unknown req id: %ld\n", reqId);
    }
    else{
        if(!it->second){
            m_id2ctxMap.erase(it);
        }
        else{
            it->second->Send(protoRes.data(), protoRes.size());
        }
    }
}

void ConvertService::OnLwsConnect()
{
    
}

void ConvertService::OnLevAccept(CContext* conn, const char* peer, uint32_t len)
{
    
}

void ConvertService::OnLevReadDone(CContext* conn, evbuffer*&& recv_data)
{
    std::string protoReq;
    uint32_t len = evbuffer_get_length(recv_data);
    protoReq.resize(len);
    evbuffer_remove(recv_data, protoReq.data(),len );
    std::string jsonReq;
    jsonReq.resize(len);
    printf("Lev Recv:%s\n", protoReq.c_str());
    ConvertUtils::ProtoReq2JsonReq(++m_reqId, protoReq, jsonReq);
    m_lwsClientPtr->SendReq(jsonReq);
    m_id2ctxMap[m_reqId] = conn;
}

void ConvertService::OnLevDisConnect(CContext* conn)
{
    
}

void ConvertService::OnLevError(CContext* conn, int err, const char* err_msg)
{
    
}

void ConvertService::SendHeartbeat(std::string& msg)
{
    m_lwsClientPtr->SendReq(msg);
}

void ConvertService::OnSendHeartbeatTimer(evutil_socket_t fd, short event, void* args)
{
    ConvertService* cs = (ConvertService*) args;
    // reqid 1 is heartbeat request
    std::string heartbeatJson = "{\"reqtype\":1,\"reqid\":1,\"session\":\"\",\"data\":{\"connectionid\":1}}";
    cs->SendHeartbeat(heartbeatJson);
}