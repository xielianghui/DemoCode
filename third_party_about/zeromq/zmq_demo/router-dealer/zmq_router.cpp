/*
Zmq dealer发送消息给Router时，如果设置了IDENTITY，那么Router收到的第一帧就是这个标识
如果没设置，那么就会生成一个随机的标识。
后面Router需要用这个标识返回消息给Dealer， dealer并不会收到该标识帧！
*/
#include <zmq.h>
#include <string>
#include <iostream>

int main(int argc, char * argv[])
{
	void * pCtx = NULL;
	void * pSock = NULL;
	const char * pAddr = "tcp://*:9002";

	//创建上下文context 
	if ((pCtx = zmq_ctx_new()) == NULL)
	{
                std::cout<<"socket new() error"<<std::endl;
		return 0;
	}
	//创建socket 
	if ((pSock = zmq_socket(pCtx, ZMQ_ROUTER)) == NULL)
	{
		zmq_ctx_destroy(pCtx);
		std::cout<<"socket error"<<std::endl;
		return 0;
	}
        //设置IDENTITY
	std::string connectionName = "heihei";
	if (zmq_setsockopt(pSock, ZMQ_IDENTITY, connectionName.c_str(), connectionName.length()))
	{
		zmq_close(pSock);
		zmq_ctx_destroy(pCtx);
		std::cout << "set connectionname error" << std::endl;
		return 0;
	}
	//bind
	if (zmq_bind(pSock, pAddr) < 0)
	{
		zmq_close(pSock);
		zmq_ctx_destroy(pCtx);
		std::cout << "bind error" << std::endl;
		return 0;
	}
	//zmq_msg_recv() receive
        int i{1};
	zmq_msg_t message;
        zmq_msg_init(&message);
	while(zmq_msg_recv(&message, pSock, 0) >=0)
	{
		std::string strMsg((char *) zmq_msg_data(&message), zmq_msg_size(&message));
                static std::string dealerName = strMsg;
                std::cout<<i++<<" frame: "<<strMsg<<std::endl;
                int more = zmq_msg_more(&message);
                zmq_msg_close(&message);
                if (more){
                	zmq_msg_init(&message);
                }else{
		    std::string firstMsg = "first";
	            std::string secondMsg = "second";
                    int rc = zmq_send(pSock, dealerName.data(), dealerName.size(), ZMQ_SNDMORE | ZMQ_DONTWAIT);
                    std::cout<<"first send, msg:"<<dealerName<<" rc = "<<rc<<std::endl;
 	    	    rc = zmq_send(pSock, firstMsg.data(), firstMsg.size(), ZMQ_SNDMORE | ZMQ_DONTWAIT);
 		    std::cout<<"second send, msg:"<<firstMsg<<" rc = "<<rc<<std::endl;
 	            rc = zmq_send(pSock, secondMsg.data(), secondMsg.size(), ZMQ_DONTWAIT);
 		    std::cout<<"third send, msg:"<<secondMsg<<" rc = "<<rc<<std::endl;
               }
	}
        
	return 0;
}
