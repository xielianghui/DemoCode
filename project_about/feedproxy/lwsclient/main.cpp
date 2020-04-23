#include <iostream>
#include <fstream>
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
    if(args < 4){
        puts("[Usage:] lwstest ip_address port json_req");
        return -1;
    }
    std::string addr(argv[1]);
    int port = std::atoi(argv[2]);
    std::string reqStrFile(argv[3]);

    std::ifstream file(reqStrFile);
    if(!file){
        printf("Open file failed, error msg:%s\n", strerror(errno));
        return -1;
    }
    std::string reqStr = "{\"reqtype\":200,\"reqid\":0,\"session\":\"\",\"data\":[";//{\"market\":2015,\"code\":\"THHIZ\",\"type\":1}]}"
    //std::string reqStr = "{\"reqtype\":51,\"reqid\":1,\"session\":\"\",\"data\":[";
    int i = 0;
    int lineNums = 30;
    std::string oneline;
    while(std::getline(file, oneline) && i++ < lineNums)
    {
        reqStr.append(oneline);
    }
    reqStr.pop_back();
    reqStr += "]}";

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




