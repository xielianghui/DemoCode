#include "convert_service.h"
#include "convert_utils.h"
#include "common_hdr.h"

#define HEARTBEAT_FORMATS "{\"reqtype\":1,\"reqid\":1,\"session\":\"\",\"data\":{\"connectionid\":1}}"
#define UN_SUB_FORMATS "{\"reqtype\":201,\"reqid\":2,\"session\":\"\",\"data\":[{\"market\":%s,\"code\":\"%s\",\"type\":%s}]}"
#define QUERY_INS_FORMATS "{\"reqtype\":52,\"reqid\":3,\"session\":\"\",\"data\":{\"marketid\":%d,\"idtype\":1,\"beginpos\":%d,\"count\":1000,\"getquote\":0}}"
#define Query_QUOTE "{\"reqtype\":153,\"reqid\":%d,\"session\":\"\",\"data\":{\"getsyminfo\":0,\"symbol\":[{\"market\":%d,\"code\":\"%s\"}]}}"

//香港期货=》商品期货：2011；股指期货：2015；货币期货：2009；香港期货：2010
//港股=》主板：2002；创业板：2031；指数：2005；认设证：2003；牛熊证：2004；ETF：2000；other：2001
//美股=》中国概念股：40007；明星股：40001；纽交所：40002；美交所：40003；纳斯达克：40004；ETF：40005；其他：40006
ConvertService::ConvertService():
    m_heartbeatEv(nullptr),
    m_reqId(REQ_ID_START),
    m_vecPos(0),
    m_marketVec({2011, 2015, 2009, 2010, 2002, 2031, 2005, 2003, 2004, 2000, 2001, 40007, 40001, 40002, 40003, 40004, 40005, 40006,
                50000, 50001, 50002, 50003, 50004, 50005, 50101, 50102, 50103, 50104, 50105, 50106, 50107, 50108, 50109, 50110,
                51201, 51202, 51203, 51204, 51205, 51206, 52301, 52302, 52303, 52304, 53401, 53402, 53403, 53404, 54501, 54502, 
                54503, 54504, 54505, 54506, 54507, 54508, 54509, 54510, 55601, 55602, 55603})
{

}

ConvertService::~ConvertService()
{
    if(m_heartbeatEv){
        event_free(m_heartbeatEv);
    }
    if(m_queryAllInsEv){
        event_free(m_queryAllInsEv);
    }
    m_levServicePtr->Release();
    m_loopHdl.Release();
}

int ConvertService::Init()
{
    if (m_loopHdl.Initialize() != 0) {
        puts("loop init failed\n");
        return -1;
    }
    // init template
    for(int i = 0; i < 10; ++i)
    {
        ConvertUtils::m_template.add_bids();
        ConvertUtils::m_template.add_offers();
    }
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
    // attach query all instrument timer to loop
    evutil_timerclear(&tv);
    tv.tv_sec = 2;
    m_queryAllInsEv = event_new(pEvBase, -1, EV_READ , OnQueryInsInfoTimer, this);
    if (m_queryAllInsEv == nullptr) {
        puts("event_new() query ins info timer failed\n");
        return -1;
    }
    event_add(m_queryAllInsEv, &tv);

    if (m_loopHdl.Start() != 0) {
        m_loopHdl.Release();
        puts("loop start failed\n");
        return -1;
    }
    // service start listen 
    m_cbPtr = std::make_shared<LevSrvCbHdl>(this);
    m_levServicePtr = std::make_shared<eddid::event_wrap::Service<LevSrvCbHdl>>(&m_loopHdl, *m_cbPtr);
    m_levServicePtr->Initialize(addr, port);
    m_levServicePtr->Start();
    return 0;
}

void ConvertService::OnLwsRecvMsg(std::string&& msg)
{
    //printf("Lws Recv:%s\n", msg.c_str());
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

    if(reqId == 1){// internal heartbeat
        return;
    }
    else if(reqId == 2){// internal unsub res
        return;
    }
    else if(reqId == 3){// internal query instrument list res
        if(m_vecPos < m_marketVec.size()){
            char queryInsMsg[1024];
            int beginpos = resJson["data"]["beginpos"].asInt();
            int totalCount = resJson["data"]["totalcount"].asInt();
            if(beginpos + 1000 >= totalCount){
                // next market
                ++m_vecPos;
                snprintf(queryInsMsg, 1024, QUERY_INS_FORMATS, m_marketVec[m_vecPos], 0);
            }
            else{
                snprintf(queryInsMsg, 1024, QUERY_INS_FORMATS, m_marketVec[m_vecPos], beginpos + 1000);
            }
            std::string queryInsStr(queryInsMsg);
            SendReq(queryInsStr);
        }
        else{
            // has query all ins list
            ConvertUtils::m_queryAllInsResStr = ConvertUtils::m_allInsInfoResProto.SerializeAsString();
            printf("Get all ins info, msg len:%d\n", ConvertUtils::m_queryAllInsResStr.size());
        }
        return;
    }
    else if(reqId == 0){// push res
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
                std::string sendMsg = PackMsg(resProtoStr);
                it->second->Send(sendMsg.data(), sendMsg.size());
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
                std::string sendMsg = PackMsg(resProtoStr);
                it->second->Send(sendMsg.data(), sendMsg.size());
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
    std::string rawReq;
    uint32_t len = evbuffer_get_length(recv_data);
    rawReq.resize(len);
    evbuffer_remove(recv_data, rawReq.data(),len );
    printf("Lev Recv:%s\n", rawReq.c_str());
    std::string reqStr = UnPackMsg(rawReq);
    // convert msg
    ++m_reqId;
    m_reqId = (m_reqId < 0) ? REQ_ID_START : m_reqId;// bigger than int::max
    Json::Value reqJson;
    int64_t ret = ConvertUtils::ProtoReq2JsonReq(m_reqId, reqStr, reqJson);
    if(ret < 0){
        printf("ProtoReq2JsonReq() failed, msg: %s\n", reqStr.c_str());
        return;
    }
    bool specilaAdd = false;

    int reqType = reqJson["reqtype"].asInt();
    // special for some req type
    if(reqType == 52){// query all ins info
        ed::MsgCarrier mc;
        mc.set_req_id(ret);
        mc.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_RESP);
        mc.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_ALL_INS_INFO);
        if(ConvertUtils::m_queryAllInsResStr.empty()){
            ed::QueryAllInsInfoResp qtyAllInsInfoResp;
            qtyAllInsInfoResp.set_error_code(-1);
            qtyAllInsInfoResp.set_error_msg("Server has not query all instrument info");
            mc.set_data(qtyAllInsInfoResp.SerializeAsString());
        }
        else{
            mc.set_data(ConvertUtils::m_queryAllInsResStr);
        }
        std::string resProtoStr = mc.SerializeAsString();
        conn->Send(resProtoStr.data(), resProtoStr.size());
        return;
    }
    else if(reqType == 200){// sub
        specilaAdd = true;
        // before sub, query 153
        char queryQuoteMsg[1024];
        snprintf(queryQuoteMsg, 1024, Query_QUOTE, m_reqId + 1, reqJson["data"][0]["market"].asInt(), reqJson["data"][0]["code"].asString().c_str());
        std::string queryQuoteStr(queryQuoteMsg);
        m_lwsClientPtr->SendReq(queryQuoteStr);
        m_id2ctxMap[m_reqId + 1] = conn;
    } 
    // send req to websockets
    m_lwsClientPtr->SendReq(reqJson.toStyledString());
    m_id2ctxMap[m_reqId] = conn;
    m_id2originalId[m_reqId] = ret; // ret is request true req id
    if(specilaAdd) ++m_reqId;

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
    printf("OnLevDisConnect(), con:%d\n", (intptr_t)conn);
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
                    printf("Send unsub msg:%s\n", unsubMsg);
                }
            }
        }
    }
}

void ConvertService::OnLevError(CContext* conn, int err, const char* err_msg)
{
    //printf("OnLevError(), conn:%d\n", (intptr_t)conn);
}

void ConvertService::SendReq(std::string& msg)
{
    m_lwsClientPtr->SendReq(msg);
}

void ConvertService::OnSendHeartbeatTimer(evutil_socket_t fd, short event, void* args)
{
    ConvertService* cs = (ConvertService*) args;
    // reqid 1 is heartbeat request
    std::string heartbeatJson = HEARTBEAT_FORMATS;
    cs->SendReq(heartbeatJson);
}

void ConvertService::OnQueryInsInfoTimer(evutil_socket_t fd, short event, void* args)
{
    ConvertService* cs = (ConvertService*) args;
    // reqid 1 is heartbeat request
    char queryInsMsg[1024];
    snprintf(queryInsMsg, 1024, QUERY_INS_FORMATS, 2011, 0);
    std::string queryInsStr(queryInsMsg);
    cs->SendReq(queryInsStr);
}

std::string ConvertService::PackMsg(const std::string &RawMsg)
{
    std::string data;
    data.resize(RawMsg.size() + eddid::COMMON_HDR_LEN);
    eddid::ST_COMMONHDR* pHdr = (eddid::ST_COMMONHDR*)data.data();
    new (pHdr) eddid::ST_COMMONHDR(0, RawMsg.size());
    memcpy(data.data() + eddid::COMMON_HDR_LEN, RawMsg.data(), RawMsg.size());
    return data;
}

std::string ConvertService::UnPackMsg(const std::string &RawMsg)
{
    char hdl[eddid::COMMON_HDR_LEN];
    memcpy(hdl, RawMsg.data(), eddid::COMMON_HDR_LEN);
    eddid::ST_COMMONHDR *pHdl = (eddid::ST_COMMONHDR *)hdl;
    return std::string(RawMsg.data() + eddid::COMMON_HDR_LEN,  pHdl->data_len_);
}