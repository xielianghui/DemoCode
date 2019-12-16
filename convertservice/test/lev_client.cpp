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
#include <string.h>

#include "typedef.pb.h"
#include "msg_carrier.pb.h"
#include "basic_info.pb.h"

int main(int args, char** argv)
{
    if(args < 3)
    {
        puts("Please input ip addr port");
        return -1;
    }
    // addr set
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(std::atoi(argv[2]));
    if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <=0){
       printf("inet_pton error for %s\n",argv[1]);
       return -1;
    }
    // create socket
    int connectfd = socket(AF_INET, SOCK_STREAM, 0);
    if(connectfd < 0){
        puts("Cant't create connect socket");
        return -1;
    }
    // connect
    if(connect(connectfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1){
        puts("connect() failed");
        return -1;
    }
    // block send
    ed::MsgCarrier mc;
    mc.set_req_id(2);
    mc.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_REQ);
    mc.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_HEARTBEAT);
    ed::Heartbeat hb;
    hb.set_connection_id(1);
    mc.set_data(hb.SerializeAsString());
    std::string msg = mc.SerializeAsString();
    if(send(connectfd, msg.data(), msg.size(), 0) < 0){
        printf("Send msg, err msg: %s\n", strerror(errno));
    }
    // Recv
    int ret;
    char buf[2048];
    while((ret = read(connectfd, buf, 2048)) > 0)// nonblock read
    {
        std::string resStr(buf, ret);
        ed::MsgCarrier mc;
        mc.ParseFromString(resStr);
        ed::HeartbeatResp hb;
        hb.ParseFromString(mc.data());
        std::string ds = hb.DebugString();
        printf("Get message:%s\n", ds.c_str());
        memset(buf, 0, 2048);
    }
    if(ret == 0){
        puts("ret == 0, connection close!");
        return -1;
    }
    if(ret < 0 && errno == EAGAIN){
        puts("EAGAIN!");
    }
    close(connectfd);
    return 0;
}