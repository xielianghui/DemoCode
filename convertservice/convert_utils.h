#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include <tuple>
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
        eddid::MsgCarrier msgCarrier;
        if(!msgCarrier.ParseFromString(reqProtoStr) || msgCarrier.msg_type() != eddid::TypeDef_MsgType::TypeDef_MsgType_REQ){
            return -1;
        }
        // write same fields
        reqJson["reqid"] = reqid;
        reqJson["session"] = "";
        if(msgCarrier.req_type() == eddid::TypeDef_ReqType::TypeDef_ReqType_HEARTBEAT){
            eddid::Heartbeat heartbeatReq;
            if(!heartbeatReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            reqJson["reqtype"] = 1;
            reqJson["data"]["connectionid"] = heartbeatReq.connection_id();
        }
        else if(msgCarrier.req_type() == eddid::TypeDef_ReqType::TypeDef_ReqType_ALL_INS_INFO){
            reqJson["reqtype"] = 52;
        }
        else if(msgCarrier.req_type() == eddid::TypeDef_ReqType::TypeDef_ReqType_SUB_QUOTES){
            eddid::SubQuotes subReq;
            if(!subReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            // find market code
            reqJson["reqtype"] = 200;
            reqJson["data"].resize(0);
            Json::Value oneCodeJson;
            oneCodeJson["type"] = (int)subReq.type();
            std::string key = std::to_string((int)subReq.exchange()) + "|" + subReq.ins_id();
            auto it = m_subChangeMap.find(key);
            if(it != m_subChangeMap.end()){
                oneCodeJson["market"] = std::get<0>(it->second);
                oneCodeJson["code"] = std::get<1>(it->second);
            }
            reqJson["data"].append(oneCodeJson);
        }
        else if(msgCarrier.req_type() == eddid::TypeDef_ReqType::TypeDef_ReqType_UNSUB_QUOTES){
            eddid::UnSubQuotes unSubReq;
            if(!unSubReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            reqJson["reqtype"] = 201;
            reqJson["data"].resize(0);
            Json::Value oneCodeJson;
            oneCodeJson["type"] = (int)unSubReq.type();
            std::string key = std::to_string((int)unSubReq.exchange()) + "|" + unSubReq.ins_id();
            auto it = m_subChangeMap.find(key);
            if(it != m_subChangeMap.end()){
                oneCodeJson["market"] = std::get<0>(it->second);
                oneCodeJson["code"] = std::get<1>(it->second);
            }
            reqJson["data"].append(oneCodeJson);
        }
        else if(msgCarrier.req_type() == eddid::TypeDef_ReqType::TypeDef_ReqType_K_LINE){
            eddid::QueryKLine queryKLineReq;
            if(!queryKLineReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            reqJson["reqtype"] = 150;
            reqJson["data"]["weight"] = 0;
            reqJson["data"]["timetype"] = 0;
            reqJson["data"]["klinetype"] = (int)queryKLineReq.type();
            reqJson["data"]["time0"] = queryKLineReq.start_time();
            reqJson["data"]["time1"] = queryKLineReq.end_time();
            std::string key = std::to_string((int)queryKLineReq.exchange()) + "|" + queryKLineReq.ins_id();
            auto it = m_subChangeMap.find(key);
            if(it != m_subChangeMap.end()){
                reqJson["data"]["market"] = std::get<0>(it->second);
                reqJson["data"]["code"] = std::get<1>(it->second);
            }
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
            std::cout<<"JsonRes2ProtoRes(): Get req id failed, msg:"<<resJson.toStyledString()<<std::endl;
            return -1;
        }
        int reqType = resJson["reqtype"].asInt();
        // internal req convert
        if(reqId < REQ_ID_START && reqId > 0){
            if (reqType == 52){ // internal query instrument
                Json::Value insInfoVec = resJson["data"]["symbol"];
                for(int i = 0; i < insInfoVec.size(); ++i){
                    auto oneInfo = m_allInsInfoResProto.add_data();

                    int market = insInfoVec[i]["market"].asInt();
                    std::string code = insInfoVec[i]["code"].asString();
                    eddid::TypeDef::Exchange exchange = MarketId2Exchange(market);
                    std::string tradecode = GBK2UTF8(insInfoVec[i]["tradecode"].asString());
                    oneInfo->set_exchange(exchange);
                    oneInfo->set_ins_id(tradecode);
                    oneInfo->set_ins_name(GBK2UTF8(insInfoVec[i]["name"].asString()));
                    oneInfo->set_ins_en_name(insInfoVec[i]["enname"].asString());
                    oneInfo->set_settle_currency(CurrencyCode2type(insInfoVec[i]["currencycode"].asString()));
                    oneInfo->set_trade_currency(CurrencyCode2type(insInfoVec[i]["currencycode"].asString()));
                    oneInfo->set_product_code(insInfoVec[i]["commoditycode"].asString());
                    auto it = m_market2timeMap.find(market);
                    if(it != m_market2timeMap.end()) oneInfo->set_trading_time_slice(it->second);
                    oneInfo->set_lot_size(insInfoVec[i]["lotsize"].asInt());
                    oneInfo->set_listing_date(insInfoVec[i]["listingdate"].asString());
                    int expireDate = insInfoVec[i]["expiredate"].asInt();
                    oneInfo->set_delivery_year(expireDate / 10000);
                    oneInfo->set_delivery_mouth((expireDate % 10000) / 100);
                    oneInfo->set_start_delivery_date(std::to_string(expireDate));
                    // save info
                    eddid::InsInfo insInfo;
                    insInfo.set_exchange(exchange);
                    insInfo.set_product_code(insInfoVec[i]["commoditycode"].asString());
                    insInfo.set_ins_id(tradecode);
                    m_type2insInfoMap[std::to_string(market) + "|" + code] = insInfo;

                    m_subChangeMap[(std::to_string((int)exchange) + "|" + tradecode)] = \
                        std::make_tuple<int, std::string>(std::move(market), std::move(code));
                    
                }
                return 0;
            }
            else if(reqType == 50){
                auto marketArray = resJson["data"]["market"];
                for(int i = 0; i < marketArray.size(); ++i)
                {
                    auto openCloseTime = marketArray[i]["openclosetime"];
                    m_market2timeMap[marketArray[i]["market"].asInt()] = openCloseTime.toStyledString();
                }
                std::cout<<"Get all markets open-close time, market size:"<<m_market2timeMap.size()<<std::endl;
                return 0;
            }
            else if(reqType == 1 || reqType == 0){// internal heartbeat
                return 0;
            }
            else if(reqType == 200 || reqType == 201){ // internal sub/unsub res
                return 0;
            }
            else{
                return -1;
            }
        }

        // external req convert
        eddid::MsgCarrier msgCarrier;
        msgCarrier.set_req_id(reqId);
        msgCarrier.set_msg_type(reqId == 0 ? eddid::TypeDef_MsgType::TypeDef_MsgType_PUSH : eddid::TypeDef_MsgType::TypeDef_MsgType_RESP);
        if(reqType == 1 || reqType == 0){// heartbeat
            eddid::HeartbeatResp heartbeatResp;
            heartbeatResp.set_error_code(resJson["status"].asInt());
            heartbeatResp.set_error_msg("");
            heartbeatResp.set_server_time(resJson["servertime"].asString());
            msgCarrier.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_HEARTBEAT);
            msgCarrier.set_data(heartbeatResp.SerializeAsString());
        }
        else if(reqType == 153){// query quote info
            eddid::QuoteInfo quoteInfo;
            if (!resJson["data"]["symbol"].empty()){
                auto oneQuoteInfo = resJson["data"]["symbol"][0];
                UpdateQuoteProtoByJson(oneQuoteInfo, quoteInfo);
                msgCarrier.set_msg_type(eddid::TypeDef_MsgType::TypeDef_MsgType_PUSH);
                msgCarrier.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_PUSH_QUOTES);
                msgCarrier.set_data(quoteInfo.SerializeAsString());
                std::string type = std::to_string(oneQuoteInfo["market"].asInt()) + "|" + oneQuoteInfo["code"].asString();
                m_type2quoteMap[type] = quoteInfo;
            }
        }
        else if(reqType == 200){// sub res
            eddid::SubQuotesResp subResp;
            subResp.set_error_code(std::atoi(resJson["data"]["ret"].asString().c_str()));
            msgCarrier.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_SUB_QUOTES);
            msgCarrier.set_data(subResp.SerializeAsString());
        }
        else if(reqType == 150){// unsub res  / k line res also is 150 (wtf)
            auto dataJson = resJson["data"];
            if(dataJson.isMember("ret")){
                eddid::UnSubQuotesResp unSubResp;
                unSubResp.set_error_code(std::atoi(resJson["data"]["ret"].asString().c_str()));
                msgCarrier.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_UNSUB_QUOTES);
                msgCarrier.set_data(unSubResp.SerializeAsString());
            }
            else{
                eddid::QueryKLineResp queryKLineRes;
                if(resJson["status"].asInt() != 0){
                    queryKLineRes.set_error_code(resJson["status"].asInt());
                    queryKLineRes.set_error_msg(resJson["msg"].asString());
                }
                else{
                    std::string type = std::to_string(resJson["data"]["market"].asInt()) + "|" + resJson["data"]["code"].asString();
                    auto it = m_type2insInfoMap.find(type);
                    if (it != m_type2insInfoMap.end()){
                        queryKLineRes.set_exchange(it->second.exchange());
                        queryKLineRes.set_product_code(it->second.product_code());
                        queryKLineRes.set_ins_id(it->second.ins_id());
                        auto klArray = resJson["data"]["kline"];
                        for(int i = 1; i < klArray.size(); ++i)
                        {
                            auto oneInfo = queryKLineRes.add_data();
                            auto dataArray = klArray[i];
                            if(dataArray.size() < 7) continue;
                            oneInfo->set_time(dataArray[0].asString());
                            oneInfo->set_open(dataArray[1].asDouble());
                            oneInfo->set_high(dataArray[2].asDouble());
                            oneInfo->set_low(dataArray[3].asDouble());
                            oneInfo->set_close(dataArray[4].asDouble());
                            oneInfo->set_total_volume(dataArray[5].asDouble());
                            oneInfo->set_total_amount(dataArray[6].asDouble());
                        }
                    }
                }
                msgCarrier.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_K_LINE);
                msgCarrier.set_data(queryKLineRes.SerializeAsString());
            }
        }
        else if(reqType == 250){// push quotes
            if (!resJson["data"]["symbol"].empty()){
                auto oneQuoteInfo = resJson["data"]["symbol"][0];
                std::string type = std::to_string(oneQuoteInfo["market"].asInt()) + "|" +  oneQuoteInfo["code"].asString();
                auto it = m_type2quoteMap.find(type);
                if(it != m_type2quoteMap.end()){
                    UpdateQuoteProtoByJson(oneQuoteInfo, it->second);
                    msgCarrier.set_msg_type(eddid::TypeDef_MsgType::TypeDef_MsgType_PUSH);
                    msgCarrier.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_PUSH_QUOTES);
                    msgCarrier.set_data(it->second.SerializeAsString());
                }
            }
        }
        else if(reqType == 251){// push tick
            if (!resJson["data"]["tick"].empty()){
                auto oneTickInfo = resJson["data"]["tick"][0];
                std::string type = std::to_string(oneTickInfo["market"].asInt()) + "|" + oneTickInfo["code"].asString();
                auto it = m_type2insInfoMap.find(type);
                if(it != m_type2insInfoMap.end()){
                    eddid::TickInfo tickInfo;
                    tickInfo.mutable_ins_info()->CopyFrom(it->second);
                    tickInfo.set_price(oneTickInfo["price"].asDouble());
                    tickInfo.set_volume(oneTickInfo["volume"].asInt());
                    tickInfo.set_time(oneTickInfo["time"].asString());//TODO
                    msgCarrier.set_msg_type(eddid::TypeDef_MsgType::TypeDef_MsgType_PUSH);
                    msgCarrier.set_req_type(eddid::TypeDef_ReqType::TypeDef_ReqType_PUSH_TICKS);
                    msgCarrier.set_data(tickInfo.SerializeAsString());
                }
            }
        }
        else{
            return -1;
        }
        resProto = msgCarrier.SerializeAsString();
        return 0;
    }

    static void UpdateQuoteProtoByJson(const Json::Value& quoteJson, eddid::QuoteInfo& quoteProto)
    {
        if(quoteProto.bids_size() < 10 || quoteProto.asks_size() < 10){
            quoteProto.CopyFrom(m_template);
        }
        if(quoteJson.isMember("market")){
            quoteProto.mutable_ins_info()->set_exchange(MarketId2Exchange(quoteJson["market"].asInt()));
        } 
        if(quoteJson.isMember("commoditycode")){
            quoteProto.mutable_ins_info()->set_product_code(quoteJson["commoditycode"].asString());
        }
        if(quoteJson.isMember("tradecode")){
            quoteProto.mutable_ins_info()->set_ins_id(quoteJson["tradecode"].asString());
        }
        if(quoteJson.isMember("name")){
            quoteProto.set_ins_name(quoteJson["name"].asString());
        }
        if(quoteJson.isMember("enname")){
            quoteProto.set_ins_en_name(quoteJson["enname"].asString());
        }
        if(quoteJson.isMember("raiselimit")){
            quoteProto.set_raise_limit(quoteJson["raiselimit"].asDouble());
        }
        if(quoteJson.isMember("downlimit")){
            quoteProto.set_down_limit(quoteJson["downlimit"].asDouble());
        }
        if(quoteJson.isMember("lastclose")){
            quoteProto.set_last_close(quoteJson["lastclose"].asDouble());
        }
        if(quoteJson.isMember("lastsettle")){
            quoteProto.set_pre_settle_price(quoteJson["lastsettle"].asDouble());
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
        if(quoteJson.isMember("now")){
            quoteProto.set_close(quoteJson["now"].asDouble());
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
        if(quoteJson.isMember("time")){ // TODO 
            quoteProto.set_time(quoteJson["time"].asString());
        }
        if(quoteJson.isMember("millisecond")){
            quoteProto.set_millisecond(quoteJson["millisecond"].asInt());
        }   
        if(quoteJson.isMember("hold")){
            quoteProto.set_hold(quoteJson["hold"].asDouble());
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
        }
        // sell
        for(int i = 0; i < 10; ++i){
            if(quoteJson.isMember(m_sellKey[i])){
                quoteProto.mutable_asks(i)->set_price(quoteJson[m_sellKey[i]].asDouble());
            }
            if(quoteJson.isMember(m_sellKey[i + 10])){
                quoteProto.mutable_asks(i)->set_volume(quoteJson[m_sellKey[i + 10]].asDouble());
            }
        }
        
    }

    static eddid::TypeDef::Exchange MarketId2Exchange(int market)
    {
        switch(market)
        {
        case 2011:case 2015:case 2009:case 2010:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_HKFE;
        case 2002:case 2031:case 2005:case 2003:case 2004:case 2000:case 2001:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_SEHK;
        case 40001:case 40002: case 40003:case 40004:case 40005:case 40006:case 40007:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_ASE;
        case 50000:case 50101:case 50102:case 50103:case 50104:case 50105:
        case 50106:case 50107:case 50108:case 50109:case 50110:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_CBOT;
        case 50001:case 51201:case 51202:case 51203:case 51204:case 51205:case 51206:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_COMEX;
        case 50002:case 52301:case 52302:case 52303:case 52304:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_NYMEX;
        case 50003:case 50004:case 53401:case 53402:case 53403:case 53404:case 54501:
        case 54502:case 54503:case 54504:case 54505:case 54506:case 54507:case 54508:case 54509:case 54510:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_CME;
        case 50005:case 55601:case 55602:case 55603:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_SGX;
        default:
            return eddid::TypeDef_Exchange::TypeDef_Exchange_EXCHANGE_UNKNOW;
        }
    }

    static eddid::TypeDef::Currency CurrencyCode2type(const std::string currencyCode)
    {
        if(currencyCode == "CNY"){
            return eddid::TypeDef_Currency::TypeDef_Currency_CNY;
        }
        else if(currencyCode == "USD"){
            return eddid::TypeDef_Currency::TypeDef_Currency_USD;
        }
        else if(currencyCode == "HKD"){
            return eddid::TypeDef_Currency::TypeDef_Currency_HKD;
        }
        else if(currencyCode == "EUR"){
            return eddid::TypeDef_Currency::TypeDef_Currency_EUR;
        }
        else if(currencyCode == "GBP"){
            return eddid::TypeDef_Currency::TypeDef_Currency_GBP;
        }
        else if(currencyCode == "JPY"){
            return eddid::TypeDef_Currency::TypeDef_Currency_JPY;
        }
        else {
            return eddid::TypeDef_Currency::TypeDef_Currency_CURRENCY_UNKNOWN;
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
    static Json::Value queryMarketJson;
    static eddid::QueryAllInsInfoResp m_allInsInfoResProto;
    static std::string m_queryAllInsResStr;
    // exchange|trade code(合约id) -> market, code(订阅id)
    static std::unordered_map<std::string, std::tuple<int, std::string>> m_subChangeMap;
    // quote info template
    static eddid::QuoteInfo m_template;
    static std::unordered_map<std::string, eddid::QuoteInfo> m_type2quoteMap;//market|code -> quote info
    static std::vector<std::string> m_buyKey;
    static std::vector<std::string> m_sellKey;
    // open time 
    static std::unordered_map<int, std::string> m_market2timeMap;
    static std::unordered_map<std::string, eddid::InsInfo> m_type2insInfoMap;//market|code -> ins info
};

eddid::QueryAllInsInfoResp ConvertUtils::m_allInsInfoResProto;
std::string ConvertUtils::m_queryAllInsResStr{""};
std::unordered_map<std::string, std::tuple<int, std::string>> ConvertUtils::m_subChangeMap;
eddid::QuoteInfo ConvertUtils::m_template;
std::unordered_map<std::string, eddid::QuoteInfo> ConvertUtils::m_type2quoteMap;
std::vector<std::string> ConvertUtils::m_buyKey{"buyprice0", "buyprice1", "buyprice2", "buyprice3", "buyprice4", "buyprice5", "buyprice6", "buyprice7", "buyprice8", "buyprice9", 
                                                "buyvol0", "buyvol1", "buyvol2", "buyvol3", "buyvol4", "buyvol5", "buyvol6", "buyvol7", "buyvol8", "buyvol9"};
std::vector<std::string> ConvertUtils::m_sellKey{"sellprice0", "sellprice1", "sellprice2", "sellprice3", "sellprice4", "sellprice5", "sellprice6", "sellprice7", "sellprice8", "sellprice9", 
                                                "sellvol0", "sellvol1", "sellvol2", "sellvol3", "sellvol4", "sellvol5", "sellvol6", "sellvol7", "sellvol8", "sellvol9"};
std::unordered_map<int, std::string> ConvertUtils::m_market2timeMap;
Json::Value ConvertUtils::queryMarketJson;
std::unordered_map<std::string, eddid::InsInfo> ConvertUtils::m_type2insInfoMap;