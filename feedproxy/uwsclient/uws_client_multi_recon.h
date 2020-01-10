#pragma once
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "json/json.h"

#include "concurrentqueue.h"
#include "../thirdparty/uWS/uWS.h"
#include "../public/event_notify.h"

#define BULK_BATCH_SIZE 300
#define SUB_REQ_TYPE 200
#define UNSUB_REQ_TYPE 201
#define WS_SIZE 12
#define PER_WS_CODE_NUMS 8

struct BufTraits : public moodycamel::ConcurrentQueueDefaultTraits
{
    static const size_t BLOCK_SIZE = 32;
};

struct WsConnections
{
    uWS::WebSocket<uWS::CLIENT>* ws;
    int counts;
    WsConnections(uWS::WebSocket<uWS::CLIENT>* wsTemp, int c = 0):ws(wsTemp), counts(c){}
    bool operator <(const WsConnections &temp) const{return this->counts > temp.counts;}
    int SendReq(const std::string& msg)
    {
        if(ws){
            if(!ws->isShuttingDown() && !ws->isClosed()){
                ws->send(msg.c_str());
                return 0;
            }
        }
        return -1;
    }
};
static std::unordered_map<std::string, std::shared_ptr<WsConnections>> symbol2conMap;
static std::unordered_map<intptr_t, std::unordered_set<std::string>> con2symbolMap;
static std::unordered_map<intptr_t, std::shared_ptr<WsConnections>> allWsMap;
static std::queue<intptr_t> disconWsQue;
static uWS::WebSocket<uWS::CLIENT>* queryWs{nullptr};

static Json::Value subTemplate;
static Json::Value resubTemplate;
static Json::Value unsubTemplate;

template<typename T>
class UwsClient final
{
public:
    typedef void(T::*OnRecv)(std::vector<std::string>&&);

    UwsClient(T* mainService):
        m_mainService(mainService),
        m_cb(nullptr),
        m_swapTimer(nullptr),
        m_consumerToken(m_resQue),
        m_producerToken(m_resQue)
    {
        subTemplate["reqtype"] = SUB_REQ_TYPE;
        subTemplate["data"].resize(0);

        resubTemplate["reqtype"] = SUB_REQ_TYPE;
        resubTemplate["reqid"] = 5;
        resubTemplate["data"].resize(0);

        unsubTemplate["reqtype"] = UNSUB_REQ_TYPE;
        unsubTemplate["data"].resize(0);
    }

    ~UwsClient()
    {
        if(m_swapTimer){
            event_free(m_swapTimer);
        }
    }
public:
    int SendReq(const std::string& msg)
    {
        Json::Value reqJson;
        Json::Reader jsonReader;
        if (!jsonReader.parse(msg, reqJson)){
            std::cout<<"SendReq(): Parse json failed, msg:"<<msg<<std::endl;
            return -1;
        }
        int reqType = reqJson["reqtype"].asInt();
        if(reqType != SUB_REQ_TYPE && reqType != UNSUB_REQ_TYPE){
            if(reqType == 1){// heartbeat
                for(auto& it : allWsMap){
                    it.second->SendReq(msg);
                }
            }
            if(queryWs){
                if(!queryWs->isShuttingDown() && !queryWs->isClosed()){
                    queryWs->send(msg.c_str());
                    return 0;
                }
            }
            return -1;
        }
        else{
            if(reqType == SUB_REQ_TYPE){
                auto subJsonArray = reqJson["data"];
                int sendCounts = 0;
                int needSubSize = subJsonArray.size();
                for(auto& it : allWsMap)
                {
                    if(sendCounts >= needSubSize) break;

                    int canSubNums = (PER_WS_CODE_NUMS - it.second->counts);
                    if(canSubNums <= 0) continue;

                    subTemplate["data"].resize(0);
                    subTemplate["reqid"] = reqJson["reqid"].asInt();
                    for(int i = 0; i < canSubNums && sendCounts < needSubSize; ++i, ++sendCounts)
                    {
                        subTemplate["data"].append(subJsonArray[sendCounts]);
                        it.second->counts += 1;
                        std::string symbol = subJsonArray[sendCounts]["type"].asString() + "|" + subJsonArray[sendCounts]["market"].asString() + "|" + subJsonArray[sendCounts]["code"].asString();
                        symbol2conMap[symbol] = it.second;
                        auto con2symbolIt = con2symbolMap.find((intptr_t)it.second->ws);
                        if(con2symbolIt == con2symbolMap.end()){
                            con2symbolIt = con2symbolMap.insert(std::make_pair((intptr_t)it.second->ws, std::unordered_set<std::string>())).first;
                        }
                        con2symbolIt->second.insert(symbol);
                    }
                    it.second->SendReq(subTemplate.toStyledString());
                }

                if(sendCounts < needSubSize){
                    std::cout<<"Can't sub all code, total code size: "<<needSubSize<<", sub size: "<<sendCounts<<std::endl;
                    return -1;
                }
                return 0;
            }
            else{
                auto unsubJsonArray = reqJson["data"];
                for(int i = 0; i < unsubJsonArray.size(); ++i)
                {
                    std::string symbol = unsubJsonArray[i]["type"].asString() + "|" + unsubJsonArray[i]["market"].asString() + "|" + unsubJsonArray[i]["code"].asString();
                    auto it = symbol2conMap.find(symbol);
                    if(it == symbol2conMap.end()){
                        std::cout<<"Unsub code error, code symbol: "<<symbol<<std::endl;
                        continue;
                    }
                    unsubTemplate["data"].resize(0);
                    unsubTemplate["reqid"] = reqJson["reqid"].asInt();
                    unsubTemplate["data"].append(unsubJsonArray[i]);
                    it->second->SendReq(unsubTemplate.toStyledString());
                    it->second->counts -= 1;
                    symbol2conMap.erase(it);
                    auto con2symbolIt = con2symbolMap.find((intptr_t)it->second->ws);
                    if(con2symbolIt != con2symbolMap.end()){
                        con2symbolIt->second.erase(symbol);
                    }
                }
            }
        }
    }
    
    void Register(event_base* base, OnRecv cb)
    {
        m_pBase = base;
        m_cb = cb;
        m_hub.onConnection([this](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest /*req*/) {
            if(disconWsQue.empty()){// first connect
                std::cout<<"Connected, ws: "<<(intptr_t)ws<<std::endl;
                if(!queryWs) queryWs = ws;
                else allWsMap.insert(std::make_pair((intptr_t)ws, std::make_shared<WsConnections>(ws)));
            }
            else{ //reconnect
                intptr_t disWs = disconWsQue.front();
                disconWsQue.pop();
                std::cout<<"Reconnect success, dis ws: "<<disWs<<", new ws: "<<(intptr_t)ws<<std::endl;

                std::unordered_set<std::string> tempSet;
                auto con2symbolIt = con2symbolMap.find((intptr_t)disWs);
                if(con2symbolIt != con2symbolMap.end()){
                    tempSet.swap(con2symbolIt->second);
                    con2symbolMap.erase(con2symbolIt);
                }
                for(auto& it : tempSet)
                {
                    symbol2conMap[it] = allWsMap[(intptr_t)ws];
                    std::vector<std::string> keyVec;
                    int start = 0;
                    for (int pos = 0; pos < it.size(); ++pos)
                    {
                        if (it[pos] == '|')
                        {
                            keyVec.emplace_back(it.substr(start, pos - start));
                            start = pos + 1;
                        }
                    }
                    keyVec.emplace_back(it.substr(start));
                    if (keyVec.size() < 3) continue;
                    Json::Value oneCodeJson;
                    oneCodeJson["type"] = std::atoi(keyVec[0].c_str());
                    oneCodeJson["market"] = std::atoi(keyVec[1].c_str());
                    oneCodeJson["code"] = keyVec[2];
                    subTemplate["data"].resize(0);
                    subTemplate["data"].append(oneCodeJson);
                    ws->send(subTemplate.toStyledString().c_str());
                }
                con2symbolMap[(intptr_t)ws] = tempSet;
                
                allWsMap.erase((intptr_t)disWs);
                allWsMap.insert(std::make_pair((intptr_t)ws, std::make_shared<WsConnections>(ws, tempSet.size())));
            }
        });
        m_hub.onMessage([this](uWS::WebSocket<uWS::CLIENT> *ws, char *data, size_t len, uWS::OpCode type) {
            m_resQue.enqueue(m_producerToken, std::string(data, len));
        });
        m_hub.onDisconnection([this](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *data, size_t len) {
            std::cout<<"Disconnect, ws: "<<(intptr_t)ws<<std::endl;
            auto con2symbolIt = con2symbolMap.find((intptr_t)ws);
            if(con2symbolIt != con2symbolMap.end()){
                for(auto& it : con2symbolIt->second)
                {
                    std::cout<<it<<std::endl;
                }
            }
            disconWsQue.push((intptr_t)ws);
            std::cout<<"Reconnect..."<<std::endl;
            m_hub.connect(m_hostName);
        });
        m_hub.onError([this](uWS::Group<uWS::CLIENT>::errorType) {
            std::cout<<"Connect error"<<std::endl;
        });

        timeval tv;
        evutil_timerclear(&tv);
        tv.tv_usec = 100000;
        m_swapTimer = event_new(base, -1, EV_PERSIST , OnSwapTimer, this);
        if (m_swapTimer == nullptr) {
            std::cout<<"event_new() swap timer failed"<<std::endl;
            return ;
        }
        event_add(m_swapTimer, &tv);

        evutil_timerclear(&tv);
        tv.tv_sec = 3 * 60;
        m_resubTimer = event_new(base, -1, EV_PERSIST , OnResubTimer, this);
        if (m_resubTimer == nullptr) {
            std::cout<<"event_new() resub timer failed"<<std::endl;
            return ;
        }
        event_add(m_resubTimer, &tv);
    }

    int Connect(std::string addr, int port)
    {
        m_hostName = ("ws://" + addr + ":" + std::to_string(port));
        for(int i = 0; i < WS_SIZE; ++i)
        {
            m_hub.connect(m_hostName);
        }
        m_thread = std::thread(&UwsClient::Run, this);
        m_thread.detach();
        return 0;
    }

    void OnNotify()
    {
        std::vector<std::string> resVec;
        resVec.resize(BULK_BATCH_SIZE);
        int ret = m_resQue.try_dequeue_bulk(m_consumerToken, resVec.begin(), BULK_BATCH_SIZE);
        resVec.resize(ret);
        (m_mainService->*m_cb)(std::move(resVec));
    }

    void Resub()
    {
        for(auto& it : symbol2conMap)
        {
            std::string key = it.first;
            std::vector<std::string> keyVec;
            int start = 0;
            for (int pos = 0; pos < key.size(); ++pos)
            {
                if (key[pos] == '|'){
                    keyVec.emplace_back(key.substr(start, pos - start));
                    start = pos + 1;
                }
            }
            keyVec.emplace_back(key.substr(start));
            if (keyVec.size() < 3) continue;
            Json::Value oneCodeJson;
            oneCodeJson["type"] = std::atoi(keyVec[0].c_str());
            oneCodeJson["market"] = std::atoi(keyVec[1].c_str());
            oneCodeJson["code"] = keyVec[2];
            resubTemplate["data"].resize(0);
            resubTemplate["data"].append(oneCodeJson);
            it.second->SendReq(resubTemplate.toStyledString());
        }
    }
private:
    void Run()
    {
        m_hub.run();
    }

    static void OnSwapTimer(evutil_socket_t fd, short event, void* args)
    {
        UwsClient<T>* th = (UwsClient<T>*)args;
        th->OnNotify();
    }

    static void OnResubTimer(evutil_socket_t fd, short event, void* args)
    {
        UwsClient<T>* th = (UwsClient<T>*)args;
        th->Resub();
    }

public:
    T* m_mainService;
    event_base* m_pBase;
    OnRecv m_cb;
    int m_port;
    std::string m_addr;
    uWS::Hub m_hub;
    std::thread m_thread;
    event* m_swapTimer;
    event* m_resubTimer;
    std::string m_hostName;
    moodycamel::ConcurrentQueue<std::string, BufTraits> m_resQue;
    moodycamel::ConcurrentQueue<std::string, BufTraits>::consumer_token_t m_consumerToken;
    moodycamel::ConcurrentQueue<std::string, BufTraits>::producer_token_t m_producerToken;
};