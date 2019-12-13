#include "convert_service.h"

#define UN_SUB_FORMATS "{\"reqtype\":201,\"reqid\":1,\"session\":\"\",\"data\":[{\"market\":%s,\"code\":\"%s\",\"type\":%s}]}"
#define HEARTBEAT_FORMATS "{\"reqtype\":1,\"reqid\":1,\"session\":\"\",\"data\":{\"connectionid\":1}}"
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
    Json::Value resJson;
    Json::Reader jsonReader;
    if (!jsonReader.parse(msg, resJson)){
        printf("Parse json failed, msg: %s\n", msg.c_str());
        return;
    }
    int64_t reqId;
    std::string resProtoStr;
    int ret = ConvertUtils::JsonRes2ProtoRes(reqId, resJson, resProtoStr);
    if(ret < 0){
        printf("JsonRes2ProtoRes() failed, msg: %s", msg.c_str());
        return;
    }

    if(reqId == 1){// heartbeat 
        return;
    }
    else if(reqId == 0){// push
        std::string key;
        int reqType = resJson["reqtype"].asInt();
        if(reqType == 250){
            Json::Value infoArryJson = resJson["data"]["symbol"];
            if(infoArryJson.empty()) return;
            key = "1|" + infoArryJson[0]["market"].asString() + "|" + infoArryJson[0]["code"].asString();
        }
        else if (reqType == 251){
            Json::Value infoArryJson = resJson["data"]["tick"];
            if(infoArryJson.empty()) return;
            key = "2|" + infoArryJson[0]["market"].asString() + "|" + infoArryJson[0]["code"].asString();
        }
        else if (reqType == 252){
            Json::Value infoJson = resJson["data"];
            key = "4|" + infoJson["market"].asString() + "|" + infoJson["code"].asString();
        }
        auto mapIt = m_type2ctxsMap.find(key);
        if(mapIt == m_type2ctxsMap.end()){
            printf("Can't find connect, key: %s", key.c_str());
            return;
        }

        for(auto it = mapIt->second.begin(); it != mapIt->second.end();)
        {
            if(it->second){
                it->second->Send(resProtoStr.data(), resProtoStr.size());
                ++it;
            }
            else{
                mapIt->second.erase(it++);
            }
        }
        
    }
    else{// reply
        auto it = m_id2ctxMap.find(reqId);
        if(it == m_id2ctxMap.end()){
            printf("Unknown req id: %ld\n", reqId);
        }
        else{
            if(it->second){
                it->second->Send(resProtoStr.data(), resProtoStr.size());
            }
            m_id2ctxMap.erase(it);
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
    // recv string request msg
    std::string reqStr;
    uint32_t len = evbuffer_get_length(recv_data);
    reqStr.resize(len);
    evbuffer_remove(recv_data, reqStr.data(),len );
    printf("Lev Recv:%s\n", reqStr.c_str());
    // convert msg
    Json::Value reqJson;
    int ret = ConvertUtils::ProtoReq2JsonReq(++m_reqId, reqStr, reqJson);
    if(ret < 0){
        printf("ProtoReq2JsonReq() failed, msg: %s", reqStr.c_str());
        return;
    }
    m_lwsClientPtr->SendReq(reqJson.toStyledString());
    m_id2ctxMap[m_reqId] = conn;

    int reqType = reqJson["reqtype"].asInt();
    // sub or unsub
    if(reqType == 200 || reqType == 201){
        for(auto& it : reqJson["data"])
        {
            std::string type = it["type"].asString();
            type = (type == "3" ? "1" : type);
            std::string key = type + "|" + it["market"].asString() + "|" + it["code"].asString();
            auto mapIt = m_type2ctxsMap.find(key);
            if(reqType == 200){
                if(mapIt == m_type2ctxsMap.end()){
                    mapIt = m_type2ctxsMap.insert(std::make_pair(key, std::unordered_map<intptr_t, CContext*>())).first;
                }
                mapIt->second.insert(std::make_pair((intptr_t)conn, conn));
            }
            else{
                if(mapIt != m_type2ctxsMap.end()){
                    mapIt->second.erase((intptr_t)conn);
                }
            }
        }
    }
}

void ConvertService::OnLevDisConnect(CContext* conn)
{
    
}

void ConvertService::OnLevError(CContext* conn, int err, const char* err_msg)
{
    intptr_t iCon = (intptr_t)conn;
    for(auto& it : m_type2ctxsMap)
    {
        // key : type|market|code
        std::string key = it.first;
        auto mapIt = it.second.find(iCon);
        if(mapIt != it.second.end()){
            it.second.erase(mapIt);
            if(it.second.empty()){
                // unsub
                std::vector<std::string> keyVec;
                int start = 0;
                for(int pos = 0; pos < key.size(); ++pos)
                {
                    if(key[pos] == '|'){
                        keyVec.emplace_back(key.substr(start, pos - start));
                        start = pos + 1;
                    }
                }
                keyVec.emplace_back(key.substr(start));
                if(keyVec.size() >= 3){
                    char unsubMsg[1024];
                    snprintf(unsubMsg, 1024, UN_SUB_FORMATS, keyVec[1].data(), keyVec[2].data(), keyVec[0].data());
                    m_lwsClientPtr->SendReq(unsubMsg);
                }
            }
        }
    }
}

void ConvertService::SendHeartbeat(std::string& msg)
{
    m_lwsClientPtr->SendReq(msg);
}

void ConvertService::OnSendHeartbeatTimer(evutil_socket_t fd, short event, void* args)
{
    ConvertService* cs = (ConvertService*) args;
    // reqid 1 is heartbeat request
    std::string heartbeatJson = HEARTBEAT_FORMATS;
    cs->SendHeartbeat(heartbeatJson);
}