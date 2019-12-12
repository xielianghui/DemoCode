#include "convert_service.h"
#include "lws_callback.h"

LwsCltCbHdl::LwsCltCbHdl(ConvertService* mainService):
    m_mainService(mainService){}

void LwsCltCbHdl::OnConnect()
{
    m_mainService->OnLwsConnect();
}

void LwsCltCbHdl::OnRecv(std::string&& msg)
{
    m_mainService->OnLwsRecvMsg(std::move(msg));
}