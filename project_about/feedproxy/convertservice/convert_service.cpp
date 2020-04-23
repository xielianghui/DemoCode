#include "convert_service.h"
#include "convert_utils.h"
#include "common_hdr.h"

#define HEARTBEAT_FORMATS "{\"reqtype\":1,\"reqid\":1,\"session\":\"\",\"data\":{\"connectionid\":1}}"
#define UN_SUB_FORMATS "{\"reqtype\":201,\"reqid\":2,\"session\":\"\",\"data\":[{\"market\":%s,\"code\":\"%s\",\"type\":%s}]}"
#define QUERY_INS_FORMATS "{\"reqtype\":52,\"reqid\":3,\"session\":\"\",\"data\":{\"marketid\":%d,\"idtype\":1,\"beginpos\":%d,\"count\":1000,\"getquote\":0}}"
//#define Query_QUOTE "{\"reqtype\":153,\"reqid\":%d,\"session\":\"\",\"data\":{\"getsyminfo\":1,\"symbol\":[{\"market\":%d,\"code\":\"%s\"}]}}"

//香港期货=》商品期货：2011；股指期货：2015；货币期货：2009；港股期货：2010
//港股=》主板：2002；创业板：2031；指数：2005；认设证：2003；牛熊证：2004；ETF：2000；other：2001
//美股=》中国概念股：40007；明星股：40001；纽交所：40002；美交所：40003；纳斯达克：40004；ETF：40005；其他：40006

static int64_t heartbeatTimes{0};
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
    for(auto& it : m_levServicePtrVec)
    {
        if(it){
            it->Release();
        }
    }
    m_loopHdl.Release();
}

int ConvertService::Init()
{
    if (m_loopHdl.Initialize() != 0) {
        std::cout<<"loop init failed"<<std::endl;
        return -1;
    }
    // init template
    for(int i = 0; i < 10; ++i)
    {
        ConvertUtils::m_template.add_bids();
        ConvertUtils::m_template.add_asks();
    }
    // init queryMarketJson
    ConvertUtils::queryMarketJson["reqtype"] = 50;
    ConvertUtils::queryMarketJson["reqid"] = 4;
    ConvertUtils::queryMarketJson["session"] = "";
    ConvertUtils::queryMarketJson["data"].resize(0);    
    for(auto& it : m_marketVec)
    {
        Json::Value oneMarket;
        oneMarket["market"] = it;
        ConvertUtils::queryMarketJson["data"].append(oneMarket);
    }
    // init queryQuotesJson
    ConvertUtils::queryQuotesJson["reqtype"] = 153;
    ConvertUtils::queryQuotesJson["session"] = "";
    ConvertUtils::queryQuotesJson["data"]["getsyminfo"] = 1;
    ConvertUtils::queryQuotesJson["data"]["symbol"].resize(0);

}

int ConvertService::InitUwsClient(std::string& addr, int port)
{
    event_base* pEvBase = m_loopHdl.GetWorkLoop();
    m_loopHdl.Increment(pEvBase);
    m_uwsClientPtr = std::make_shared<UwsClient<ConvertService>>(this);
    m_uwsClientPtr->Register(pEvBase, &ConvertService::OnUwsRecvMsg);
    m_uwsClientPtr->Connect(addr, port);
    return 0;
}

int ConvertService::InitLevService(std::string& addr, const std::vector<int>& portVec)
{
    // attach heart timer to loop
    timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = 60;
    event_base* pEvBase = m_loopHdl.GetWorkLoop();
    m_loopHdl.Increment(pEvBase);
    m_heartbeatEv = event_new(pEvBase, -1, EV_PERSIST , OnSendHeartbeatTimer, this);
    if (m_heartbeatEv == nullptr) {
        std::cout<<"event_new() heartbeat timer failed"<<std::endl;
        return -1;
    }
    event_add(m_heartbeatEv, &tv);
    // attach query all instrument timer to loop
    evutil_timerclear(&tv);
    tv.tv_sec = 2;
    m_queryAllInsEv = event_new(pEvBase, -1, EV_READ , OnQueryInsInfoTimer, this);
    if (m_queryAllInsEv == nullptr) {
        std::cout<<"event_new() query ins info timer failed"<<std::endl;
        return -1;
    }
    event_add(m_queryAllInsEv, &tv);

    if (m_loopHdl.Start() != 0) {
        m_loopHdl.Release();
        std::cout<<"loop start failed"<<std::endl;
        return -1;
    }

    // service start listen 
    m_cbPtr = std::make_shared<LevSrvCbHdl>(this);
    for(auto& it : portVec)
    {
        auto levServicePtr = std::make_shared<eddid::event_wrap::Service<LevSrvCbHdl>>(&m_loopHdl, *m_cbPtr);
        levServicePtr->Initialize(addr, it);
        levServicePtr->Start();
        m_levServicePtrVec.emplace_back(levServicePtr);
    }
    return 0;
}

void ConvertService::OnUwsRecvMsg(std::vector<std::string>&& resVec)
{
    for(auto& it : resVec)
    {
        ProcessRes(it);
    }
}

int ConvertService::ProcessRes(std::string& res)
{
    //printf("Uws Recv:%s\n", msg.c_str());
    Json::Value resJson;
    Json::Reader jsonReader;
    if (!jsonReader.parse(res, resJson)){
        std::cout<<"Parse json failed, msg:"<<res<<std::endl;
        return 0;
    }
    int64_t reqId;
    std::vector<std::string> resProtoStrVec;
    int ret = ConvertUtils::JsonRes2ProtoRes(reqId, resJson, resProtoStrVec);
    if(ret < 0){
        std::cout<<"JsonRes2ProtoRes() failed, msg:"<<res<<std::endl;
        return -1;
    }

    if(reqId == 1){// internal heartbeat
        return 0;
    }
    else if(reqId == 2){// internal unsub res
        return 0;
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
            std::cout<<"Get all instrument info, msg len:"<<ConvertUtils::m_queryAllInsResStr.size()<<std::endl;
            std::cout<<"System init OK"<<std::endl;
        }
        return 0;
    }
    else if(reqId == 4){ // query market open time
        return 0;
    }
    else if(reqId == 5){ // resub res 
        //std::cout<<"Recv resub res"<<resJson.toStyledString()<<std::endl;
        return 0;
    }
    else if(reqId == 0){// push res
        std::string key;
        int reqType = resJson["reqtype"].asInt();
        if(reqType == 250){
            Json::Value infoArryJson = resJson["data"]["symbol"];
            if(infoArryJson.empty()) return -1;
            key = "1|" + infoArryJson[0]["market"].asString() + "|" + infoArryJson[0]["code"].asString();
        }
        else if (reqType == 251){
            Json::Value infoArryJson = resJson["data"]["tick"];
            if(infoArryJson.empty()) return -1;
            key = "2|" + infoArryJson[0]["market"].asString() + "|" + infoArryJson[0]["code"].asString();
        }
        else if (reqType == 252){
            Json::Value infoJson = resJson["data"];
            key = "4|" + infoJson["market"].asString() + "|" + infoJson["code"].asString();
        }
        auto mapIt = m_type2ctxsMap.find(key);
        if(mapIt == m_type2ctxsMap.end()){
            //std::cout<<"Can't find connect, key: "<< key <<"msg: "<<resJson.toStyledString()<<std::endl;
            return -1;
        }
        for(auto it = mapIt->second.begin(); it != mapIt->second.end();)
        {
            if(it->second){
                for (auto& resIt : resProtoStrVec)
                {
                    std::string sendMsg = PackMsg(resIt);
                    it->second->Send(sendMsg.data(), sendMsg.size());
                    ++it;
                }
            }
            else{
                mapIt->second.erase(it++);
            }
        }
        
    }
    else{// reply
        auto it = m_id2ctxMap.find(reqId);
        if(it == m_id2ctxMap.end()){
            //std::cout<<"Unknown req id: "<<reqId<<", msg:"<<resJson.toStyledString()<<std::endl;
        }
        else{
            if(it->second){
                for (auto& resIt : resProtoStrVec)
                {
                    std::string sendMsg = PackMsg(resIt);
                    it->second->Send(sendMsg.data(), sendMsg.size());
                }
            }
            m_id2ctxMap.erase(it);
        }
    }
}

void ConvertService::OnLevAccept(CContext* conn, const char* peer, uint32_t len)
{
    m_ctx2isLiveMap[conn] = true;
    std::cout<<"Accept: "<<(intptr_t)conn<<std::endl;
}

void ConvertService::OnLevReadDone(CContext* conn, evbuffer*&& recv_data)
{
    std::string rawReq;
    uint32_t len = evbuffer_get_length(recv_data);
    rawReq.resize(len);
    evbuffer_remove(recv_data, rawReq.data(), len);
    m_lastMsg += rawReq;
    while (true)
    {
        char hdl[eddid::COMMON_HDR_LEN];
        memcpy(hdl, m_lastMsg.data(), eddid::COMMON_HDR_LEN);
        eddid::ST_COMMONHDR *pHdl = (eddid::ST_COMMONHDR *)hdl;
        int dataLen = pHdl->data_len_;
        if (dataLen + eddid::COMMON_HDR_LEN > m_lastMsg.size()){
            break;
        }
        else{
            std::string reqStr(m_lastMsg.data() + eddid::COMMON_HDR_LEN, dataLen);
            int ret = ProcessReq(conn, reqStr);
            if(ret < 0){
                m_lastMsg.clear();
                return;
            }
            else{
                m_lastMsg = m_lastMsg.substr(eddid::COMMON_HDR_LEN + dataLen);
                if (m_lastMsg.size() < eddid::COMMON_HDR_LEN) break;
            }
        }
    }
}

int ConvertService::ProcessReq(CContext* conn, const std::string& reqStr)
{
    // convert msg
    ++m_reqId;
    m_reqId = (m_reqId < 0) ? REQ_ID_START : m_reqId;// bigger than int::max
    Json::Value reqJson;
    int64_t ret = ConvertUtils::ProtoReq2JsonReq(m_reqId, reqStr, reqJson);
    if(ret < 0){
        std::cout<<"ProtoReq2JsonReq() failed, msg: "<<reqStr<<std::endl;
        return -1;
    }
    else{

        if(reqJson["reqtype"].asInt() != 1){
            std::cout<<"\033[1;31;40mLev Recv:\033[0m "<<reqJson.toStyledString()<<std::endl;
        }
    }
    bool specilaAdd = false;

    int reqType = reqJson["reqtype"].asInt();
    // special for some req type
    if(reqType == 1){// heartbeat
        m_ctx2isLiveMap[conn] = true;
        return 0;
    }
    else if(reqType == 50){// query trade date
        eddid::MsgCarrier mc;
        mc.set_req_id(ret);
        mc.set_msg_type(eddid::TypeDef_MsgType::TypeDef_MsgType_RESP);
        mc.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_TRADE_DATE);
        mc.set_data(ConvertUtils::m_tradeDateProto.SerializeAsString());
        std::string resProtoStr = mc.SerializeAsString();
        std::string sendMsg = PackMsg(resProtoStr);
        conn->Send(sendMsg.data(), sendMsg.size());
        return 0;
    }
    else if(reqType == 52){// query all ins info
        eddid::MsgCarrier mc;
        mc.set_req_id(ret);
        mc.set_msg_type(eddid::TypeDef_MsgType::TypeDef_MsgType_RESP);
        mc.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_ALL_INS_INFO);
        if(ConvertUtils::m_queryAllInsResStr.empty()){
            eddid::QueryAllInsInfoResp qtyAllInsInfoResp;
            qtyAllInsInfoResp.set_error_code(-1);
            qtyAllInsInfoResp.set_error_msg("Server has not query all instrument info");
            mc.set_data(qtyAllInsInfoResp.SerializeAsString());
        }
        else{
            mc.set_data(ConvertUtils::m_queryAllInsResStr);
        }
        std::string resProtoStr = mc.SerializeAsString();
        std::string sendMsg = PackMsg(resProtoStr);
        conn->Send(sendMsg.data(), sendMsg.size());
        return 0;
    }
    else if(reqType == 200 || reqType == 201){// sub or unsub
        Json::Value realArray;
        realArray.resize(0);
        int realCounts = 0;
        for(int i = 0; i < reqJson["data"].size(); ++i)
        {
            std::string type = reqJson["data"][i]["type"].asString();
            std::string key = type + "|" + reqJson["data"][i]["market"].asString() + "|" + reqJson["data"][i]["code"].asString();
            if(reqType == 200){
                if(type == "1"){
                    Json::Value oneCode;
                    oneCode["market"] = reqJson["data"][i]["market"].asInt();
                    oneCode["code"] = reqJson["data"][i]["code"].asString();
                    ConvertUtils::queryQuotesJson["data"]["symbol"].append(oneCode);
                }
                
                auto typeIt = m_type2ctxsMap.find(key);
                if (typeIt == m_type2ctxsMap.end()){
                    typeIt = m_type2ctxsMap.insert(std::make_pair(key, std::unordered_map<intptr_t, CContext *>())).first;
                    realArray[realCounts++] = reqJson["data"][i];
                }
                typeIt->second.insert(std::make_pair((intptr_t)conn, conn));

                auto ctxIt = m_ctx2typeMap.find((intptr_t)conn);
                if (ctxIt == m_ctx2typeMap.end()){
                    ctxIt = m_ctx2typeMap.insert(std::make_pair((intptr_t)conn, std::vector<std::string>())).first;
                }
                ctxIt->second.emplace_back(key);

            }
            else{
                Json::Value del;
                auto typeIt = m_type2ctxsMap.find(key);
                if (typeIt != m_type2ctxsMap.end()){
                    auto it = typeIt->second.find((intptr_t)conn);
                    if(it != typeIt->second.end()){
                        typeIt->second.erase(it);
                        if(typeIt->second.empty()){
                            m_type2ctxsMap.erase(typeIt);
                            realArray[realCounts++] = reqJson["data"][i];
                        }
                    }
                }
            } 
        }
        reqJson["data"] = realArray;

        if(!ConvertUtils::queryQuotesJson["data"]["symbol"].empty()){
            specilaAdd = true;
            ConvertUtils::queryQuotesJson["reqid"] = m_reqId + 1;
            // before sub, query 153
            std::string queryQuoteStr = ConvertUtils::queryQuotesJson.toStyledString();
            m_uwsClientPtr->SendReq(queryQuoteStr);
            m_id2ctxMap[m_reqId + 1] = conn;
            ConvertUtils::queryQuotesJson["data"]["symbol"].resize(0);
        }
        //std::cout<<"\033[1;31;40mReal Send:\033[0m"<<reqJson.toStyledString()<<std::endl;
    }

    // send req to websockets
    m_uwsClientPtr->SendReq(reqJson.toStyledString());
    m_id2ctxMap[m_reqId] = conn;
    m_id2originalId[m_reqId] = ret; // ret is request true req id
    if(specilaAdd) ++m_reqId;
    return 0;
}

void ConvertService::OnLevDisConnect(CContext* conn)
{
    std::cout<<"OnLevDisConnect(), con: "<<(intptr_t)conn<<std::endl;
    intptr_t iCon = (intptr_t)conn;
    auto ctxIt = m_ctx2typeMap.find(iCon);
    if(ctxIt != m_ctx2typeMap.end()){
        auto& keyVec = ctxIt->second;
        for(auto& it : keyVec)
        {
            auto typeIt = m_type2ctxsMap.find(it);
            if(typeIt != m_type2ctxsMap.end()){
                typeIt->second.erase(iCon);
                if(typeIt->second.empty()){
                    // unsub
                    std::vector<std::string> keyVec;
                    int start = 0;
                    for (int pos = 0; pos < it.size(); ++pos)
                    {
                        if (it[pos] == '|'){
                            keyVec.emplace_back(it.substr(start, pos - start));
                            start = pos + 1;
                        }
                    }
                    keyVec.emplace_back(it.substr(start));
                    if (keyVec.size() >= 3){
                        char unsubMsg[1024];
                        snprintf(unsubMsg, 1024, UN_SUB_FORMATS, keyVec[1].data(), keyVec[2].data(), keyVec[0].data());
                        m_uwsClientPtr->SendReq(unsubMsg);
                        std::cout << "Send unsub msg: " << unsubMsg << std::endl;
                    }
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
    m_uwsClientPtr->SendReq(msg);
}

void ConvertService::OnSendHeartbeatTimer(evutil_socket_t fd, short event, void* args)
{
    ConvertService* cs = (ConvertService*) args;
    // client connect alive check
    cs->CheckClientAlive();
    // reqid 1 is heartbeat request
    std::string heartbeatJson = HEARTBEAT_FORMATS;
    cs->SendReq(heartbeatJson);
    std::cout<<"WebScokets heartbeat send, times: "<<heartbeatTimes++<<std::endl;
}

void ConvertService::CheckClientAlive()
{
    for(auto it = m_ctx2isLiveMap.begin(); it != m_ctx2isLiveMap.end();)
    {
        if(it->second){
            it->second = false;
            ++it;
            continue;
        }
        else{
            std::cout<<"Connection: "<<(intptr_t)it->first<<" not alive, close connection"<<std::endl;
            OnLevDisConnect(it->first);
            m_id2ctxMap.erase((intptr_t)it->first);
            it->first->Close();
            m_ctx2isLiveMap.erase(it++);
        }
    }
}

void ConvertService::OnQueryInsInfoTimer(evutil_socket_t fd, short event, void* args)
{
    ConvertService* cs = (ConvertService*) args;
    std::string queryOpenTime = ConvertUtils::queryMarketJson.toStyledString();
    cs->SendReq(queryOpenTime);

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