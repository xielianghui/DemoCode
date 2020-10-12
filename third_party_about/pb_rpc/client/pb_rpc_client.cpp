#include "pb_rpc_client.h"

bool RpcChannelImpl::init(std::string addr, int port)
{
    // addr set
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if(inet_pton(AF_INET, addr.c_str(), &servaddr.sin_addr) <=0){
       printf("inet_pton error for %s\n", addr.c_str());
       return false;
    }
    // create socket
    connfd = socket(AF_INET, SOCK_STREAM, 0);
    if(connfd < 0){
        puts("Cant't create connect socket");
        return false;
    }
    // connect
    if(connect(connfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1){
        puts("connect() failed");
        return false;
    }
    return true;
}

void RpcChannelImpl::CallMethod(const google::protobuf::MethodDescriptor *method,
                                google::protobuf::RpcController *controller,
                                const google::protobuf::Message *request,
                                google::protobuf::Message *response,
                                google::protobuf::Closure *done)
{
    std::string requestStr;
    request->SerializeToString(&requestStr);
    // block send
    if(send(connfd, requestStr.c_str(), requestStr.size(), 0) < 0){
        printf("Send msg failed, err msg: %s\n", strerror(errno));
    }
    else{
        printf("Send msg: %s\n", requestStr.c_str());
    }

    // block recv
    char buf[1024]{0};
    if(read(connfd, buf, 1024) > 0) // block read
    {
        printf("Get message:%s\n", buf);
    }

    response->ParseFromString(std::string(buf));
}