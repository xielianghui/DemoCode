#include <iostream>
#include <string>

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
    printf("Recv:%s\n", msg.c_str());
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

    LwsClient lct;
    std::shared_ptr<LwsCb> cbPtr = std::make_shared<LwsCb>();
    lct.Register(cbPtr.get());
    lct.Connect(addr, port);
    lct.SendReq(reqStr);

    char ch_input;
    while (true) {
        std::cin >> ch_input;
        if (ch_input == 'q' || ch_input == 'Q') {
            break;
        }
    }

    return 0;
}




