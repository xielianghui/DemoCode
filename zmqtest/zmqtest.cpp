//包含zmq的头文件 
#include <zmq.h>
#include <string>
#include "stdio.h"
#include <iostream>

int main(int argc, char * argv[])
{
	void * pCtx = NULL;
	void * pSock = NULL;
	//使用tcp协议进行通信，需要连接的目标机器IP地址为192.168.1.2
	//通信使用的网络端口 为7766 
	const char * pAddr = "tcp://127.0.0.1:9002";

	//创建context 
	if ((pCtx = zmq_ctx_new()) == NULL)
	{
		return 0;
	}
	//创建socket 
	if ((pSock = zmq_socket(pCtx, ZMQ_DEALER)) == NULL)
	{
		zmq_ctx_destroy(pCtx);
		std::cout<<"socket error"<<std::endl;
		return 0;
	}
	std::string connectionName = "haha";
	if (zmq_setsockopt(pSock, ZMQ_IDENTITY, connectionName.c_str(), connectionName.length())
	{
		zmq_close(pSock);
		zmq_ctx_destroy(pCtx);
		std::cout << "connectionname error" << std::endl;
		return 0;
	}
	//连接目标IP192.168.1.2，端口7766 
	if (zmq_connect(pSock, pAddr) < 0)
	{
		zmq_close(pSock);
		zmq_ctx_destroy(pCtx);
		std::cout << "connect error" << std::endl;
		return 0;
	}
	int i = 0;
	char snedMsg[1024] = { 0 };
	snprintf(snedMsg, sizeof(snedMsg), "hello world : %3d", i++);
	printf("Enter to send...\n");
	if (zmq_send(pSock, snedMsg, sizeof(snedMsg), 0) < 0)
	{
		fprintf(stderr, "send message faild\n");
	}
	printf("send message : [%s] succeed\n", snedMsg);

	char revMsg[1024] = { 0 };
	printf("waitting...\n");
	errno = 0;
	//循环等待接收到来的消息，当超过5秒没有接到消息时，
	//zmq_recv函数返回错误信息 ，并使用zmq_strerror函数进行错误定位 
	if (zmq_recv(pSock, revMsg, sizeof(revMsg), 0) < 0)
	{
		printf("error = %s\n", zmq_strerror(errno));
	}
	printf("received message : %s\n", revMsg);


	return 0;
}