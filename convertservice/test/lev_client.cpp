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

#include <iostream>

#include "typedef.pb.h"
#include "msg_carrier.pb.h"
#include "query.pb.h"
#include "push.pb.h"
#include "../common_hdr.h"

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
    // pack req
    ed::MsgCarrier mc;
    mc.set_req_id(2);
    mc.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_REQ);
    mc.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_SUB_QUOTES);
    ed::SubQuotes sub;
    sub.set_market_id(2002);
    sub.set_ins_id("00700");
    mc.set_data(sub.SerializeAsString());
    std::string RawMsg = mc.SerializeAsString();
    // add pkg hdl
    std::string msg;
    msg.resize(RawMsg.size() + eddid::COMMON_HDR_LEN);
    eddid::ST_COMMONHDR* pHdr = (eddid::ST_COMMONHDR*)msg.data();
    new (pHdr) eddid::ST_COMMONHDR(0, RawMsg.size());
    memcpy(msg.data() + eddid::COMMON_HDR_LEN, RawMsg.data(), RawMsg.size());
    if(send(connectfd, msg.data(), msg.size(), 0) < 0){
        printf("Send msg, err msg: %s\n", strerror(errno));
    }
    // Recv
    int ret;
    int size = 5 * 1024 * 1024;
    char buf[size];
    while((ret = read(connectfd, buf, size)) > 0)// nonblock read
    {
        int pos = 0;
        while(pos < ret)
        {
            char hdl[eddid::COMMON_HDR_LEN];
            memcpy(hdl, buf + pos, eddid::COMMON_HDR_LEN);
            eddid::ST_COMMONHDR* pHdl = (eddid::ST_COMMONHDR*) hdl;
            int dataLen = pHdl->data_len_;
            std::string resStr(buf + pos + eddid::COMMON_HDR_LEN, dataLen);
            ed::MsgCarrier mcRes;
            if (!mcRes.ParseFromString(resStr)){
                printf("mc parse failed, msg:%s\n", resStr.c_str());
                break;
            }

            if (mcRes.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_SUB_QUOTES){
                ed::SubQuotesResp subRes;
                if (!subRes.ParseFromString(mcRes.data())){
                    printf("sub res parse failed, msg:%s\n", mcRes.data());
                    return -1;
                }
                std::cout<<subRes.DebugString()<<std::endl;
            }
            else{
                ed::QuoteInfo quoteInfo;
                if (!quoteInfo.ParseFromString(mcRes.data()))
                {
                    printf("quote info parse failed, msg:%s\n", mcRes.data());
                    return -1;
                }
                std::cout<<quoteInfo.DebugString()<<std::endl;
            }
            pos += (eddid::COMMON_HDR_LEN + dataLen);
        }
        memset(buf, 0, size);
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
