#include <iostream>
#include <string>

#include <event2/event.h>

#include "lws_client.h"

class LwsCb : public LwsCbInterface
{
public:
    LwsCb() = default;
    ~LwsCb() = default;
    void OnConnect() override ;
    void OnRecv(std::string&& msg) override ;
};

void LwsCb::OnConnect()
{
    printf("OnConnect\n");
}

void LwsCb::OnRecv(std::string&& msg)
{
    printf("\033[1;31;40mLength: %d Recv:\033[0m %s\n", msg.size(), msg.c_str());
}

int main(int args, char** argv)
{
    if(args < 3){
        puts("[Usage:] lwstest ip_address port json_req");
        return -1;
    }
    std::string addr(argv[1]);
    int port = std::atoi(argv[2]);
    std::string reqStr(argv[3]);

    struct event_base* base = event_base_new();
    LwsClient lct(base);
    std::shared_ptr<LwsCb> cbPtr = std::make_shared<LwsCb>();
    lct.Register(cbPtr.get());
    lct.Connect(addr, port);
    lct.SendReq(reqStr);

    event_base_dispatch(base);
    event_base_free(base);
    return 0;
}




