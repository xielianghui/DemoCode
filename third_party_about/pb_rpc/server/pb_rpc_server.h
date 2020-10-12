#pragma once
#include <unistd.h>
#include <sys/types.h>       /* basic system data types */
#include <sys/socket.h>      /* basic socket definitions */
#include <netinet/in.h>      /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>       /* inet(3) functions */
#include <sys/epoll.h>       /* epoll function */
#include <fcntl.h>           /* nonblocking */
#include <sys/resource.h>    /* setrlimit */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include <vector>
#include <iostream>
#include <string>
#include <unordered_map>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "../proto/echo_service.pb.h"

struct ServiceInfo
{
    ::google::protobuf::Service* service;
    std::vector<::google::protobuf::MethodDescriptor*> methodVec;
};

class EchoServerImpl : public pbrpc::EchoService
{
public:
    EchoServerImpl() = default;
    ~EchoServerImpl() = default;

private:
    void Echo(google::protobuf::RpcController *controller,
              const pbrpc::EchoRequest *request,
              pbrpc::EchoResponse *response,
              google::protobuf::Closure *done) override;
};

//----------------------------rpc server--------------------------------------
class RpcServer
{
public:
    RpcServer() = default;
    ~RpcServer() = default;

public:
    void Start(int port);
    bool RegisterService(::google::protobuf::Service* service);
    void SendBack(::google::protobuf::Message* response, int fd);
private:
    std::unordered_map<int, ServiceInfo> serviceMap;
};