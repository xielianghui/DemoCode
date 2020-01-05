#include <iostream>

#include "uws_client.h"

//g++ -std=c++17 -pthread ../thirdparty/uWS/*.cpp main.cpp uws_client.cpp -o client -luv -lssl -lcrypto -lz -levent


static int i = 0;
static void OnMessage(std::string&& res)
{
    std::cout<<i++<<std::endl;
}

int main()
{
    struct event_base* base = event_base_new();
    UwsClient uClient;
    uClient.Register(base, &OnMessage);
    uClient.Connect("127.0.0.1", 8500);
    
    event_base_dispatch(base);
    event_base_free(base);
    return 0;
}