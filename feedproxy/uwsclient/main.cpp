#include <iostream>

#include "uws_client.h"

//g++ -std=c++17 -pthread ../thirdparty/uWS/*.cpp main.cpp -o a -luv -lssl -lcrypto -lz



static void OnMessage(std::vector<std::string>&& resVec)
{
    for(auto& it : resVec)
    {
        std::cout<<it<<std::endl;
    }
}

int main()
{
    struct event_base* base = event_base_new();
    UwsClient uClient;
    uClient.Register(base, &OnMessage);
    uClient.Connect("120.78.155.211", 8500);
    
    event_base_dispatch(base);
    event_base_free(base);
    return 0;
}