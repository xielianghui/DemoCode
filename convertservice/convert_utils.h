#pragma once
#include <vector>
#include <iconv.h>

#include "json/json.h"

#include "typedef.pb.h"
#include "msg_carrier.pb.h"
#include "query.pb.h"
#include "push.pb.h"

#define REQ_ID_START 10 // 0 is push data, 1-9 are reserve for internal request

class ConvertUtils
{
public:
    static int64_t ProtoReq2JsonReq(int64_t reqid, const std::string& reqProtoStr, Json::Value& reqJson)
    {
        reqJson.clear();
        ed::MsgCarrier msgCarrier;
        if(!msgCarrier.ParseFromString(reqProtoStr) || msgCarrier.msg_type() != ed::TypeDef_MsgType::TypeDef_MsgType_REQ){
            return -1;
        }
        // write same fields
        reqJson["reqid"] = reqid;
        reqJson["session"] = "";
        if(msgCarrier.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_HEARTBEAT){
            ed::Heartbeat heartbeatReq;
            if(!heartbeatReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            reqJson["reqtype"] = 1;
            reqJson["data"]["connectionid"] = heartbeatReq.connection_id();
        }
        else if(msgCarrier.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_ALL_INS_INFO){
            reqJson["reqtype"] = 52;
        }
        else if(msgCarrier.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_SUB_QUOTES){
            ed::SubQuotes subReq;
            if(!subReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            reqJson["reqtype"] = 200;
            reqJson["data"].resize(0);
            Json::Value oneCodeJson;
            oneCodeJson["type"] = 1; // 行情
            oneCodeJson["market"] = subReq.market_id();
            oneCodeJson["code"] = subReq.ins_id();
            reqJson["data"].append(oneCodeJson);
        }
        else if(msgCarrier.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_UNSUB_QUOTES){
            ed::UnSubQuotes unSubReq;
            if(!unSubReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            reqJson["reqtype"] = 201;
            reqJson["data"].resize(0);
            Json::Value oneCodeJson;
            oneCodeJson["type"] = 1; // 行情
            oneCodeJson["market"] = unSubReq.market_id();
            oneCodeJson["code"] = unSubReq.ins_id();
            reqJson["data"].append(oneCodeJson);
        }
        else{
            return -1;
        }
        return msgCarrier.req_id();
    }

    static int JsonRes2ProtoRes(int64_t& reqId, const Json::Value& resJson, std::string& resProto)
    {
        reqId = resJson.isMember("reqid") && resJson["reqid"].isInt() ? resJson["reqid"].asInt() : -1;
        if(reqId == -1){
            printf("Get req id failed, msg: %s\n", resJson.toStyledString().c_str());
            return -1;
        }
        int reqType = resJson["reqtype"].asInt();
        // internal req convert
        if(reqId < REQ_ID_START && reqId > 0){
            if (reqType == 52){ // internal query instrument
                Json::Value insInfoVec = resJson["data"]["symbol"];
                for(int i = 0; i < insInfoVec.size(); ++i){
                    auto oneInfo = m_allInsInfoResProto.add_data();
                    oneInfo->set_ins_id(GBK2UTF8(insInfoVec[i]["code"].asString()));
                    oneInfo->set_market_id(insInfoVec[i]["market"].asInt());
                    oneInfo->set_ins_name(GBK2UTF8(insInfoVec[i]["name"].asString()));
                    oneInfo->set_ins_en_name(insInfoVec[i]["enname"].asString());
                }
                return 0;
            }
            else if(reqType == 1 || reqType == 0){// internal heartbeat
                return 0;
            }
            else if(reqType == 201){ // unsub res
                return 0;
            }
            else{
                return -1;
            }
        }

        // external req convert
        ed::MsgCarrier msgCarrier;
        msgCarrier.set_req_id(reqId);
        msgCarrier.set_msg_type(reqId == 0 ? ed::TypeDef_MsgType::TypeDef_MsgType_PUSH : ed::TypeDef_MsgType::TypeDef_MsgType_RESP);
        if(reqType == 1 || reqType == 0){// heartbeat
            ed::HeartbeatResp heartbeatResp;
            heartbeatResp.set_error_code(resJson["status"].asInt());
            heartbeatResp.set_error_msg("");
            heartbeatResp.set_server_time(resJson["servertime"].asString());
            msgCarrier.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_HEARTBEAT);
            msgCarrier.set_data(heartbeatResp.SerializeAsString());
        }
        else if(reqType == 153){// query quote info
            ed::QuoteInfo quoteInfo;
            if (!resJson["data"]["symbol"].empty()){
                auto oneQuoteInfo = resJson["data"]["symbol"][0];
                UpdateQuoteProtoByJson(oneQuoteInfo, quoteInfo);
                msgCarrier.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_PUSH);
                msgCarrier.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_PUSH_QUOTES);
                msgCarrier.set_data(quoteInfo.SerializeAsString());
                std::string type = std::to_string(quoteInfo.market_id()) + quoteInfo.ins_id();
                m_type2quoteMap[type] = quoteInfo;
            }
        }
        else if(reqType == 200){// sub res
            ed::SubQuotesResp subResp;
            subResp.set_error_code(std::atoi(resJson["data"]["ret"].asString().c_str()));
            msgCarrier.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_SUB_QUOTES);
            msgCarrier.set_data(subResp.SerializeAsString());
        }
        else if(reqType == 250){// push 
            if (!resJson["data"]["symbol"].empty()){
                auto oneQuoteInfo = resJson["data"]["symbol"][0];
                std::string type = std::to_string(oneQuoteInfo["market"].asInt()) + oneQuoteInfo["code"].asString();
                auto it = m_type2quoteMap.find(type);
                if(it != m_type2quoteMap.end()){
                    UpdateQuoteProtoByJson(oneQuoteInfo, it->second);
                    msgCarrier.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_PUSH);
                    msgCarrier.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_PUSH_QUOTES);
                    msgCarrier.set_data(it->second.SerializeAsString());
                }
            }
        }
        else if(reqType == 150){// unsub res 
            ed::UnSubQuotesResp unSubResp;
            subResp.set_error_code(std::atoi(resJson["data"]["ret"].asString().c_str()));
            msgCarrier.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_UNSUB_QUOTES);
            msgCarrier.set_data(subResp.SerializeAsString());
        }
        else{
            return -1;
        }
        resProto = msgCarrier.SerializeAsString();
        return 0;
    }

    static void UpdateQuoteProtoByJson(const Json::Value& quoteJson, ed::QuoteInfo& quoteProto)
    {
        if(quoteProto.bids_size() < 10 || quoteProto.offers_size() < 10){
            quoteProto.CopyFrom(m_template);
        }
        if(quoteJson.isMember("market")){
            quoteProto.set_market_id(quoteJson["market"].asInt());
        } 
        if(quoteJson.isMember("code")){
            quoteProto.set_ins_id(quoteJson["code"].asString());
        }   
        if(quoteJson.isMember("open")){
            quoteProto.set_open(quoteJson["open"].asDouble());
        }   
        if(quoteJson.isMember("high")){
            quoteProto.set_high(quoteJson["high"].asDouble());
        }   
        if(quoteJson.isMember("low")){
            quoteProto.set_low(quoteJson["low"].asDouble());
        }   
        if(quoteJson.isMember("lastclose")){
            quoteProto.set_last_close(quoteJson["lastclose"].asDouble());
        }   
        if(quoteJson.isMember("now")){
            quoteProto.set_last_price(quoteJson["now"].asDouble());
        }   
        if(quoteJson.isMember("amount")){
            quoteProto.set_total_amount(quoteJson["amount"].asDouble());
        }   
        if(quoteJson.isMember("volume")){
            quoteProto.set_total_volume(quoteJson["volume"].asDouble());
        }   
        if(quoteJson.isMember("avg")){
            quoteProto.set_avg_price(quoteJson["avg"].asDouble());
        }   
        if(quoteJson.isMember("time")){
            quoteProto.set_time(quoteJson["time"].asString());
        }   
        if(quoteJson.isMember("millisecond")){
            quoteProto.set_millisecond(quoteJson["millisecond"].asInt());
        }   
        if(quoteJson.isMember("hold")){
            quoteProto.set_open_interest(quoteJson["hold"].asDouble());
        }   
        if(quoteJson.isMember("settle")){
            quoteProto.set_settle_price(quoteJson["settle"].asDouble());
        }   
        if(quoteJson.isMember("buyvolume")){
            quoteProto.set_buy_volume(quoteJson["buyvolume"].asDouble());
        }   
        if(quoteJson.isMember("sellvolume")){
            quoteProto.set_sell_volume(quoteJson["sellvolume"].asDouble());
        }
        // buy
        for(int i = 0; i < 10; ++i){
            if(quoteJson.isMember(m_buyKey[i])){
                quoteProto.mutable_bids(i)->set_price(quoteJson[m_buyKey[i]].asDouble());
            }
            if(quoteJson.isMember(m_buyKey[i + 10])){
                quoteProto.mutable_bids(i)->set_volume(quoteJson[m_buyKey[i + 10]].asDouble());
            }
            if(quoteJson.isMember(m_buyKey[i + 20])){
                quoteProto.mutable_bids(i)->set_broker(quoteJson[m_buyKey[i + 20]].asDouble());
            }
        }
        // sell
        for(int i = 0; i < 10; ++i){
            if(quoteJson.isMember(m_sellKey[i])){
                quoteProto.mutable_offers(i)->set_price(quoteJson[m_sellKey[i]].asDouble());
            }
            if(quoteJson.isMember(m_sellKey[i + 10])){
                quoteProto.mutable_offers(i)->set_volume(quoteJson[m_sellKey[i + 10]].asDouble());
            }
            if(quoteJson.isMember(m_sellKey[i + 20])){
                quoteProto.mutable_offers(i)->set_broker(quoteJson[m_sellKey[i + 20]].asDouble());
            }
        }
        
    }

    // tool
    static int GBK2UTF8(const std::string &input, std::string &output)
    {
        int ret = 0;
        size_t charInPutLen = input.length();
        if (charInPutLen == 0)
        {
            return 0;
        }
        size_t charOutPutLen = 2 * charInPutLen + 1;
        char *pTemp = new char[charOutPutLen];
        memset(pTemp, 0, charOutPutLen);
        iconv_t cd;
        char *pSource = (char *)input.c_str();
        char *pOut = pTemp;
        cd = iconv_open("utf-8", "GBK");
        ret = iconv(cd, &pSource, &charInPutLen, &pTemp, &charOutPutLen);
        iconv_close(cd);
        output = pOut;
        delete[] pOut; //注意这里，不能使用delete []pTemp, iconv函数会改变指针pTemp的值
        return ret;
    }

    static std::string GBK2UTF8(const std::string &input)
    {
        std::string output;
        GBK2UTF8(input, output);
        return std::move(output);
    }

public:
    static ed::QueryAllInsInfoResp m_allInsInfoResProto;
    static std::string m_queryAllInsResStr;
    static ed::QuoteInfo m_template;
    static std::unordered_map<std::string, ed::QuoteInfo> m_type2quoteMap;//market|code -> quote info
    static std::vector<std::string> m_buyKey;
    static std::vector<std::string> m_sellKey;
};

ed::QueryAllInsInfoResp ConvertUtils::m_allInsInfoResProto;
std::string ConvertUtils::m_queryAllInsResStr{""};
ed::QuoteInfo ConvertUtils::m_template;
std::unordered_map<std::string, ed::QuoteInfo> ConvertUtils::m_type2quoteMap;
std::vector<std::string> ConvertUtils::m_buyKey{"buyprice0", "buyprice1", "buyprice2", "buyprice3", "buyprice4", "buyprice5", "buyprice6", "buyprice7", "buyprice8", "buyprice9", 
                                                "buyvol0", "buyvol1", "buyvol2", "buyvol3", "buyvol4", "buyvol5", "buyvol6", "buyvol7", "buyvol8", "buyvol9", 
                                                "buybroker0", "buybroker1", "buybroker2", "buybroker3", "buybroker4", "buybroker5", "buybroker6", "buybroker7", "buybroker8", "buybroker9"};
std::vector<std::string> ConvertUtils::m_sellKey{"sellprice0", "sellprice1", "sellprice2", "sellprice3", "sellprice4", "sellprice5", "sellprice6", "sellprice7", "sellprice8", "sellprice9", 
                                                "sellvol0", "sellvol1", "sellvol2", "sellvol3", "sellvol4", "sellvol5", "sellvol6", "sellvol7", "sellvol8", "sellvol9", 
                                                "sellbroker0", "sellbroker1", "sellbroker2", "sellbroker3", "sellbroker4", "sellbroker5", "sellbroker6", "sellbroker7", "sellbroker8", "sellbroker9"};