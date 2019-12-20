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

//g++ -lpthread -std=c++17 ../lev_client.cpp ../../proto/gen_cpp/*.cc -g -o levclient -I ../../proto/gen_cpp -lprotobuf
int main(int args, char** argv)
{
    if(args < 4)
    {
        puts("Please input ip addr port req_type");
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
    int type = std::atoi(argv[3]); // type: 0-获取所有合约信息  1-查询K线   2-订阅行情 3-订阅逐笔
    if(type == 0){
        // pack req
        ed::MsgCarrier mc;
        mc.set_req_id(1);
        mc.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_REQ);
        mc.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_ALL_INS_INFO);
        ed::QueryAllInsInfo query;
        mc.set_data(query.SerializeAsString());
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
        std::string lastMsg;
        char buf[size];
        while((ret = read(connectfd, buf, size)) > 0)// nonblock read
        {
            lastMsg += std::string(buf, ret);
            while(true)
            {
                char hdl[eddid::COMMON_HDR_LEN];
                memcpy(hdl, lastMsg.data(), eddid::COMMON_HDR_LEN);
                eddid::ST_COMMONHDR* pHdl = (eddid::ST_COMMONHDR*) hdl;
                int dataLen = pHdl->data_len_;
                if(dataLen + eddid::COMMON_HDR_LEN > lastMsg.size()){
                    break;
                }
                else{
                    std::string resStr(lastMsg.data() + eddid::COMMON_HDR_LEN, dataLen);
                    ed::MsgCarrier mcRes;
                    if (!mcRes.ParseFromString(resStr)){
                        printf("mc parse failed, msg:%s\n", resStr.c_str());
                        break;
                    }
                    if (mcRes.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_ALL_INS_INFO){
                        ed::QueryAllInsInfoResp queryRes;
                        if (!queryRes.ParseFromString(mcRes.data())){
                            printf("query ins info res parse failed, msg:%s\n", mcRes.data());
                            return -1;
                        }
                        if (queryRes.error_code() == -1){
                            std::cout << "Get all ins info falied, error msg:" << queryRes.error_msg() << std::endl;
                        }
                        else{
                            //std::cout << "Get all ins info, info size:" << queryRes.data_size() << ", first info:" << queryRes.data(0).DebugString() << std::endl;
                            for(int i = 0; i < queryRes.data_size(); ++i){
                                std::cout<<"Exchange:["<<queryRes.data(i).exchange()<<"], Code:["<<queryRes.data(i).ins_id()<<"]"<<std::endl;
                            }
                        }
                    }
                    lastMsg = lastMsg.substr(eddid::COMMON_HDR_LEN + dataLen);
                    if(lastMsg.size() < eddid::COMMON_HDR_LEN) break;
                }
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
    }
    else if(type == 1){
        // pack req
        ed::MsgCarrier mc;
        mc.set_req_id(1);
        mc.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_REQ);
        mc.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_K_LINE);
        ed::QueryKLine query;
        query.set_type(ed::TypeDef_KLineType::TypeDef_KLineType_KLT_MINUTE);
        query.set_exchange(ed::TypeDef_Exchange::TypeDef_Exchange_SEHK);
        query.set_ins_id("00700");
        query.set_start_time("2019-12-20 01:30:00");
        query.set_end_time("2019-12-20 01:35:00");
        mc.set_data(query.SerializeAsString());
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
        std::string lastMsg;
        char buf[size];
        while((ret = read(connectfd, buf, size)) > 0)// nonblock read
        {
            lastMsg += std::string(buf, ret);
            while(true)
            {
                char hdl[eddid::COMMON_HDR_LEN];
                memcpy(hdl, lastMsg.data(), eddid::COMMON_HDR_LEN);
                eddid::ST_COMMONHDR* pHdl = (eddid::ST_COMMONHDR*) hdl;
                int dataLen = pHdl->data_len_;
                if(dataLen + eddid::COMMON_HDR_LEN > lastMsg.size()){
                    break;
                }
                else{
                    std::string resStr(lastMsg.data() + eddid::COMMON_HDR_LEN, dataLen);
                    ed::MsgCarrier mcRes;
                    if (!mcRes.ParseFromString(resStr)){
                        printf("mc parse failed, msg:%s\n", resStr.c_str());
                        break;
                    }
                    if (mcRes.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_K_LINE){
                        ed::QueryKLineResp queryRes;
                        if (!queryRes.ParseFromString(mcRes.data())){
                            printf("query K line res parse failed, msg:%s\n", mcRes.data());
                            return -1;
                        }
                        if (queryRes.error_code() != 0){
                            std::cout << "Get all k line info falied, error msg:" << queryRes.error_msg() << std::endl;
                        }
                        else{
                            std::cout <<queryRes.DebugString() << std::endl;
                            std::cout << "Get all K line info, info size:" << queryRes.data_size()<<std::endl;
                        }
                    }
                    lastMsg = lastMsg.substr(eddid::COMMON_HDR_LEN + dataLen);
                    if(lastMsg.size() < eddid::COMMON_HDR_LEN) break;
                }
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
    }
    else if(type == 2 || type == 3){// 订阅静态/快照/盘口 或者 逐笔
        // input exchange code
        int exchange = 0;
        std::string code;
        std::cout<<"Input exchange code:"<<std::endl;
        std::cin>>exchange>>code;
        // pack req
        ed::MsgCarrier mc;
        mc.set_req_id(1);
        mc.set_msg_type(ed::TypeDef_MsgType::TypeDef_MsgType_REQ);
        mc.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_SUB_QUOTES);
        ed::SubQuotes sub;
        if(type == 2){
            sub.set_type(ed::TypeDef_SubType::TypeDef_SubType_QUOTES);
        }
        else{
            sub.set_type(ed::TypeDef_SubType::TypeDef_SubType_TICK);
        }
        
        sub.set_exchange((ed::TypeDef_Exchange)exchange);
        sub.set_ins_id(code);
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
                    if(type == 2){
                        ed::QuoteInfo quoteInfo;
                        if (!quoteInfo.ParseFromString(mcRes.data()))
                        {
                            printf("quote info parse failed, msg:%s\n", mcRes.data());
                            return -1;
                        }
                        std::cout << quoteInfo.DebugString() << std::endl;
                    }
                    else{
                        ed::TickInfo tickInfo;
                        if (!tickInfo.ParseFromString(mcRes.data()))
                        {
                            printf("quote info parse failed, msg:%s\n", mcRes.data());
                            return -1;
                        }
                        std::cout << tickInfo.DebugString() << std::endl;
                    }
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
    }
    close(connectfd);
    return 0;
}
