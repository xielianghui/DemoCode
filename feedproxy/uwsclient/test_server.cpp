#include <iostream>
#include <thread>
#include <memory>
#include <chrono>

#include "../thirdparty/uWS/uWS.h"

//g++ -std=c++17 -pthread ../thirdparty/uWS/*.cpp test_server.cpp -o userver -luv -lssl -lcrypto -lz
static uWS::Hub hub;
static int i = 0;
static void OnConnect(uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest /*req*/)
{
    std::cout<<"OnConnect(), ws"<<(intptr_t)ws<<std::endl;
    if(i++ == 11){
        ws->close();
    }
}

static void OnMessage(uWS::WebSocket<uWS::SERVER> *ws, char *data, size_t len, uWS::OpCode type)
{
    std::cout<<"OnMessage(), ws"<<(intptr_t)ws<<std::endl;
    std::string res(data, len);
    std::cout<<"Recv: "<<res<<std::endl;
}

static void OnDisconnection(uWS::WebSocket<uWS::SERVER> *ws, int code, char *data, size_t len) 
{
    std::cout<<"OnDisconnection(), ws"<<(intptr_t)ws<<std::endl;
}

int main()
{
    hub.onConnection(&OnConnect);
    hub.onMessage(&OnMessage);
    hub.listen(8500);
    hub.run();
    return 0;
}