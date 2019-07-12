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

#define MAXEPOLLSIZE 10000
#define MAXLINE 10240

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
    if(args < 2){
        puts("Please input server port!");
        return -1;
    }
    char* cPort = argv[1];
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

    while(1)
    {
        
        int nfds = epoll_wait(epollhdl, events, 1024, 3000);
        if(nfds == -1)
        {
            puts("epoll_wait() failed!");
            continue;
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
    close(listenfd);
    return 0;
}


