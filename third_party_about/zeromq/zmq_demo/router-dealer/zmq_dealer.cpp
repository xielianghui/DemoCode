#include <zmq.h>
#include <string>
#include <iostream>

int main(int argc, char * argv[])
{
	void * pCtx = NULL;
	void * pSock = NULL;
	const char * pAddr = "tcp://127.0.0.1:9002";

	//创建上下文context 
	if ((pCtx = zmq_ctx_new()) == NULL)
	{
                std::cout<<"socket new() error"<<std::endl;
		return 0;
	}
	//创建socket 
	if ((pSock = zmq_socket(pCtx, ZMQ_DEALER)) == NULL)
	{
		zmq_ctx_destroy(pCtx);
		std::cout<<"socket error"<<std::endl;
		return 0;
	}
        //设置IDENTITY
	std::string connectionName = "haha";
	if (zmq_setsockopt(pSock, ZMQ_IDENTITY, connectionName.c_str(), connectionName.length()))
	{
		zmq_close(pSock);
		zmq_ctx_destroy(pCtx);
		std::cout << "set connectionname error" << std::endl;
		return 0;
	}
	//连接目标
	if (zmq_connect(pSock, pAddr) < 0)
	{
		zmq_close(pSock);
		zmq_ctx_destroy(pCtx);
		std::cout << "connect error" << std::endl;
		return 0;
	}
        std::string firstMsg = "first";
        std::string secondMsg = "second";
        int rc = zmq_send(pSock, firstMsg.data(), firstMsg.size(), ZMQ_SNDMORE | ZMQ_DONTWAIT);
        std::cout<<"first send, msg:"<<firstMsg<<" rc = "<<rc<<std::endl;
	rc = zmq_send(pSock, secondMsg.data(), secondMsg.size(), ZMQ_DONTWAIT);
        std::cout<<"second send,msg:"<<secondMsg<<" rc = "<<rc<<std::endl;

	//zmq_msg_recv() receive
        int i{1};
	zmq_msg_t message;
        zmq_msg_init(&message);
	while(zmq_msg_recv(&message, pSock, 0) >=0)
	{
		std::string strMsg((char *) zmq_msg_data(&message), zmq_msg_size(&message));
                std::cout<<i++<<" frame: "<<strMsg<<std::endl;
                int more = zmq_msg_more(&message);
                zmq_msg_close(&message);
                if (more){
                	zmq_msg_init(&message);
                }
	}
	return 0;
}
