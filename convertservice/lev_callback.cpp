#include "convert_service.h"
#include "lev_callback.h"

LevSrvCbHdl::LevSrvCbHdl(ConvertService* mainService):
    m_mainService(mainService){}

void LevSrvCbHdl::OnAccept(CContext *conn, const char *peer, uint32_t len)
{
    m_mainService->OnLevAccept(conn, peer, len);
}

void LevSrvCbHdl::OnReadDone(CContext *conn, evbuffer *&&recv_data)
{
    m_mainService->OnLevReadDone(conn, std::move(recv_data));
}

void LevSrvCbHdl::OnDisConnect(CContext *conn)
{
    m_mainService->OnLevDisConnect(conn);
}

void LevSrvCbHdl::OnError(CContext* conn, int err, const char* err_msg)
{
    m_mainService->OnLevError(conn, err, err_msg);
}