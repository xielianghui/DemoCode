#include "client/pb_rpc_client.h"

int main(int argc, char* argv[])
{
    RpcChannelImpl channel;
    if(!channel.init(std::string(argv[1]), std::atoi(argv[2]))){
        std::cout<< "channel init failed" << std::endl;
        return -1;
    }
    pbrpc::EchoService_Stub stub(&channel);

    RpcControllerImpl controller;
    pbrpc::EchoRequest req;
    pbrpc::EchoResponse res;
    req.set_msg("Hello World");
    stub.Echo(&controller, &req, &res, nullptr);
    if(controller.Failed()){
        std::cout<< "Request failed, error info: " <<  controller.ErrorText() << std::endl;
    }
    else{
        std::cout<< "Recv res: " << res.msg() << std::endl;
    }
    return 0;
}