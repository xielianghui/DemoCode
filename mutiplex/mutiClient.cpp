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
    char* buf = new char[1024];
    snprintf(buf, 1024, "hello world");
    if(send(connectfd, buf, strlen(buf), 0) < 0){
        printf("Send msg, err msg: %s\n", strerror(errno));
    }
    getchar();
    close(connectfd);
    return 0;
}
