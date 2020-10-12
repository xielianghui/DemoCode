#include "server/pb_rpc_server.h"

int main(int argc, char* argv[])
{
    RpcServer server;
    EchoServerImpl echoService;
    server.RegisterService(&echoService);

    server.Start(5000);
    return 0;
}