#include <unistd.h>
#include <sys/types.h>       /* basic system data types */
#include <sys/socket.h>      /* basic socket definitions */
#include <netinet/in.h>      /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>       /* inet(3) functions */
#include <sys/select.h>      /* select function */
#include <sys/poll.h>        /* poll function */
#include <sys/epoll.h>       /* epoll function */
#include <fcntl.h>           /* nonblocking */
#include <sys/resource.h>    /* setrlimit */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <vector>

int Handle(int connfd)
{
    int ret;
    char buf[1024];
    while((ret = read(connfd, buf, 1024)) > 0)// nonblock read
    {
        printf("Get message:%s\n", buf);
    }
    if(ret == 0){
        puts("ret == 0, connection close!");
        return -1;
    }
    if(ret < 0 && errno == EAGAIN){
        puts("EAGAIN!");
    }
    return 0;
}

int main(int args, char** argv)
{
    if(args < 3){
        puts("1-select 2-poll 3-epoll");
        puts("Usage: ./server 1 2222");
        return -1;
    }
    char* cStyle = argv[1];
    int mpStyle = std::atoi(cStyle);
    char* cPort = argv[2];
    int listenPort = std::atoi(cPort);
    // addr set
    struct sockaddr_in servaddr,cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(listenPort);
    // create listen socket and get it's fd
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1){
        puts("Cant't create listen socket");
        return -1;
    }
    // bind(fd, addr, size)
    if(bind(listenfd,(struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1)
    {
        puts("bind() failed!");
        return -1;
    }
    // listen(fd, max size)
    if(listen(listenfd, 1024) == -1)
    {
        puts("listen() failed!");
        return -1;
    }
    // mutiplexing IO
    if(mpStyle == 1){// select
        fd_set readfds;// read fdset
        struct timeval tv;
        std::vector<int> clientFdVec;
        int maxfd = listenfd;
        int counts = 1;
        while(1)
        {
            // clear fd set
            FD_ZERO(&readfds);
            // add listen fd to set
            FD_SET(listenfd, &readfds);
            // add client fd to set
            for(auto& it : clientFdVec){
                maxfd = maxfd > it ? maxfd : it;
                FD_SET(it, &readfds);
            }
            tv.tv_sec = 3;
            tv.tv_usec =0;
            int nfds = select(maxfd + 1, &readfds, NULL, NULL, &tv); // select will set tv to zero
            printf("This is %d times select,nfds %d\n", counts++, nfds);
            if(nfds < 0){
                puts("select() failed!");
                break;
            }
            if(nfds > 0){
                if(FD_ISSET(listenfd, &readfds)){// some client connect
                    int connfd = accept(listenfd, (struct sockaddr *)&cliaddr,&socklen);// accept create a new socket fd to process one connection
                    if(connfd < 0){
                        puts("accept() failed!");
                        continue;
                    }
                    printf("Accept connection from %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                    // set nonblock
                    if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
                        puts("fcntl() set non block failed!");
                    }
                    clientFdVec.emplace_back(connfd);
                }
                else{// maybe some connect fd receive msg
                    for (auto it = clientFdVec.begin(); it != clientFdVec.end();) 
                    {
                        if(FD_ISSET(*it, &readfds)){
                            if(Handle(*it) < 0){ // connection close
                                it = clientFdVec.erase(it);
                                continue;
                            }
                            ++it;
                        }
                    }
                } 
            }
         }
    }else if(mpStyle == 2){
        struct  pollfd *pfds = (struct pollfd*)malloc(1024*sizeof(struct pollfd));
        int fdcounts = 0;
        // add listen fd to pollfd array
        pfds[0].fd = listenfd;
        pfds[0].events = POLLIN;
        ++fdcounts;
        int counts = 1;
        while(1)
        {
            int ret = poll(pfds, fdcounts, 3000);// timeout: 3000 ms
            printf("This is %d times select,nfds %d\n", counts++, ret);
            if(ret < 0){
                puts("select() failed!");
                break;
            }// ret == 0 timeout
            if(ret > 0){
                if(pfds[0].revents & pfds[0].events){// listen socket 
                    int connfd = accept(listenfd, (struct sockaddr *)&cliaddr,&socklen);// accept create a new socket fd to process one connection
                    if(connfd < 0){
                        puts("accept() failed!");
                        continue;
                    }
                    printf("Accept connection from %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                    // set nonblock
                    if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
                        puts("fcntl() set non block failed!");
                    }
                    // add connect fd to pfds
                    pfds[fdcounts].fd = connfd;
                    pfds[fdcounts].events = POLLIN;
                    ++fdcounts;
                }
                else{// another connect socket
                    for(int i = 1; i < fdcounts;)// pfds[0] is listen socket fd
                    {
                        if(pfds[i].revents & pfds[i].events){// connect socket
                            if(Handle(pfds[i].fd) < 0){ // connection close
                                //del close fd
                                for(int j = i; j + 1 < fdcounts; ++j)
                                {
                                    pfds[j] = pfds[j + 1];
                                }
                                --fdcounts;
                                continue;
                            }
                        }
                        ++i;
                    }
                }
            }
        }
    }else{// epoll
        // create epoll handl and insert listenfd into it
        int epollhdl = epoll_create(1024);// max size
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = listenfd;
        if (epoll_ctl(epollhdl, EPOLL_CTL_ADD, listenfd, &ev) < 0)
        {
            puts("epoll_ctl() failed!");
            return -1;
        }
        // epoll waite
        struct epoll_event events[1024];
        int counts = 1;
        while(1)
        {
            int nfds = epoll_wait(epollhdl, events, 1024, 3000);
            printf("This is %d times epoll,nfds %d.\n", counts++, nfds);
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
                        puts("epoll_ctl() connect fd failed!");
                        return -1;
                    }
                    continue;
                 }else{ // not listen fd, must be connect fd, handle message
                    if(Handle(events[i].data.fd) < 0){
                        epoll_ctl(epollhdl, EPOLL_CTL_DEL, events[i].data.fd,&ev);
                    }
                 }
              }
          }
          close(epollhdl);
    }
    close(listenfd);
    return 0;
}


