#pragma once

#include "lwsclient/lws_client.h"
#include "lwsclient/lws_cb_interface.h"

class ConvertService;
class LwsCltCbHdl : public LwsCbInterface
{
public:
    LwsCltCbHdl() = delete;
    ~LwsCltCbHdl() = default;
    LwsCltCbHdl(ConvertService* mainService);
public:
    void OnConnect() override;
    void OnRecv(std::string&& msg) override;
private:
    ConvertService* m_mainService;
};
