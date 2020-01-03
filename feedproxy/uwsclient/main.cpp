#include <iostream>

#include "../thirdparty/uWS/uWS.h"

//g++ -std=c++17 -pthread ../thirdparty/uWS/*.cpp main.cpp -o a -luv -lssl -lcrypto -lz


static void OnConnect(uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest /*req*/)
{
    std::cout<<"OnConnect(), ws"<<(intptr_t)ws<<std::endl;
    std::string reqStr = "{\"reqtype\": 150,\"session\": \"\",\"data\": {\"market\": 2002,\"code\": \"00700\",\"klinetype\": 1,\"weight\": 0,\"timetype\": 0,\"time0\": \"2019-12-20 01:30:00\",\"time1\": \"2019-12-20 01:35:00\"}}";
    ws->send(reqStr.c_str());
}

static void OnMessage(uWS::WebSocket<uWS::CLIENT> *ws, char *data, size_t len, uWS::OpCode type)
{
    std::cout<<"OnMessage(), ws"<<(intptr_t)ws<<std::endl;
    std::string res(data, len);
    std::cout<<"Recv: "<<res<<std::endl;
}

int main()
{
    uWS::Hub hub;
    hub.onConnection(&OnConnect);
    hub.connect("ws://120.78.155.211:8500");
    hub.onMessage(&OnMessage);
    hub.run();
    return 0;
}