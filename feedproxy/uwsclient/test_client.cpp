#include <iostream>
#include <thread>
#include <memory>
#include <chrono>

#include "../thirdparty/uWS/uWS.h"

//g++ -std=c++17 -pthread ../thirdparty/uWS/*.cpp test_client.cpp -o uclient -luv -lssl -lcrypto -lz
static uWS::Hub hub;
static uWS::WebSocket<uWS::CLIENT>* m_ws;
static std::string reqStr = "{\"reqtype\": 150,\"session\": \"\",\"data\": {\"market\": 2002,\"code\": \"00700\",\"klinetype\": 1,\"weight\": 0,\"timetype\": 0,\"time0\": \"2019-12-20 01:30:00\",\"time1\": \"2019-12-20 01:35:00\"}}";
static void OnConnect(uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest /*req*/)
{
    std::cout<<"OnConnect(), ws"<<(intptr_t)ws<<std::endl;
    m_ws = ws;
}

static void OnMessage(uWS::WebSocket<uWS::CLIENT> *ws, char *data, size_t len, uWS::OpCode type)
{
    std::cout<<"OnMessage(), ws"<<(intptr_t)ws<<std::endl;
    std::string res(data, len);
    std::cout<<"Recv: "<<res<<std::endl;
}

static void OnDisconnection(uWS::WebSocket<uWS::CLIENT> *ws, int code, char *data, size_t len) 
{
    std::cout<<"OnDisconnection(), ws"<<(intptr_t)ws<<std::endl;
    std::cout<<"Reconnect..."<<std::endl;
    hub.connect("ws://127.0.0.1:8500");
}

int main()
{
    hub.onConnection(&OnConnect);
    hub.onMessage(&OnMessage);
    hub.onDisconnection(&OnDisconnection);
    hub.connect("ws://127.0.0.1:8500");
    hub.run();
    return 0;
}