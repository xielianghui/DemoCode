#include "pb_rpc_server.h"

int RecvData(int connfd, std::string& msg)
{
    msg.clear();
    int ret = 0;
    char buf[1024]{0};
    while((ret = read(connfd, buf, 1024)) > 0)// nonblock read
    {
        msg += std::string(buf);
        memset(buf, 0, 1024);
    }
    if(ret == 0){
    }
    if(ret < 0 && errno == EAGAIN){
        puts("EAGAIN!");
    }
    return ret;
}

void EchoServerImpl::Echo(google::protobuf::RpcController *controller,
                          const ::pbrpc::EchoRequest *request,
                          pbrpc::EchoResponse *response,
                          google::protobuf::Closure *done)
{
    std::cout<< "Recv client message: " << request->msg() << std::endl;
    response->set_msg(request->msg());
    done->Run();
}

//----------------------------rpc server--------------------------------------
void RpcServer::Start(int port)
{
    // addr set
    struct sockaddr_in servaddr,cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    // create listen socket and get it's fd
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1){
        puts("Cant't create listen socket");
        return;
    }
    // set reuseaddr
    int one = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0){
        close(listenfd);
        return;
    }
    // bind(fd, addr, size)
    if(bind(listenfd,(struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1)
    {
        puts("bind() failed!");
        close(listenfd);
        return;
    }
    // listen(fd, max size)
    if(listen(listenfd, 1024) == -1)
    {
        puts("listen() failed!");
        close(listenfd);
        return;
    }
    // mutiplexing IO
    // create epoll handl and insert listenfd into it
    int epollhdl = epoll_create(1024);// max size
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenfd;
    if (epoll_ctl(epollhdl, EPOLL_CTL_ADD, listenfd, &ev) < 0)
    {
        puts("epoll_ctl() failed!");
        close(epollhdl);
        close(listenfd);
        return;
    }
    // epoll waite
    struct epoll_event events[1024];
    int counts = 1;
    while(1)
    {
        int nfds = epoll_wait(epollhdl, events, 1024, 3000);
        // printf("This is %d times epoll,nfds %d.\n", counts++, nfds);
        if(nfds == -1){
            puts("epoll_wait() failed!");
            break;
        }
        for(int i = 0; i < nfds; ++i)
        {
            if(events[i].data.fd == listenfd) // some client connect
            {
                int connfd = accept(listenfd, (struct sockaddr *)&cliaddr,&socklen);// accept create a new socket fd to process one connection
                if(connfd < 0){
                    puts("accept() failed!");
                    continue;
                }
                printf("Accept connection from %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                // EPOLLET need set nonblock
                if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
                    puts("fcntl() set non block failed!");
                }
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connfd;
                // add connection fd into epoll
                if (epoll_ctl(epollhdl, EPOLL_CTL_ADD, connfd, &ev) < 0)
                {
                    puts("epoll_ctl() add connect fd failed!");
                    continue;
                }
                continue;
            }else{ 
                // not listen fd, must be connect fd, handle message
                std::string msg;
                int connfd = events[i].data.fd;
                if(RecvData(connfd, msg) == 0){
                    epoll_ctl(epollhdl, EPOLL_CTL_DEL, connfd,&ev);
                    close(connfd);
                }
                else{
                    auto service = serviceMap[0].service;
                    auto svrDesc = serviceMap[0].methodVec[0];
                    auto recv_msg = service->GetRequestPrototype(svrDesc).New();
                    auto resp_msg = service->GetResponsePrototype(svrDesc).New();
                    recv_msg->ParseFromString(msg);
                    auto done = google::protobuf::NewCallback(
                        this, &RpcServer::SendBack, resp_msg, connfd);
                    service->CallMethod(svrDesc, nullptr, recv_msg, resp_msg, done);
                }
            }
        }
    }
    close(epollhdl);
    close(listenfd);
}

bool RpcServer::RegisterService(::google::protobuf::Service* service)
{
    ServiceInfo svrInfo;
    svrInfo.service = service;
    const ::google::protobuf::ServiceDescriptor *svrDesc = service->GetDescriptor();
    for (int i = 0; i < svrDesc->method_count(); ++i)
    {
        svrInfo.methodVec.emplace_back(const_cast<google::protobuf::MethodDescriptor*>(svrDesc->method(i)));
    }
    serviceMap[0] = svrInfo;
    return true;
}

void RpcServer::SendBack(::google::protobuf::Message* response, int fd)
{
    std::string responseStr;
    response->SerializeToString(&responseStr);
    if(send(fd, responseStr.c_str(), responseStr.size(), 0) < 0){
        printf("Send msg failed, err msg: %s\n", strerror(errno));
    }
    else{
        printf("Send msg: %s\n", responseStr.c_str());
    }
}