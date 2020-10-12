#pragma once
#include <unistd.h>
#include <sys/types.h>       /* basic system data types */
#include <sys/socket.h>      /* basic socket definitions */
#include <netinet/in.h>      /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>       /* inet(3) functions */

#include <errno.h>

#include <string>


#include "../proto/echo_service.pb.h"

class RpcChannelImpl : public google::protobuf::RpcChannel
{
public:
    RpcChannelImpl() = default;
    ~RpcChannelImpl() = default;
public:
    bool init(std::string addr, int port);
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;
private:
    int connfd;
};

class RpcControllerImpl : public google::protobuf::RpcController
{
public:
    RpcControllerImpl() { Reset(); }
    virtual ~RpcControllerImpl() {}

    virtual void Reset()
    {
        isFailed = false;
        errCode = "";
    }
    virtual bool Failed() const { return isFailed; }
    virtual void SetFailed(const std::string &reason)
    {
        isFailed = true;
        errCode = reason;
    }
    virtual std::string ErrorText() const { return errCode; }
    virtual void StartCancel(){};
    virtual bool IsCanceled() const { return false; };
    // 当RPC调用被取消了会被调用
    virtual void NotifyOnCancel(::google::protobuf::Closure * /* callback */){};

private:
    bool isFailed;
    std::string errCode;
};